#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#ifndef _WIN32
#include <arpa/inet.h>
#endif

namespace net::detail {
    enum class ResolveStatus {
        SUCCESS,
        FAILED,
        TIMEOUT
    };

    struct ResolveResult {
        std::mutex mtx;
        std::condition_variable cnd;
        std::string host;
        uint16_t port = 0;
        sockaddr_in address{};
        std::atomic<bool> abandoned{false};
        bool done = false;
        bool success = false;
    };

    inline void resolveIPv4Worker(const std::shared_ptr<ResolveResult>& result) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;

        addrinfo* addresses = nullptr;
        int error = getaddrinfo(result->host.c_str(), std::to_string(result->port).c_str(), &hints, &addresses);

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

    // getaddrinfo has no portable cancellation API. A single process-lifetime
    // worker bounds the number of potentially stuck resolver threads to one.
    // Timed-out requests are removed from its queue before they can accumulate.
    class ResolverService {
    public:
        ResolverService() : worker(&ResolverService::run, this) {}

        void submit(const std::shared_ptr<ResolveResult>& result) {
            {
                std::lock_guard lck(mtx);
                queue.push_back(result);
            }
            cnd.notify_one();
        }

        void abandon(const std::shared_ptr<ResolveResult>& result) {
            result->abandoned = true;
            std::lock_guard lck(mtx);
            queue.erase(std::remove(queue.begin(), queue.end(), result), queue.end());
        }

    private:
        void run() {
            while (true) {
                std::shared_ptr<ResolveResult> result;
                {
                    std::unique_lock lck(mtx);
                    cnd.wait(lck, [this]() { return !queue.empty(); });
                    result = std::move(queue.front());
                    queue.pop_front();
                }
                if (!result->abandoned) {
                    resolveIPv4Worker(result);
                }
            }
        }

        std::mutex mtx;
        std::condition_variable cnd;
        std::deque<std::shared_ptr<ResolveResult>> queue;
        std::thread worker;
    };

    inline ResolverService& resolverService() {
        // Intentionally process-lifetime: joining a worker blocked in
        // getaddrinfo during static destruction would hang application exit.
        static ResolverService* service = new ResolverService();
        return *service;
    }

    inline ResolveStatus resolveIPv4(const std::string& host, uint16_t port, int timeoutMS, sockaddr_in& address) {
        sockaddr_in numericAddress{};
        numericAddress.sin_family = AF_INET;
        numericAddress.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &numericAddress.sin_addr) == 1) {
            address = numericAddress;
            return ResolveStatus::SUCCESS;
        }

        auto result = std::make_shared<ResolveResult>();
        result->host = host;
        result->port = port;

        if (timeoutMS < 0) {
            resolveIPv4Worker(result);
        }
        else {
            ResolverService& service = resolverService();
            service.submit(result);
            std::unique_lock lck(result->mtx);
            if (!result->cnd.wait_for(lck, std::chrono::milliseconds(timeoutMS), [&]() { return result->done; })) {
                lck.unlock();
                service.abandon(result);
                return ResolveStatus::TIMEOUT;
            }
            lck.unlock();
        }

        std::lock_guard lck(result->mtx);
        if (!result->success) {
            return ResolveStatus::FAILED;
        }
        address = result->address;
        return ResolveStatus::SUCCESS;
    }
}
