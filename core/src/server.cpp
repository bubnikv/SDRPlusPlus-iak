#include "server.h"
#include "core.h"
#include <utils/flog.h>
#include <version.h>
#include <config.h>
#include <filesystem>
#include <dsp/types.h>
#include <signal_path/signal_path.h>
#include <gui/smgui.h>
#include <utils/optionlist.h>
#include <utils/proto/pbkdf2_sha256.h>
#include "dsp/compression/sample_stream_compressor.h"
#include "dsp/sink/handler_sink.h"
#include <zstd.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <mutex>
#include <memory>
#include <vector>
#include <cstring>
#include <chrono>

namespace server {
    dsp::stream<dsp::complex_t> dummyInput;
    dsp::compression::SampleStreamCompressor comp;
    dsp::sink::Handler<uint8_t> hnd;

    // Baseband TX buffer. Only ever touched by the single DSP/baseband thread,
    // so it stays a plain global (separate from the per-session send buffer).
    uint8_t* bbuf = NULL;
    PacketHeader* bb_pkt_hdr = NULL;
    uint8_t* bb_pkt_data = NULL;

    SmGui::DrawListElem dummyElem;

    ZSTD_CCtx* cctx;

    net::Listener listener;

    OptionList<std::string, std::string> sourceList;
    int sourceId = 0;
    bool running = false;
    // Written by the command handler, read by the baseband DSP thread.
    std::atomic<bool> compression{false};

    // Cached server-side state pushed to the client, guarded by its own leaf
    // mutex (never held while taking another lock). setInputSampleRate() and
    // setTuningLimits() only record here — they are called from source-module
    // code both while the render path already holds controlMtx and from
    // module worker threads, so they must not touch controlMtx themselves.
    // flushPushedStateLocked() sends dirty state to the client, and the
    // greeting re-sends it all, so a later client still gets current values.
    std::mutex pushedStateMtx;
    double sampleRate = 1000000.0;
    bool tuningLimitEnabled = false;
    double tuningLimitMin = 0.0;
    double tuningLimitMax = 0.0;
    bool samplerateDirty = false;
    bool tuningLimitsDirty = false;
    bool authRequired = false;
    std::array<uint8_t, SERVER_AUTH_RESPONSE_SIZE> authKey{};

    using SteadyClock = std::chrono::steady_clock;
    static constexpr std::chrono::milliseconds HANDSHAKE_TIMEOUT{10000};
    static constexpr std::chrono::milliseconds HEARTBEAT_INTERVAL{5000};
    static constexpr std::chrono::milliseconds HEARTBEAT_TIMEOUT{15000};
    // Online password guessing brake: disconnect after a bad response and
    // keep a short global cooldown so reconnects do not reset the guess rate.
    // Offline brute force of a captured challenge/response pair is only
    // slowed by the PBKDF2 iteration count.
    static constexpr std::chrono::milliseconds AUTH_FAIL_COOLDOWN{1000};
    // Guarded by controlMtx, like the rest of the auth handling.
    SteadyClock::time_point authCooldownUntil{};

    // A connected client and everything scoped to that connection: the socket
    // plus its own receive and send buffers. Making the buffers per-session is
    // what keeps an old, still-draining callback from corrupting the buffers of
    // a freshly accepted connection, and routing replies through the session
    // (rather than a global handle) keeps them going to the origin socket.
    struct ClientSession {
        // Buffers are declared before `conn` on purpose: members are destroyed
        // in reverse order, so `conn` (destroyed first) joins its read worker
        // while these buffers — which a callback still running on that worker
        // may be reading/writing — are still alive.
        std::vector<uint8_t> rstore;
        std::vector<uint8_t> sstore;

        PacketHeader* r_pkt_hdr;
        uint8_t* r_pkt_data;
        CommandHeader* r_cmd_hdr;
        uint8_t* r_cmd_data;

        PacketHeader* s_pkt_hdr;
        uint8_t* s_pkt_data;
        CommandHeader* s_cmd_hdr;
        uint8_t* s_cmd_data;

        std::shared_ptr<net::ConnClass> conn;
        std::atomic<bool> protocolReady{false};
        std::atomic<bool> authenticated{false};
        std::atomic<bool> protocolRejected{false};
        std::atomic<uint32_t> peerCapabilities{0};
        SteadyClock::time_point connectedAt = SteadyClock::now();
        uint32_t heartbeatSeq = 0;
        bool heartbeatAwaitingAck = false;
        SteadyClock::time_point heartbeatLastSend = SteadyClock::now();
        std::array<uint8_t, SERVER_AUTH_CHALLENGE_SIZE> authChallenge{};

