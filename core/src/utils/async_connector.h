#pragma once
#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

// Runs a client connection attempt on a background thread so the GUI stays
// responsive while a slow or unreachable server is contacted, with
// cancellation and per-frame polling of the outcome.
//
// ClientT must be a movable, bool-testable handle with close() reachable
// through operator-> (e.g. std::shared_ptr<server::Client>,
// spyserver::SpyServerClient).
//
// Thread model: every member is GUI-thread only. The factory passed to
// begin() is the exception — it runs on a worker thread owned by this class.
// It must return a connected client or throw, and may watch cancelToken()
// to abort a long handshake early.
template<typename ClientT>
class AsyncConnector {
public:
    enum class Result {
        IDLE,       // no finished attempt to report
        CONNECTED,  // the client was moved into poll()'s out parameter
        FAILED,     // the attempt failed; message in lastError()
        CANCELLED   // the attempt was cancelled; any late client was discarded
    };

    ~AsyncConnector() {
        shutdown();
    }

    // Start a connection attempt. Returns false if one is already running.
    bool begin(std::function<ClientT()> factory) {
        if (connectingFlag.exchange(true)) { return false; }

        // connectingFlag was false, so a joinable worker has already finished:
        // join it and discard anything poll() never picked up.
        joinWorker();
        ClientT stale{};
        {
            std::lock_guard<std::mutex> lck(mtx);
            stale = std::move(pending);
            pendingError.clear();
        }
        if (stale) { stale->close(); }
        lastErrorStr.clear();
        cancelledFlag = false;

        try {
            worker = std::thread([this, factory = std::move(factory)]() {
                try {
                    ClientT client = factory();
                    bool discard = false;
                    {
                        std::lock_guard<std::mutex> lck(mtx);
                        discard = cancelledFlag.load();
                        if (!discard) { pending = std::move(client); }
                    }
                    // Moved into pending unless discarding, so this only
                    // closes a client that finished after a cancellation.
                    if (discard && client) { client->close(); }
                }
                catch (const std::exception& e) {
                    if (!cancelledFlag.load()) {
                        std::lock_guard<std::mutex> lck(mtx);
                        pendingError = e.what();
                    }
                }
                connectingFlag = false;
            });
        }
        catch (const std::exception& e) {
            connectingFlag = false;
            lastErrorStr = std::string("Could not start connection worker: ") + e.what();
        }
        return true;
    }

    // Call once per frame. Joins the worker once it is done and hands out
    // the outcome exactly once.
    Result poll(ClientT& clientOut) {
        if (connectingFlag.load() || !worker.joinable()) { return Result::IDLE; }
        worker.join();

        const bool wasCancelled = cancelledFlag.exchange(false);
        ClientT client{};
        std::string error;
        {
            std::lock_guard<std::mutex> lck(mtx);
            client = std::move(pending);
            error = std::move(pendingError);
        }
        if (wasCancelled) {
            if (client) { client->close(); }
            return Result::CANCELLED;
        }
        if (!client) {
            if (error.empty()) { return Result::IDLE; }
            lastErrorStr = std::move(error);
            return Result::FAILED;
        }
        clientOut = std::move(client);
        return Result::CONNECTED;
    }

    // Abandon the running attempt (source deselected, module shutting down).
    // The worker is not interrupted — it observes cancelToken() or finishes
    // on its own and the result is discarded — so this returns immediately.
    void cancel() {
        cancelledFlag = true;
        ClientT abandoned{};
        {
            std::lock_guard<std::mutex> lck(mtx);
            abandoned = std::move(pending);
            pendingError.clear();
        }
        if (abandoned) { abandoned->close(); }
        lastErrorStr.clear();
    }

    // cancel() and wait for the worker to end. For owner destructors, which
    // must stop the worker before members the factory uses are destroyed.
    void shutdown() {
        cancel();
        joinWorker();
    }

    bool connecting() const {
        return connectingFlag.load();
    }

    // Message of the last failed attempt, or from setError(). Empty if none.
    const std::string& lastError() const {
        return lastErrorStr;
    }

    // Record a failure produced outside the factory (input validation,
    // post-connection initialization on the GUI thread).
    void setError(std::string error) {
        lastErrorStr = std::move(error);
    }

    // Flag the factory may watch to abort a long handshake early.
    const std::atomic<bool>* cancelToken() const {
        return &cancelledFlag;
    }

private:
    void joinWorker() {
        if (worker.joinable()) { worker.join(); }
    }

    std::thread worker;
    std::mutex mtx;
    ClientT pending{};
    std::string pendingError;
    std::string lastErrorStr;
    std::atomic<bool> connectingFlag{false};
    std::atomic<bool> cancelledFlag{false};
};
