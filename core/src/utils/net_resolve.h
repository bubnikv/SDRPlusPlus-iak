#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace net::detail {
    enum class ResolveStatus {
        SUCCESS,
        ERROR,
        TIMEOUT
    };

    struct ResolveResult {
        std::mutex mtx;
        std::condition_variable cnd;
        sockaddr_in address{};
        bool done = false;
        bool success = false;
    };

    inline void resolveIPv4Worker(const std::string& host, uint16_t port, const std::shared_ptr<ResolveResult>& result) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* addresses = nullptr;
        int error = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses);

        sockaddr_in address{};
        bool success = false;
        if (error == 0 && addresses && addresses->ai_addrlen >= sizeof(sockaddr_in)) {
            memcpy(&address, addresses->ai_addr, sizeof(address));
            success = true;
        }
        if (addresses) {
            freeaddrinfo(addresses);
        }

        {
            std::lock_guard lck(result->mtx);
            result->address = address;
            result->success = success;
            result->done = true;
        }
        result->cnd.notify_all();
    }

    // getaddrinfo has no portable cancellation API. Run it on an isolated
    // shared-state worker when a deadline is requested, so a slow resolver
    // cannot extend the caller's connection deadline or object lifetime.
    inline ResolveStatus resolveIPv4(const std::string& host, uint16_t port, int timeoutMS, sockaddr_in& address) {
        auto result = std::make_shared<ResolveResult>();

        if (timeoutMS < 0) {
            resolveIPv4Worker(host, port, result);
        }
        else {
            std::thread resolver(resolveIPv4Worker, host, port, result);
            std::unique_lock lck(result->mtx);
            if (!result->cnd.wait_for(lck, std::chrono::milliseconds(timeoutMS), [&]() { return result->done; })) {
                lck.unlock();
                resolver.detach();
                return ResolveStatus::TIMEOUT;
            }
            lck.unlock();
            resolver.join();
        }

        std::lock_guard lck(result->mtx);
        if (!result->success) {
            return ResolveStatus::ERROR;
        }
        address = result->address;
        return ResolveStatus::SUCCESS;
    }
}
