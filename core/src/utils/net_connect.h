#pragma once

#include <chrono>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <cerrno>
#include <csignal>
#endif

// Connection-establishment internals shared by the two networking stacks
// (net.h and the legacy networking.h). Both implement the same timed
// non-blocking TCP connect; keeping the subtle parts (Winsock's except-set
// failure reporting, SO_ERROR handling) in one place means the next fix
// only lands once.
namespace net::detail {
#ifdef _WIN32
    using SocketHandle = SOCKET;
#else
    using SocketHandle = int;
#endif

    // Process-wide one-time initialization (WSAStartup / SIGPIPE ignore).
    // Safe to call from both stacks: WSAStartup is reference-counted.
    inline void ensureInit() {
        static std::once_flag flag;
        std::call_once(flag, []() {
#ifdef _WIN32
            WSADATA wsa;
            if (WSAStartup(MAKEWORD(2, 2), &wsa)) {
                throw std::runtime_error("Could not initialize WinSock2");
            }
#else
            // Disable SIGPIPE to avoid dying when the remote host disconnects
            signal(SIGPIPE, SIG_IGN);
#endif
        });
    }

    inline int lastOsError() {
#ifdef _WIN32
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    // Human-readable form of an OS socket error for exception messages.
    // strerror() does not map Winsock codes, so Windows keeps the number.
    inline std::string osErrorString(int error) {
#ifdef _WIN32
        return "error " + std::to_string(error);
#else
        return strerror(error);
#endif
    }

    // Milliseconds left of timeoutMs since started; <= 0 means expired.
    inline int remainingTimeoutMs(std::chrono::steady_clock::time_point started, int timeoutMs) {
        int elapsedMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        return timeoutMs - elapsedMs;
    }

    enum class ConnectStatus {
        SUCCESS,
        TIMEOUT,
        FAILED
    };

    // Establish a TCP connection on an already non-blocking socket, waiting
    // at most timeoutMs for the handshake. The caller owns the socket (and
    // closes it on failure) and decides whether to restore blocking mode.
    // On ERROR, errorOut holds the OS error code.
    inline ConnectStatus finishNonblockingConnect(SocketHandle sock, const sockaddr_in& addr, int timeoutMs, int& errorOut) {
        errorOut = 0;
        if (::connect(sock, (const sockaddr*)&addr, sizeof(sockaddr_in)) == 0) {
            return ConnectStatus::SUCCESS;
        }

        int connectError = lastOsError();
#ifdef _WIN32
        bool inProgress = (connectError == WSAEWOULDBLOCK || connectError == WSAEINPROGRESS);
#else
        bool inProgress = (connectError == EINPROGRESS || connectError == EWOULDBLOCK);
#endif
        if (!inProgress) {
            errorOut = connectError;
            return ConnectStatus::FAILED;
        }

        // Winsock reports a failed non-blocking connect on the except set,
        // not the write set; POSIX reports both outcomes as writability.
        fd_set writeSet;
        fd_set exceptSet;
        FD_ZERO(&writeSet);
        FD_ZERO(&exceptSet);
        FD_SET(sock, &writeSet);
        FD_SET(sock, &exceptSet);
        timeval tv {
            timeoutMs / 1000,
            (timeoutMs % 1000) * 1000
        };
        int selected = select((int)sock + 1, NULL, &writeSet, &exceptSet, &tv);
        if (selected == 0) {
            return ConnectStatus::TIMEOUT;
        }
        if (selected < 0) {
            errorOut = lastOsError();
            return ConnectStatus::FAILED;
        }

        // Either set fired; SO_ERROR distinguishes success from failure.
        int socketError = 0;
#ifdef _WIN32
        int socketErrorLen = sizeof(socketError);
#else
        socklen_t socketErrorLen = sizeof(socketError);
#endif
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&socketError, &socketErrorLen) != 0) {
            errorOut = lastOsError();
            return ConnectStatus::FAILED;
        }
        if (socketError) {
            errorOut = socketError;
            return ConnectStatus::FAILED;
        }
        return ConnectStatus::SUCCESS;
    }
}
