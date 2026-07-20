#include <spyserver_client.h>
#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/widgets/stepped_slider.h>
#include <gui/smgui.h>
#include <atomic>
#include <mutex>
#include <thread>


#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "spyserver_source",
    /* Description:     */ "SpyServer source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

const char* deviceTypesStr[] = {
    "Unknown",
    "Airspy One",
    "Airspy HF+",
    "RTL-SDR"
};

const char* streamFormatStr = "UInt8\0"
                              "Int16\0"
                              "Float32\0";

const SpyServerStreamFormat streamFormats[] = {
    SPYSERVER_STREAM_FORMAT_UINT8,
    SPYSERVER_STREAM_FORMAT_INT16,
    SPYSERVER_STREAM_FORMAT_FLOAT
};

const int streamFormatsBitCount[] = {
    8,
    16,
    32
};

ConfigManager config;

class SpyServerSourceModule : public ModuleManager::Instance {
public:
    SpyServerSourceModule(std::string name) {
        this->name = name;

        config.acquire();
        std::string host = config.conf["hostname"];
        port = config.conf["port"];
        config.release();

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        strcpy(hostname, host.c_str());

        sigpath::sourceManager.registerSource("SpyServer", &handler);
    }

    ~SpyServerSourceModule() {
        stop(this);
        cancelConnection();
        if (connectionThread.joinable()) {
            connectionThread.join();
        }
        sigpath::sourceManager.unregisterSource("SpyServer");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

private:
    std::string getBandwdithScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            sprintf(buf, "%.1lfMHz", bw / 1000000.0);
        }
        else if (bw >= 1000.0) {
            sprintf(buf, "%.1lfKHz", bw / 1000.0);
        }
        else {
            sprintf(buf, "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    static void menuSelected(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        gui::mainWindow.playButtonLocked = !(_this->client && _this->client->isOpen());
        flog::info("SpyServerSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        _this->cancelConnection();
        gui::mainWindow.playButtonLocked = false;
        flog::info("SpyServerSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (_this->running) { return; }
        
        // Try to connect if not already connected
        if (!_this->client) {
            _this->tryConnect();
            if (!_this->client) { return; }
        }

        int srvBits = streamFormatsBitCount[_this->iqType];
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FORMAT, streamFormats[_this->iqType]);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DECIMATION, _this->srId + _this->client->devInfo.MinimumIQDecimation);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, _this->freq);
        _this->client->setSetting(SPYSERVER_SETTING_STREAMING_MODE, SPYSERVER_STREAM_MODE_IQ_ONLY);
        _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
        _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->srId + _this->client->devInfo.MinimumIQDecimation));
        _this->client->startStream();

        _this->running = true;
        flog::info("SpyServerSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (!_this->running) { return; }

        _this->client->stopStream();

        _this->running = false;
        flog::info("SpyServerSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;
        if (_this->running) {
            _this->client->setSetting(SPYSERVER_SETTING_IQ_FREQUENCY, freq);
        }
        _this->freq = freq;
        flog::info("SpyServerSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        SpyServerSourceModule* _this = (SpyServerSourceModule*)ctx;

        _this->pollConnection();

        bool connected = (_this->client && _this->client->isOpen());
        gui::mainWindow.playButtonLocked = !connected;

        const bool connecting = _this->connecting.load();
        if (connected || connecting) { SmGui::BeginDisabled(); }
        if (SmGui::InputText(CONCAT("##_spyserver_srv_host_", _this->name), _this->hostname, 1023)) {
            config.acquire();
            config.conf["hostname"] = _this->hostname;
            config.release(true);
        }
        SmGui::SameLine();
        SmGui::FillWidth();
        if (SmGui::InputInt(CONCAT("##_spyserver_srv_port_", _this->name), &_this->port, 0, 0)) {
            config.acquire();
            config.conf["port"] = _this->port;
            config.release(true);
        }
        if (connected || connecting) { SmGui::EndDisabled(); }

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        // Keep this value for the whole ImGui scope. tryConnect() changes the
        // atomic immediately; testing it again below would call EndDisabled()
        // without the matching BeginDisabled() in the frame where Connect was
        // clicked, corrupting ImGui's style stack.
        if (connecting) { SmGui::BeginDisabled(); }
        if (!connected && SmGui::Button("Connect##spyserver_source")) {
            _this->tryConnect();
        }
        else if (connected && SmGui::Button("Disconnect##spyserver_source")) {
            _this->client->close();
        }
        if (connecting) { SmGui::EndDisabled(); }
        if (_this->running) { SmGui::EndDisabled(); }


        if (connected) {
            if (_this->running) { style::beginDisabled(); }
            SmGui::LeftLabel("Samplerate");
            SmGui::FillWidth();
            if (SmGui::Combo("##spyserver_source_sr", &_this->srId, _this->sampleRatesTxt.c_str())) {
                _this->sampleRate = _this->sampleRates[_this->srId];
                core::setInputSampleRate(_this->sampleRate);
                config.acquire();
                config.conf["devices"][_this->devRef]["sampleRateId"] = _this->srId;
                config.release(true);
            }
            if (_this->running) { style::endDisabled(); }

            SmGui::LeftLabel("Sample bit depth");
            SmGui::FillWidth();
            if (SmGui::Combo("##spyserver_source_type", &_this->iqType, streamFormatStr)) {
                int srvBits = streamFormatsBitCount[_this->iqType];
                _this->client->setSetting(SPYSERVER_SETTING_IQ_FORMAT, streamFormats[_this->iqType]);
                _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->srId + _this->client->devInfo.MinimumIQDecimation));

                config.acquire();
                config.conf["devices"][_this->devRef]["sampleBitDepthId"] = _this->iqType;
                config.release(true);
            }

            if (_this->client->devInfo.MaximumGainIndex) {
                SmGui::FillWidth();
                if (SmGui::SliderInt("##spyserver_source_gain", (int*)&_this->gain, 0, _this->client->devInfo.MaximumGainIndex)) {
                    int srvBits = streamFormatsBitCount[_this->iqType];
                    _this->client->setSetting(SPYSERVER_SETTING_GAIN, _this->gain);
                    _this->client->setSetting(SPYSERVER_SETTING_IQ_DIGITAL_GAIN, _this->client->computeDigitalGain(srvBits, _this->gain, _this->srId + _this->client->devInfo.MinimumIQDecimation));
                    config.acquire();
                    config.conf["devices"][_this->devRef]["gainId"] = _this->gain;
                    config.release(true);
                }
            }

            SmGui::Text("Status:");
            SmGui::SameLine();
            SmGui::TextColoredF(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Connected (%s)", deviceTypesStr[_this->client->devInfo.DeviceType]);
        }
        else {
            SmGui::Text("Status:");
            SmGui::SameLine();
            SmGui::Text(_this->connecting ? "Connecting..." : "Not connected");
            if (!_this->connecting && !_this->lastConnectionError.empty()) {
                SmGui::TextColoredF(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Connection failed: %s", _this->lastConnectionError.c_str());
            }
        }
    }

    void tryConnect() {
        if (connecting.exchange(true)) { return; }

        if (port <= 0 || port > 65535) {
            lastConnectionError = "Port must be between 1 and 65535";
            connecting = false;
            return;
        }

        if (connectionThread.joinable()) {
            connectionThread.join();
        }
        connectionCancelled = false;
        if (client) { client.reset(); }
        {
            std::lock_guard lck(connectionMtx);
            connectionError.clear();
        }
        lastConnectionError.clear();

        const std::string host = hostname;
        const uint16_t targetPort = port;
        try {
            connectionThread = std::thread([this, host, targetPort]() {
                try {
                    auto newClient = spyserver::connect(host, targetPort, &stream, 3000);
                    if (!newClient->waitForDevInfo(3000)) {
                        newClient->close();
                        throw std::runtime_error("SpyServer didn't respond with device information");
                    }

                    bool discard = false;
                    {
                        std::lock_guard lck(connectionMtx);
                        discard = connectionCancelled.load();
                        if (!discard) {
                            pendingClient = std::move(newClient);
                        }
                    }
                    if (discard) {
                        newClient->close();
                    }
                }
                catch (const std::exception& e) {
                    if (!connectionCancelled) {
                        std::lock_guard lck(connectionMtx);
                        connectionError = e.what();
                    }
                }
                connecting = false;
            });
        }
        catch (const std::exception& e) {
            connecting = false;
            lastConnectionError = std::string("Could not start connection worker: ") + e.what();
            flog::error("{}", lastConnectionError);
        }
    }

    void cancelConnection() {
        connectionCancelled = true;

        spyserver::SpyServerClient abandonedClient;
        {
            std::lock_guard lck(connectionMtx);
            abandonedClient = std::move(pendingClient);
            connectionError.clear();
        }
        if (abandonedClient) {
            abandonedClient->close();
        }
        lastConnectionError.clear();
    }

    void pollConnection() {
        if (connecting || !connectionThread.joinable()) { return; }

        connectionThread.join();

        const bool cancelled = connectionCancelled.exchange(false);
        spyserver::SpyServerClient newClient;
        std::string error;
        {
            std::lock_guard lck(connectionMtx);
            newClient = std::move(pendingClient);
            error = std::move(connectionError);
        }
        if (cancelled) {
            if (newClient) {
                newClient->close();
            }
            return;
        }
        if (!newClient) {
            if (!error.empty()) {
                flog::error("Could not connect to spyserver {}", error);
                lastConnectionError = std::move(error);
            }
            return;
        }

        try {
            constexpr uint32_t DEVICE_TYPE_COUNT = sizeof(deviceTypesStr) / sizeof(deviceTypesStr[0]);
            if (newClient->devInfo.DeviceType >= DEVICE_TYPE_COUNT) {
                throw std::runtime_error("SpyServer reported an unsupported device type");
            }
            if (newClient->devInfo.MinimumIQDecimation > newClient->devInfo.DecimationStageCount ||
                newClient->devInfo.DecimationStageCount >= 31 ||
                newClient->devInfo.MaximumSampleRate == 0) {
                throw std::runtime_error("SpyServer reported invalid sample-rate metadata");
            }

            client = std::move(newClient);
            char buf[1024];
            snprintf(buf, sizeof(buf), "%s [%08X]", deviceTypesStr[client->devInfo.DeviceType], client->devInfo.DeviceSerial);
            devRef = std::string(buf);

            config.acquire();
            try {
                if (!config.conf["devices"].contains(devRef)) {
                    config.conf["devices"][devRef]["sampleRateId"] = 0;
                    config.conf["devices"][devRef]["sampleBitDepthId"] = 1;
                    config.conf["devices"][devRef]["gainId"] = 0;
                }
                srId = config.conf["devices"][devRef]["sampleRateId"];
                iqType = config.conf["devices"][devRef]["sampleBitDepthId"];
                gain = config.conf["devices"][devRef]["gainId"];
                config.release(true);
            }
            catch (...) {
                config.release();
                throw;
            }

            gain = std::clamp<int>(gain, 0, client->devInfo.MaximumGainIndex);

            // Refresh sample rates on the GUI thread, where the source state is used.
            sampleRates.clear();
            sampleRatesTxt.clear();
            for (int i = client->devInfo.MinimumIQDecimation; i <= client->devInfo.DecimationStageCount; i++) {
                double sr = (double)client->devInfo.MaximumSampleRate / ((double)(1U << i));
                sampleRates.push_back(sr);
                sampleRatesTxt += getBandwdithScaled(sr);
                sampleRatesTxt += '\0';
            }

            srId = std::clamp<int>(srId, 0, (int)sampleRates.size() - 1);
            iqType = std::clamp<int>(iqType, 0, (int)(sizeof(streamFormats) / sizeof(streamFormats[0])) - 1);

            sampleRate = sampleRates[srId];
            core::setInputSampleRate(sampleRate);
            flog::info("Connected to server");
        }
        catch (const std::exception& e) {
            flog::error("Could not initialize SpyServer connection: {}", e.what());
            lastConnectionError = e.what();
            if (client) {
                client->close();
                client.reset();
            }
        }
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    double sampleRate = 1000000;
    double freq;

    char hostname[1024];
    int port = 5555;
    int iqType = 0;

    int srId = 0;
    std::vector<double> sampleRates;
    std::string sampleRatesTxt;

    uint32_t gain = 0;

    std::string devRef = "";

    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;

    spyserver::SpyServerClient client;
    spyserver::SpyServerClient pendingClient;
    std::thread connectionThread;
    std::mutex connectionMtx;
    std::string connectionError;
    std::string lastConnectionError;
    std::atomic<bool> connecting{false};
    std::atomic<bool> connectionCancelled{false};
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["hostname"] = "localhost";
    def["port"] = 5555;
    def["devices"] = json::object();
    config.setPath(core::args["root"].s() + "/spyserver_config.json");
    config.load(def);
    config.enableAutoSave();

    // Check config in case a user has a very old version
    config.acquire();
    bool corrected = false;
    if (!config.conf.contains("hostname") || !config.conf.contains("port") || !config.conf.contains("devices")) {
        config.conf = def;
        corrected = true;
    }
    config.release(corrected);
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new SpyServerSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (SpyServerSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