        ClientSession(net::Conn c)
            : rstore(SERVER_MAX_PACKET_SIZE), sstore(SERVER_MAX_PACKET_SIZE), conn(std::move(c)) {
            uint8_t* rbuf = rstore.data();
            r_pkt_hdr = (PacketHeader*)rbuf;
            r_pkt_data = &rbuf[sizeof(PacketHeader)];
            r_cmd_hdr = (CommandHeader*)r_pkt_data;
            r_cmd_data = &rbuf[sizeof(PacketHeader) + sizeof(CommandHeader)];

            uint8_t* sbuf = sstore.data();
            s_pkt_hdr = (PacketHeader*)sbuf;
            s_pkt_data = &sbuf[sizeof(PacketHeader)];
            s_cmd_hdr = (CommandHeader*)s_pkt_data;
            s_cmd_data = &sbuf[sizeof(PacketHeader) + sizeof(CommandHeader)];
        }

        bool isOpen() { return conn && conn->isOpen(); }

        // Compose-and-send into this session's send buffer. The caller must
        // hold controlMtx (it serializes composition into sstore between the
        // threads that send: the session's read worker and the main loop's
        // heartbeat/state flush).
        bool sendPacket(PacketType type, int len) {
            s_pkt_hdr->type = type;
            s_pkt_hdr->size = sizeof(PacketHeader) + len;
            return conn && conn->write(s_pkt_hdr->size, sstore.data());
        }

        bool sendCommand(Command cmd, int len) {
            s_cmd_hdr->cmd = cmd;
            return sendPacket(PACKET_TYPE_COMMAND, sizeof(CommandHeader) + len);
        }

        bool sendCommandAck(Command cmd, int len) {
            s_cmd_hdr->cmd = cmd;
            return sendPacket(PACKET_TYPE_COMMAND_ACK, sizeof(CommandHeader) + len);
        }

        bool sendError(Error err) {
            s_pkt_data[0] = err;
            return sendPacket(PACKET_TYPE_ERROR, 1);
        }

        bool sendSampleRate(double samplerate) {
            // memcpy: s_cmd_data is not 8-byte aligned (header offsets).
            memcpy(s_cmd_data, &samplerate, sizeof(double));
            return sendCommand(COMMAND_SET_SAMPLERATE, sizeof(double));
        }

        bool sendTuningLimits(bool enabled, double minFreq, double maxFreq) {
            s_cmd_data[0] = enabled;
            // memcpy: s_cmd_data is not 8-byte aligned (header offsets).
            memcpy(&s_cmd_data[1], &minFreq, sizeof(double));
            memcpy(&s_cmd_data[1 + sizeof(double)], &maxFreq, sizeof(double));
            return sendCommand(COMMAND_SET_TUNING_LIMITS, 1 + 2 * sizeof(double));
        }

        bool sendHeartbeat(uint32_t seq) {
            memcpy(s_cmd_data, &seq, sizeof(seq));
            return sendCommand(COMMAND_HEARTBEAT, sizeof(seq));
        }

        bool sendHelloAck() {
            HelloPayload hello = makeHelloPayload(SERVER_PROTOCOL_CAP_HEARTBEAT | (authRequired ? SERVER_PROTOCOL_CAP_AUTH : 0));
            memcpy(s_cmd_data, &hello, sizeof(hello));
            return sendCommandAck(COMMAND_HELLO, sizeof(hello));
        }

        bool sendAuthChallenge() {
            crypto::randomBytes(authChallenge.data(), authChallenge.size());
            memcpy(s_cmd_data, authChallenge.data(), authChallenge.size());
            return sendCommand(COMMAND_AUTH_CHALLENGE, (int)authChallenge.size());
        }
    };

    // The client sessions and the mutex guarding the handles themselves.
    // Every user snapshots the shared_ptr under sessionMtx so a reconnect on
    // the accept thread can't free a connection out from under a send/receive
    // in flight. A newly accepted connection starts as pendingSession and is
    // only promoted to currentSession — the one that receives baseband and
    // state pushes — after it completes the hello (and authentication), so an
    // unauthenticated TCP connect can never displace an authenticated client.
    std::shared_ptr<ClientSession> currentSession;
    std::shared_ptr<ClientSession> pendingSession;
    // Sessions displaced by a promotion, kept alive until heartbeatTick
    // destroys them: promotion runs on the new session's read worker while
    // holding controlMtx, and destroying a session joins its read worker,
    // which may itself be blocked on controlMtx — a deadlock if done in place.
    std::vector<std::shared_ptr<ClientSession>> retiredSessions;
    std::mutex sessionMtx;

    static std::shared_ptr<ClientSession> getSession() {
        std::lock_guard lck(sessionMtx);
        return currentSession;
    }

    // Whether the session is still one of the two live slots (current or
    // pending), i.e. not superseded by a newer connection.
    static bool isActiveSession(ClientSession* s) {
        std::lock_guard lck(sessionMtx);
        return currentSession.get() == s || pendingSession.get() == s;
    }

