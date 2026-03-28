#pragma once

#include <qmx/QmxDevice.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace qmx::detail {
    class SerialCatPort {
    public:
        ~SerialCatPort();

        bool open(const std::string& portName);
        void close();
        bool isOpen() const;

        bool setIQMode(bool enabled);
        bool setFrequency(std::int64_t frequency);

        static std::vector<SerialPortInfo> listPorts();

    private:
        bool send(const std::string& command);

        void* handle = nullptr;
        std::mutex mutex;
    };
}
