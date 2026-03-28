#include "SerialCat.h"

#include <algorithm>
#include <array>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <filesystem>
#endif

namespace {
#ifndef _WIN32
    speed_t fixedBaudToPosix() {
#ifdef B115200
        return B115200;
#else
        return B38400;
#endif
    }
#endif
}

namespace qmx::detail {
    SerialCatPort::~SerialCatPort() {
        close();
    }

    bool SerialCatPort::open(const std::string& portName) {
        close();

#ifdef _WIN32
        std::string fullPort = portName;
        if (fullPort.rfind("\\\\.\\", 0) != 0) {
            fullPort = "\\\\.\\" + fullPort;
        }

        HANDLE serial = CreateFileA(fullPort.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (serial == INVALID_HANDLE_VALUE) {
            return false;
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (!GetCommState(serial, &dcb)) {
            CloseHandle(serial);
            return false;
        }

        dcb.BaudRate = qmx::kSerialBaudRate;
        dcb.ByteSize = 8;
        dcb.StopBits = ONESTOPBIT;
        dcb.Parity = NOPARITY;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;
        dcb.fAbortOnError = FALSE;

        if (!SetCommState(serial, &dcb)) {
            CloseHandle(serial);
            return false;
        }

        COMMTIMEOUTS timeouts = {};
        timeouts.ReadIntervalTimeout = 20;
        timeouts.ReadTotalTimeoutConstant = 20;
        timeouts.ReadTotalTimeoutMultiplier = 1;
        timeouts.WriteTotalTimeoutConstant = 20;
        timeouts.WriteTotalTimeoutMultiplier = 1;
        SetCommTimeouts(serial, &timeouts);
        PurgeComm(serial, PURGE_RXCLEAR | PURGE_TXCLEAR);

        handle = serial;
        return true;
#else
        int fd = ::open(portName.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
        if (fd < 0) {
            return false;
        }

        termios tty = {};
        if (tcgetattr(fd, &tty) != 0) {
            ::close(fd);
            return false;
        }

        cfmakeraw(&tty);
        speed_t speed = fixedBaudToPosix();
        cfsetospeed(&tty, speed);
        cfsetispeed(&tty, speed);
        tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
        tty.c_cflag |= CLOCAL | CREAD;
        tty.c_cflag &= ~(PARENB | PARODD);
        tty.c_cflag &= ~CSTOPB;
        tty.c_cflag &= ~CRTSCTS;
        tty.c_cc[VMIN] = 0;
        tty.c_cc[VTIME] = 1;

        if (tcsetattr(fd, TCSANOW, &tty) != 0) {
            ::close(fd);
            return false;
        }

        tcflush(fd, TCIOFLUSH);
        handle = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
        return true;
#endif
    }

    void SerialCatPort::close() {
        if (!handle) {
            return;
        }

#ifdef _WIN32
        CloseHandle(reinterpret_cast<HANDLE>(handle));
#else
        ::close(static_cast<int>(reinterpret_cast<intptr_t>(handle)));
#endif
        handle = nullptr;
    }

    bool SerialCatPort::isOpen() const {
        return handle != nullptr;
    }

    bool SerialCatPort::setIQMode(bool enabled) {
        return send(enabled ? "Q91;" : "Q90;");
    }

    bool SerialCatPort::setFrequency(std::int64_t frequency) {
        char cmd[32];
        std::snprintf(cmd, sizeof(cmd), "FA%011lld;", static_cast<long long>(frequency));
        return send(cmd);
    }

    std::vector<SerialPortInfo> SerialCatPort::listPorts() {
        std::vector<SerialPortInfo> ports;

#ifdef _WIN32
        char target[16];
        char resolved[256];
        for (int i = 1; i <= 256; ++i) {
            std::snprintf(target, sizeof(target), "COM%d", i);
            if (QueryDosDeviceA(target, resolved, sizeof(resolved)) != 0) {
                ports.push_back({ target, target });
            }
        }
#else
        static const std::array<const char*, 6> prefixes = {
            "/dev/ttyACM",
            "/dev/ttyUSB",
            "/dev/cu.usbmodem",
            "/dev/cu.usbserial",
            "/dev/tty.usbmodem",
            "/dev/tty.usbserial"
        };

        for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
            if (!entry.is_character_file()) {
                continue;
            }

            std::string path = entry.path().string();
            for (const char* prefix : prefixes) {
                if (path.rfind(prefix, 0) == 0) {
                    ports.push_back({ path, path });
                    break;
                }
            }
        }

        std::sort(ports.begin(), ports.end(), [](const SerialPortInfo& lhs, const SerialPortInfo& rhs) {
            return lhs.path < rhs.path;
        });
#endif

        return ports;
    }

    bool SerialCatPort::send(const std::string& command) {
        if (!handle) {
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex);

#ifdef _WIN32
        DWORD written = 0;
        if (!WriteFile(reinterpret_cast<HANDLE>(handle), command.data(), static_cast<DWORD>(command.size()), &written, nullptr)) {
            return false;
        }
        FlushFileBuffers(reinterpret_cast<HANDLE>(handle));
        return written == command.size();
#else
        ssize_t written = ::write(static_cast<int>(reinterpret_cast<intptr_t>(handle)), command.data(), command.size());
        if (written != static_cast<ssize_t>(command.size())) {
            return false;
        }
        tcdrain(static_cast<int>(reinterpret_cast<intptr_t>(handle)));
        return true;
#endif
    }
}