    // Serializes the whole control plane: the global SmGui rendering
    // (renderUI/drawMenu and the source menus they invoke), the `running`
    // flag, the auth state, and all command/state sends into a session's
    // send buffer. Acquired exactly once at each entry point (_clientHandler,
    // _packetHandler dispatch, handleHello, commandHandler,
    // commandAckHandler, heartbeatTick); everything below runs in *Locked
    // helpers that require it but never take it. Source-module code invoked
    // from the render path never loops back into this lock: the state-push
    // helpers it calls (setInputSampleRate/setTuningLimits) only record
    // under pushedStateMtx. The baseband path deliberately does NOT take
    // this lock: it uses its own bbuf and only snapshots the session.
    std::mutex controlMtx;

    // Internal handlers/helpers (never called from outside this file).
    static void _clientHandler(net::Conn conn, void* ctx);
    static void _packetHandler(int count, uint8_t* buf, void* ctx);
    static void _testServerHandler(uint8_t* data, int count, void* ctx);
    static bool handleHello(ClientSession* s, Command cmd, uint8_t* data, int len);
    static bool activateSessionLocked(ClientSession* s);
    static bool promoteSessionLocked(ClientSession* s);
    static bool promoteAndResetLocked(ClientSession* s);
    static void rejectBusyLocked(ClientSession* s);
    static void rejectSessionLocked(ClientSession* s, const char* reason);
    static void commandHandler(ClientSession* s, Command cmd, uint8_t* data, int len);
    static void commandAckHandler(ClientSession* s, Command cmd, uint8_t* data, int len);
    static void drawMenu();
    static void renderUI(SmGui::DrawList* dl, std::string diffId, SmGui::DrawListElem diffValue);
    static void sendUILocked(ClientSession* s, Command originCmd, std::string diffId, SmGui::DrawListElem diffValue);
    static void sendInitialStateLocked(ClientSession* s);
    static void flushPushedStateLocked();
    static void stopRunningSourceLocked(const char* reason);
    static void heartbeatTick();
    static void clearSessionIfCurrent(const std::shared_ptr<ClientSession>& session);

    int main() {
        flog::info("=====| SERVER MODE |=====");

        // Init DSP
        comp.init(&dummyInput, dsp::compression::PCM_TYPE_I8);
        hnd.init(&comp.out, _testServerHandler, NULL);
        bbuf = new uint8_t[SERVER_MAX_PACKET_SIZE];
        comp.start();
        hnd.start();

        // Initialize baseband header pointers
        bb_pkt_hdr = (PacketHeader*)bbuf;
        bb_pkt_data = &bbuf[sizeof(PacketHeader)];

        // Initialize compressor
        cctx = ZSTD_createCCtx();

        std::string password = core::args["password"].s();
        if (!password.empty()) {
            crypto::pbkdf2Sha256((const uint8_t*)password.data(), password.size(),
                (const uint8_t*)SERVER_AUTH_SALT, sizeof(SERVER_AUTH_SALT) - 1,
                SERVER_AUTH_PBKDF2_ITERATIONS, authKey.data(), authKey.size());
            // Scrub both the local copy and the parser's stored original.
            // The OS-level command line (argv / GetCommandLine) is beyond
            // reach and may still expose it.
            std::fill(password.begin(), password.end(), '\0');
            core::args.scrubString("password");
            authRequired = true;
            flog::info("SDRIAK server password authentication enabled");
        }

        // Load config
        std::string modulesDir = core::getModulesDirectory();
        std::vector<std::string> modules;
        json moduleInstances;
        std::string sourceName;
        {
            auto configAccess = core::configManager.read();
            configAccess.tryGet("modules", modules);
            moduleInstances = configAccess.value("moduleInstances", json::object());
            configAccess.tryGet("source", sourceName);
        }
        auto modList = moduleInstances.items();

        // Initialize SmGui in server mode
        SmGui::init(true);

        flog::info("Loading modules");
        // Load modules and check type to only load sources ( TODO: Have a proper type parameter int the info )
        // TODO LATER: Add whitelist/blacklist stuff
        if (std::filesystem::is_directory(modulesDir)) {
            for (const auto& file : std::filesystem::directory_iterator(modulesDir)) {
                std::string path = file.path().generic_string();
                std::string fn = file.path().filename().string();
                if (file.path().extension().generic_string() != SDRPP_MOD_EXTENTSION) {
                    continue;
                }
                if (!file.is_regular_file()) { continue; }
                if (fn.find("source") == std::string::npos) { continue; }

                flog::info("Loading {0}", path);
                core::moduleManager.loadModule(path);
            }
        }
        else {
            flog::warn("Module directory {0} does not exist, not loading modules from directory", modulesDir);
        }

        // Load additional modules through the config ( TODO: Have a proper type parameter int the info )
        // TODO LATER: Add whitelist/blacklist stuff
        for (auto const& apath : modules) {
            std::filesystem::path file = core::resolveConfigPath(apath);
            std::string path = file.generic_string();
            std::string fn = file.filename().string();
            if (file.extension().generic_string() != SDRPP_MOD_EXTENTSION) {
                continue;
            }
            if (!std::filesystem::is_regular_file(file)) { continue; }
            if (fn.find("source") == std::string::npos) { continue; }

            flog::info("Loading {0}", path);
            core::moduleManager.loadModule(path);
        }

        // Create module instances
        for (auto const& [name, _module] : modList) {
            std::string mod = _module["module"];
            bool enabled = _module["enabled"];
            if (core::moduleManager.modules.find(mod) == core::moduleManager.modules.end()) { continue; }
            flog::info("Initializing {0} ({1})", name, mod);
            core::moduleManager.createInstance(name, mod);
            if (!enabled) { core::moduleManager.disableInstance(name); }
        }

        // Do post-init
        core::moduleManager.doPostInitAll();

        // Generate source list
        auto list = sigpath::sourceManager.getSourceNames();
        for (auto& name : list) {
            sourceList.define(name, name);
        }

        // Load sourceId from config
        sourceId = 0;
        if (sourceList.keyExists(sourceName)) { sourceId = sourceList.keyId(sourceName); }
        sigpath::sourceManager.selectSource(sourceList[sourceId]);

        // TODO: Use command line option
        std::string host = (std::string)core::args["addr"];
        int port = (int)core::args["port"];
        listener = net::listen(host, port);
        listener->acceptAsync(_clientHandler, NULL);

        flog::info("Ready, listening on {0}:{1}", host, port);
        while(1) {
            heartbeatTick();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        return 0;
    }

    static void _clientHandler(net::Conn conn, void* ctx) {
        // A streaming client keeps its slot: reject the newcomer outright.
        if (auto s = getSession(); s && s->isOpen()) {
            bool busy = false;
            {
                std::lock_guard lck(controlMtx);
                busy = running;
            }
            if (busy) {
                flog::info("REJECTED Connection from {0}:{1}, another client is already connected.", "TODO", "TODO");

                // Issue a disconnect command to the client
                uint8_t buf[sizeof(PacketHeader) + sizeof(CommandHeader)];
                PacketHeader* tmp_phdr = (PacketHeader*)buf;
                CommandHeader* tmp_chdr = (CommandHeader*)&buf[sizeof(PacketHeader)];
                tmp_phdr->size = sizeof(PacketHeader) + sizeof(CommandHeader);
                tmp_phdr->type = PACKET_TYPE_COMMAND;
                tmp_chdr->cmd = COMMAND_DISCONNECT;
                conn->write(tmp_phdr->size, buf);

                // TODO: Find something cleaner
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                conn->close();

                // Start another async accept
                listener->acceptAsync(_clientHandler, NULL);
                return;
            }
            // An idle client may be replaced, but only by a connection that
            // completes the hello (and authentication): a takeover at accept
            // time would let any unauthenticated TCP connect (a port scan, a
            // monitoring probe) evict a legitimate client. The newcomer
            // therefore starts as a pending session and only displaces the
            // current one in promoteSessionLocked().
        }

        flog::info("Connection from {0}:{1}", "TODO", "TODO");
        auto session = std::make_shared<ClientSession>(std::move(conn));

        // Publish the new session as pending and drop any previous
        // half-handshaken connection outside the lock. Destroying it joins
        // its read worker, so any callback still draining on it finishes
        // (against its own buffers) before it goes away.
        std::shared_ptr<ClientSession> old;
        {
            std::lock_guard lck(sessionMtx);
            old = std::move(pendingSession);
            pendingSession = session;
        }
        old.reset();

        // Start reading commands from the new client. The first command must
        // be COMMAND_HELLO; only then do we reset source state or send UI data.
        session->conn->readAsync(sizeof(PacketHeader), session->rstore.data(), _packetHandler, session.get());

        listener->acceptAsync(_clientHandler, NULL);
    }

    static void _packetHandler(int count, uint8_t* buf, void* ctx) {
        // The session is passed as a raw ctx. It stays valid for this whole
        // call: whichever thread destroys the session (accept thread replacing
        // a pending one, or heartbeatTick reaping a retired one) runs the conn
        // destructor first, which joins this very worker before freeing the
        // buffers below. Grab the conn pointer once here.
        ClientSession* s = (ClientSession*)ctx;
        net::ConnClass* conn = s->conn.get();
        PacketHeader* hdr = s->r_pkt_hdr;

        // Validate the client-supplied size before using it as a read length:
        // the receive buffer is a fixed SERVER_MAX_PACKET_SIZE, so an
        // out-of-range size would overflow it (and a value below the header
        // size would wrap the unsigned subtraction below).
        if (hdr->size < sizeof(PacketHeader) || hdr->size > SERVER_MAX_PACKET_SIZE) {
            flog::error("Invalid packet size from client: {0}", hdr->size);
            s->protocolRejected = true;
            return;
        }

        // Read the rest of the data (TODO: ADD TIMEOUT)
        int len = 0;
        int read = 0;
        int goal = hdr->size - sizeof(PacketHeader);
        while (len < goal) {
            read = conn->read(goal - len, &s->r_pkt_data[len]);
            if (read < 0) { return; };
            len += read;
        }

        if (!s->protocolReady.load()) {
            if (hdr->type == PACKET_TYPE_COMMAND && hdr->size >= sizeof(PacketHeader) + sizeof(CommandHeader)) {
                handleHello(s, (Command)s->r_cmd_hdr->cmd, s->r_cmd_data, hdr->size - sizeof(PacketHeader) - sizeof(CommandHeader));
            }
            else {
                std::lock_guard lck(controlMtx);
                rejectSessionLocked(s, "first packet was not a protocol hello");
            }
            if (s->protocolRejected.load()) { return; }
            conn->readAsync(sizeof(PacketHeader), s->rstore.data(), _packetHandler, s);
            return;
        }

        // Parse and process
        if (hdr->type == PACKET_TYPE_COMMAND && hdr->size >= sizeof(PacketHeader) + sizeof(CommandHeader)) {
            commandHandler(s, (Command)s->r_cmd_hdr->cmd, s->r_cmd_data, hdr->size - sizeof(PacketHeader) - sizeof(CommandHeader));
        }
        else if (hdr->type == PACKET_TYPE_COMMAND_ACK && hdr->size >= sizeof(PacketHeader) + sizeof(CommandHeader)) {
            commandAckHandler(s, (Command)s->r_cmd_hdr->cmd, s->r_cmd_data, hdr->size - sizeof(PacketHeader) - sizeof(CommandHeader));
        }
        else {
            std::lock_guard lck(controlMtx);
            s->sendError(ERROR_INVALID_PACKET);
        }

        // Start another async read
        conn->readAsync(sizeof(PacketHeader), s->rstore.data(), _packetHandler, s);
    }

    static void _testServerHandler(uint8_t* data, int count, void* ctx) {
        // Compress data if needed and fill out header fields
        if (compression) {
            bb_pkt_hdr->type = PACKET_TYPE_BASEBAND_COMPRESSED;
            bb_pkt_hdr->size = sizeof(PacketHeader) + (uint32_t)ZSTD_compressCCtx(cctx, &bbuf[sizeof(PacketHeader)], SERVER_MAX_PACKET_SIZE-sizeof(PacketHeader), data, count, 1);
        }
        else {
            bb_pkt_hdr->type = PACKET_TYPE_BASEBAND;
            bb_pkt_hdr->size = sizeof(PacketHeader) + count;
            memcpy(&bbuf[sizeof(PacketHeader)], data, count);
        }

        // Write to network. Snapshot the session so a concurrent reconnect on
        // the accept thread can't free the connection mid-write. The current
        // session is always fully handshaken and authenticated (promotion
        // gate), so only liveness needs checking here.
        auto s = getSession();
        if (!s) { return; }
        if (!s->isOpen() || !s->conn->write(bb_pkt_hdr->size, bbuf)) {
            s->protocolRejected = true;
        }
    }

    void setInput(dsp::stream<dsp::complex_t>* stream) {
        comp.setInput(stream);
    }

    static bool handleHello(ClientSession* s, Command cmd, uint8_t* data, int len) {
        std::lock_guard lck(controlMtx);
        if (cmd != COMMAND_HELLO || len != sizeof(HelloPayload)) {
            rejectSessionLocked(s, "first command was not a valid protocol hello");
            return false;
        }

        HelloPayload hello;
        memcpy(&hello, data, sizeof(hello));
        if (!isCompatibleHello(hello)) {
            rejectSessionLocked(s, "client protocol/fork ID is incompatible");
            return false;
        }
        if (authRequired && (hello.capabilities & SERVER_PROTOCOL_CAP_AUTH) == 0) {
            rejectSessionLocked(s, "client does not support password authentication");
            return false;
        }
        if (!isActiveSession(s)) {
            rejectSessionLocked(s, "client session is no longer active");
            return false;
        }

        s->peerCapabilities = hello.capabilities;
        return activateSessionLocked(s);
    }

    static bool activateSessionLocked(ClientSession* s) {
        if (authRequired) {
            // Stay pending: an unauthenticated connection must not displace
            // the current client or touch shared state. Promotion happens
            // once the challenge is answered (commandHandler).
            s->sendHelloAck();
            s->protocolReady = true;
            s->sendAuthChallenge();
            return true;
        }

        if (!promoteAndResetLocked(s)) {
            rejectBusyLocked(s);
            return false;
        }
        s->sendHelloAck();
        s->authenticated = true;
        s->protocolReady = true;
        sendInitialStateLocked(s);
        return true;
    }

    // Promote a session that completed its handshake to being THE client.
    // Fails if the session was superseded by a newer connection or the
    // current client started streaming meanwhile. The caller must hold
    // controlMtx (it reads `running`; the nested lock order is always
    // controlMtx -> sessionMtx).
    static bool promoteSessionLocked(ClientSession* s) {
        std::lock_guard lck(sessionMtx);
        if (currentSession.get() == s) { return true; }
        if (pendingSession.get() != s) { return false; }
        if (currentSession && currentSession->isOpen() && running) { return false; }
        if (currentSession) {
            flog::info("Closing previous SDRIAK server client, displaced by a newly handshaken connection.");
            // Closed and destroyed by heartbeatTick: destroying it here would
            // join its read worker, which may be blocked on the controlMtx we
            // hold.
            retiredSessions.push_back(std::move(currentSession));
        }
        currentSession = std::move(pendingSession);
        return true;
    }

    // Promote and reset the per-client settings for the new client.
    static bool promoteAndResetLocked(ClientSession* s) {
        if (!promoteSessionLocked(s)) { return false; }
        sigpath::sourceManager.stop();
        running = false;
        comp.setPCMType(dsp::compression::PCM_TYPE_I16);
        compression = false;
        return true;
    }

    // The handshake finished, but a competing client holds the slot.
    // COMMAND_DISCONNECT is what the client maps to "server busy".
    static void rejectBusyLocked(ClientSession* s) {
        flog::info("Rejecting handshaken SDRIAK server client: the server is in use.");
        s->protocolRejected = true;
        s->sendCommand(COMMAND_DISCONNECT, 0);
    }

    static void rejectSessionLocked(ClientSession* s, const char* reason) {
        flog::warn("Rejecting SDRIAK server client: {0}", reason);
        s->protocolRejected = true;
        if (s->isOpen()) { s->sendError(ERROR_PROTOCOL_MISMATCH); }
    }

    static void commandHandler(ClientSession* s, Command cmd, uint8_t* data, int len) {
        // Everything below acts on shared control state, so hold controlMtx
        // for the whole dispatch. A displaced session's socket may live for
        // a moment after promotion (it is closed by heartbeatTick); checking
        // isActiveSession under the same lock as the command's effects makes
        // the guard atomic with a concurrent promotion (which also holds
        // controlMtx), so a displaced session's in-flight command can't slip
        // through the check and act on the new client's state.
        std::lock_guard lck(controlMtx);
        if (!isActiveSession(s)) { return; }

        if (authRequired && !s->authenticated.load()) {
            if (cmd != COMMAND_AUTH_RESPONSE) {
                s->sendError(ERROR_AUTH_REQUIRED);
                return;
            }
            if (len != SERVER_AUTH_RESPONSE_SIZE) {
                s->sendError(ERROR_INVALID_ARGUMENT);
                return;
            }

            // During the post-failure cooldown, fail responses unverified so
            // reconnecting can't buy an attacker a faster guess rate.
            auto now = SteadyClock::now();
            bool throttled = now < authCooldownUntil;

            bool ok = false;
            if (!throttled) {
                std::array<uint8_t, SERVER_AUTH_RESPONSE_SIZE> expected{};
                crypto::hmacSha256(authKey.data(), authKey.size(), s->authChallenge.data(), s->authChallenge.size(), expected.data());
                ok = crypto::constantTimeEqual(expected.data(), data, expected.size());
            }
            if (!ok) {
                authCooldownUntil = now + AUTH_FAIL_COOLDOWN;
                flog::warn("Rejecting SDRIAK server client: authentication failed{0}", throttled ? " (throttled)" : "");
                s->sendError(ERROR_AUTH_FAILED);
                s->protocolRejected = true; // heartbeatTick closes the session
                return;
            }

            if (!promoteAndResetLocked(s)) {
                rejectBusyLocked(s);
                return;
            }
            s->authenticated = true;
            s->sendCommandAck(COMMAND_AUTH_RESPONSE, 0);
            sendInitialStateLocked(s);
            return;
        }

        if (cmd == COMMAND_GET_UI) {
            sendUILocked(s, COMMAND_GET_UI, "", dummyElem);
        }
        else if (cmd == COMMAND_UI_ACTION && len >= 3) {
            // Check if sending back data is needed
            int i = 0;
            bool sendback = data[i++];
            len--;

            // Load id
            SmGui::DrawListElem diffId;
            if (len <= 0) {
                s->sendError(ERROR_INVALID_ARGUMENT);
                return;
            }
            int count = SmGui::DrawList::loadItem(diffId, &data[i], len);
            if (count < 0 || diffId.type != SmGui::DRAW_LIST_ELEM_TYPE_STRING) {
                s->sendError(ERROR_INVALID_ARGUMENT);
                return;
            }
            i += count;
            len -= count;

            // Load value
            SmGui::DrawListElem diffValue;
            if (len <= 0) {
                s->sendError(ERROR_INVALID_ARGUMENT);
                return;
            }
            count = SmGui::DrawList::loadItem(diffValue, &data[i], len);
            if (count < 0) {
                s->sendError(ERROR_INVALID_ARGUMENT);
                return;
            }
            i += count;
            len -= count;

            // Render and send back
            if (sendback) {
                sendUILocked(s, COMMAND_UI_ACTION, diffId.str, diffValue);
            }
            else {
                renderUI(NULL, diffId.str, diffValue);
            }
        }
        else if (cmd == COMMAND_START) {
            sigpath::sourceManager.start();
            running = true;
        }
        else if (cmd == COMMAND_STOP) {
            sigpath::sourceManager.stop();
            running = false;
        }
        else if (cmd == COMMAND_SET_FREQUENCY && len == 8) {
            // memcpy: data is not 8-byte aligned (header offsets).
            double freq;
            memcpy(&freq, data, sizeof(double));
            sigpath::sourceManager.tune(freq);
            s->sendCommandAck(COMMAND_SET_FREQUENCY, 0);
        }
        else if (cmd == COMMAND_SET_SAMPLE_TYPE && len == 1) {
            dsp::compression::PCMType type = (dsp::compression::PCMType)*(uint8_t*)data;
            comp.setPCMType(type);
        }
        else if (cmd == COMMAND_SET_COMPRESSION && len == 1) {
            compression = *(uint8_t*)data;
        }
        else {
            flog::error("Invalid Command: {0} (len = {1})", (int)cmd, len);
            s->sendError(ERROR_INVALID_COMMAND);
        }

        // Module menu code run by the dispatch above can only record state
        // changes (setInputSampleRate/setTuningLimits); push them now so the
        // client learns about a change its own action just caused.
        flushPushedStateLocked();
    }

    static void commandAckHandler(ClientSession* s, Command cmd, uint8_t* data, int len) {
        if (cmd != COMMAND_HEARTBEAT) { return; }
        if (len != sizeof(uint32_t)) {
            flog::warn("Ignoring malformed heartbeat ACK (len={0})", len);
            return;
        }

        uint32_t seq;
        memcpy(&seq, data, sizeof(seq));

        std::lock_guard lck(controlMtx);
        if (s->heartbeatAwaitingAck && seq == s->heartbeatSeq) {
            s->heartbeatAwaitingAck = false;
        }
    }

    static void drawMenu() {
        if (running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo("##sdrpp_server_src_sel", &sourceId, sourceList.txt)) {
            sigpath::sourceManager.selectSource(sourceList[sourceId]);
            core::configManager.edit().set("source", sourceList.key(sourceId));
        }
        if (running) { SmGui::EndDisabled(); }

        sigpath::sourceManager.showSelectedMenu();
    }

    static void renderUI(SmGui::DrawList* dl, std::string diffId, SmGui::DrawListElem diffValue) {
        // If we're recording and there's an action, render once with the action and record without

        if (dl && !diffId.empty()) {
            SmGui::setDiff(diffId, diffValue);
            drawMenu();

            SmGui::setDiff("", dummyElem);
            SmGui::startRecord(dl);
            drawMenu();
            SmGui::stopRecord();
        }
        else {
            SmGui::setDiff(diffId, diffValue);
            SmGui::startRecord(dl);
            drawMenu();
            SmGui::stopRecord();
        }
    }

    static void sendUILocked(ClientSession* s, Command originCmd, std::string diffId, SmGui::DrawListElem diffValue) {
        // Render UI
        SmGui::DrawList dl;
        renderUI(&dl, diffId, diffValue);

        // Create response and send to network
        int size = dl.getSize();
        dl.store(s->s_cmd_data, size);
        s->sendCommandAck(originCmd, size);
    }

    static void sendInitialStateLocked(ClientSession* s) {
        double sr;
        bool limitsEnabled;
        double limitMin, limitMax;
        {
            std::lock_guard lck(pushedStateMtx);
            sr = sampleRate;
            limitsEnabled = tuningLimitEnabled;
            limitMin = tuningLimitMin;
            limitMax = tuningLimitMax;
            // The greeting sends the freshest values; drop pending pushes so
            // the next flush doesn't immediately duplicate them.
            samplerateDirty = false;
            tuningLimitsDirty = false;
        }
        s->sendSampleRate(sr);
        s->sendTuningLimits(limitsEnabled, limitMin, limitMax);
    }

    // Push samplerate/tuning-limit changes recorded by setInputSampleRate()
    // and setTuningLimits() to the connected client. Called after a command
    // dispatch (so a change a client action just caused goes out with it)
    // and from the heartbeat tick (for changes from module worker threads).
    static void flushPushedStateLocked() {
        auto s = getSession();
        if (!s || !s->isOpen()) { return; }

        bool sendSamplerate, sendLimits;
        double sr;
        bool limitsEnabled;
        double limitMin, limitMax;
        {
            std::lock_guard lck(pushedStateMtx);
            sendSamplerate = samplerateDirty;
            sendLimits = tuningLimitsDirty;
            samplerateDirty = false;
            tuningLimitsDirty = false;
            sr = sampleRate;
            limitsEnabled = tuningLimitEnabled;
            limitMin = tuningLimitMin;
            limitMax = tuningLimitMax;
        }
        if (sendSamplerate) { s->sendSampleRate(sr); }
        if (sendLimits) { s->sendTuningLimits(limitsEnabled, limitMin, limitMax); }
    }

    static void stopRunningSourceLocked(const char* reason) {
        if (!running) { return; }
        flog::warn("Stopping SDR source: {0}", reason);
        sigpath::sourceManager.stop();
        running = false;
    }

    // Record-only: called from source-module code both while the render path
    // already holds controlMtx and from module worker threads, so it must
    // only touch the pushedStateMtx leaf. flushPushedStateLocked() sends.
    void setTuningLimits(bool enabled, double minFreq, double maxFreq) {
        std::lock_guard lck(pushedStateMtx);
        tuningLimitEnabled = enabled;
        tuningLimitMin = minFreq;
        tuningLimitMax = maxFreq;
        tuningLimitsDirty = true;
    }

    // Record-only; see setTuningLimits.
    void setInputSampleRate(double samplerate) {
        std::lock_guard lck(pushedStateMtx);
        sampleRate = samplerate;
        samplerateDirty = true;
    }

    static void heartbeatTick() {
        // Destroy sessions displaced by a promotion. Deferred to here because
        // destroying a session joins its read worker; this thread holds no
        // locks, so a worker still draining a command can finish and exit.
        std::vector<std::shared_ptr<ClientSession>> retired;
        {
            std::lock_guard lck(sessionMtx);
            retired.swap(retiredSessions);
        }
        retired.clear();

        // Reap a pending (handshaking) session that was rejected, died, or
        // ran out its window without completing hello + authentication. The
        // expiry check runs under sessionMtx so it can't race a concurrent
        // promotion into reaping a session that just became current.
        std::shared_ptr<ClientSession> expired;
        {
            std::lock_guard lck(sessionMtx);
            if (pendingSession &&
                (pendingSession->protocolRejected.load() || !pendingSession->isOpen() ||
                 (SteadyClock::now() - pendingSession->connectedAt) >= HANDSHAKE_TIMEOUT)) {
                expired = std::move(pendingSession);
            }
        }
        if (expired) {
            if (!expired->protocolRejected.load() && expired->isOpen()) {
                flog::warn("SDRIAK server client did not complete the handshake in time; closing session");
            }
            if (expired->conn) { expired->conn->close(); }
            expired.reset();
        }

        auto s = getSession();
        if (!s) { return; }

        // The current session completed hello and authentication before it
        // was promoted, so only liveness needs tracking here.
        bool shouldClose = false;
        bool timedOut = false;
        {
            std::lock_guard lck(controlMtx);
            // Push state changes recorded by module worker threads (command
            // dispatch flushes its own; this covers everything else).
            flushPushedStateLocked();
            auto now = SteadyClock::now();
            if (s->protocolRejected.load() || !s->isOpen()) {
                shouldClose = true;
            }
            else if (s->heartbeatAwaitingAck && (now - s->heartbeatLastSend) >= HEARTBEAT_TIMEOUT) {
                shouldClose = true;
                timedOut = true;
            }
            else if ((s->peerCapabilities.load() & SERVER_PROTOCOL_CAP_HEARTBEAT) != 0 &&
                !s->heartbeatAwaitingAck && (now - s->heartbeatLastSend) >= HEARTBEAT_INTERVAL) {
                s->heartbeatAwaitingAck = true;
                s->heartbeatSeq++;
                s->heartbeatLastSend = now;
                s->sendHeartbeat(s->heartbeatSeq);
            }
        }

        if (!shouldClose) { return; }
        if (timedOut) {
            flog::warn("SDRIAK server client heartbeat timed out; closing orphaned session");
        }
        {
            std::lock_guard lck(controlMtx);
            stopRunningSourceLocked(timedOut ? "client heartbeat timed out" : "client disconnected");
        }
        if (s->conn) { s->conn->close(); }
        clearSessionIfCurrent(s);
    }

    static void clearSessionIfCurrent(const std::shared_ptr<ClientSession>& session) {
        std::lock_guard lck(sessionMtx);
        if (currentSession == session) { currentSession.reset(); }
    }
}
