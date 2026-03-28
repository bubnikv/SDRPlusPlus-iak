#include <config.h>
#include <core.h>
#include <gui/smgui.h>
#include <gui/style.h>
#include <imgui.h>
#include <module.h>
#include <qmx/QmxDevice.h>
#include <signal_path/signal_path.h>
#include <utils/flog.h>
#include <utils/optionlist.h>

#ifdef __ANDROID__
#include <android_backend.h>
#endif

#include <algorithm>
#include <cmath>
#include <string>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "qmx_source",
    /* Description:     */ "Direct QMX USB source module for SDR++",
    /* Author:          */ "OK1IAK",
    /* Version:         */ 0, 2, 0,
    /* Max instances    */ 1
};

ConfigManager config;

namespace {
    const int BAUD_RATES[] = {
        4800,
        9600,
        19200,
        38400,
        57600,
        115200
    };

    struct AudioChoice {
        std::string id;
        std::string label;

        bool operator==(const AudioChoice& other) const {
            return id == other.id;
        }
    };

    struct SerialChoice {
        std::string path;
        std::string label;

        bool operator==(const SerialChoice& other) const {
            return path == other.path;
        }
    };
}

class QMXSourceModule : public ModuleManager::Instance {
public:
    QMXSourceModule(std::string name) {
        this->name = std::move(name);
        sampleRate = qmx::kSampleRate;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        for (int baud : BAUD_RATES) {
            baudRates.define(baud, std::to_string(baud), baud);
        }

        refresh();
        loadConfig();

        sigpath::sourceManager.registerSource("QMX", &handler);
    }

    ~QMXSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("QMX");
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
    void loadConfig() {
        config.acquire();
        if (config.conf.contains("frequency")) {
            freq = config.conf["frequency"];
        }
        if (config.conf.contains("baudRate")) {
            int configured = config.conf["baudRate"];
            if (baudRates.keyExists(configured)) {
                baudId = baudRates.keyId(configured);
            }
        }
#ifndef __ANDROID__
        if (config.conf.contains("audioDevice")) {
            selectedAudioDevice = config.conf["audioDevice"];
        }
        if (config.conf.contains("serialPort")) {
            selectedSerialPort = config.conf["serialPort"];
        }
#endif
        config.release();

#ifndef __ANDROID__
        if (!selectedAudioDevice.empty()) {
            selectAudioDevice(selectedAudioDevice);
        }
        else {
            selectPreferredAudioDevice();
        }

        if (!selectedSerialPort.empty()) {
            selectSerialPort(selectedSerialPort);
        }
        else {
            selectFirstSerialPort();
        }
#endif
    }

    void refresh() {
#ifndef __ANDROID__
        audioDevices.clear();
        for (const auto& device : qmx::QmxDevice::listAudioDevices()) {
            audioDevices.define(device.id, device.label, { device.id, device.label });
        }

        serialPorts.clear();
        for (const auto& port : qmx::QmxDevice::listSerialPorts()) {
            serialPorts.define(port.path, port.label, { port.path, port.label });
        }
#endif
    }

#ifndef __ANDROID__
    void selectPreferredAudioDevice() {
        if (audioDevices.empty()) {
            selectedAudioDevice.clear();
            return;
        }

        for (int i = 0; i < audioDevices.size(); ++i) {
            const auto device = audioDevices.value(i);
            std::string upper = device.label;
            std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            if (upper.find("QMX") != std::string::npos || upper.find("QDX") != std::string::npos) {
                selectAudioDevice(audioDevices.key(i));
                return;
            }
        }

        selectAudioDevice(audioDevices.key(0));
    }

    void selectAudioDevice(const std::string& deviceId) {
        if (audioDevices.empty() || !audioDevices.keyExists(deviceId)) {
            selectedAudioDevice.clear();
            return;
        }

        selectedAudioDevice = deviceId;
        audioDevId = audioDevices.keyId(deviceId);
    }

    void selectFirstSerialPort() {
        if (serialPorts.empty()) {
            selectedSerialPort.clear();
            return;
        }
        selectSerialPort(serialPorts.key(0));
    }

    void selectSerialPort(const std::string& portName) {
        if (serialPorts.empty() || !serialPorts.keyExists(portName)) {
            selectedSerialPort.clear();
            return;
        }

        selectedSerialPort = portName;
        serialPortId = serialPorts.keyId(portName);
    }
#endif

    static void menuSelected(void* ctx) {
        auto* self = static_cast<QMXSourceModule*>(ctx);
        core::setInputSampleRate(self->sampleRate);
    }

    static void menuDeselected(void* ctx) {
        auto* self = static_cast<QMXSourceModule*>(ctx);
        flog::info("QMXSourceModule '{}': Menu Deselect!", self->name);
    }

    static void start(void* ctx) {
        auto* self = static_cast<QMXSourceModule*>(ctx);
        if (self->running) {
            return;
        }

        qmx::StartOptions options;
#ifndef __ANDROID__
        if (self->selectedAudioDevice.empty()) {
            flog::error("QMXSourceModule: No QMX audio device selected");
            return;
        }
        options.audioDeviceId = self->selectedAudioDevice;
        options.serialPort = self->selectedSerialPort;
        options.serialBaudRate = self->baudRates.value(self->baudId);
#else
        int vid = -1;
        int pid = -1;
        int fd = backend::getDeviceFD(vid, pid, backend::QMX_VIDPIDS);
        if (fd < 0) {
            flog::error("QMXSourceModule: No authorized QMX USB device available on Android");
            return;
        }
        options.androidUsb.fd = fd;
        options.androidUsb.vid = vid;
        options.androidUsb.pid = pid;
#endif

        std::string error;
        if (!self->device.start(options, &QMXSourceModule::sampleHandler, self, &error)) {
            flog::error("QMXSourceModule: {}", error);
            return;
        }

        self->running = true;
        if (self->freq > 0.0) {
            std::string tuneError;
            if (!self->device.setFrequency(static_cast<std::int64_t>(std::llround(self->freq)), &tuneError)) {
                flog::warn("QMXSourceModule: {}", tuneError);
            }
        }

        flog::info("QMXSourceModule '{}': Start!", self->name);
    }

    static void stop(void* ctx) {
        auto* self = static_cast<QMXSourceModule*>(ctx);
        if (!self->running) {
            return;
        }

        self->running = false;
        self->stream.stopWriter();
        self->device.stop();
        self->stream.clearWriteStop();

        flog::info("QMXSourceModule '{}': Stop!", self->name);
    }

    static void tune(double freq, void* ctx) {
        auto* self = static_cast<QMXSourceModule*>(ctx);
        self->freq = freq;

        config.acquire();
        config.conf["frequency"] = freq;
        config.release(true);

        if (self->running) {
            std::string error;
            if (!self->device.setFrequency(static_cast<std::int64_t>(std::llround(freq)), &error)) {
                flog::warn("QMXSourceModule: {}", error);
            }
        }
    }

    static void menuHandler(void* ctx) {
        auto* self = static_cast<QMXSourceModule*>(ctx);

#ifndef __ANDROID__
        if (self->running) {
            SmGui::BeginDisabled();
        }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_qmx_audio_dev_", self->name), &self->audioDevId, self->audioDevices.txt)) {
            std::string dev = self->audioDevices.key(self->audioDevId);
            self->selectAudioDevice(dev);
            config.acquire();
            config.conf["audioDevice"] = dev;
            config.release(true);
        }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_qmx_serial_dev_", self->name), &self->serialPortId, self->serialPorts.txt)) {
            std::string port = self->serialPorts.key(self->serialPortId);
            self->selectSerialPort(port);
            config.acquire();
            config.conf["serialPort"] = port;
            config.release(true);
        }

        if (SmGui::Combo(CONCAT("##_qmx_baud_", self->name), &self->baudId, self->baudRates.txt)) {
            config.acquire();
            config.conf["baudRate"] = self->baudRates.value(self->baudId);
            config.release(true);
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_qmx_refr_", self->name))) {
            self->refresh();
            self->selectAudioDevice(self->selectedAudioDevice);
            self->selectSerialPort(self->selectedSerialPort);
        }

        if (self->running) {
            SmGui::EndDisabled();
        }

        SmGui::Text("IQ Audio:");
        SmGui::SameLine();
        if (self->selectedAudioDevice.empty()) {
            SmGui::Text("Not selected");
        }
        else {
            SmGui::Text(self->audioDevices.value(self->audioDevId).label.c_str());
        }

        SmGui::Text("CAT:");
        SmGui::SameLine();
        if (self->selectedSerialPort.empty()) {
            SmGui::Text("Manual tune only");
        }
        else if (self->running) {
            SmGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), self->serialPorts.value(self->serialPortId).label.c_str());
        }
        else {
            SmGui::Text(self->serialPorts.value(self->serialPortId).label.c_str());
        }
#else
        SmGui::Text("Android backend: direct USB libusb");
        ImGui::TextWrapped("Connect the QMX over USB and grant USB permission. The Android backend uses direct libusb access to the composite QMX device for both IQ and simple CAT.");
#endif
    }

    static void sampleHandler(const qmx::IQSample* samples, std::size_t count, void* ctx) {
        auto* self = static_cast<QMXSourceModule*>(ctx);
        if (!self->running || !samples || count == 0) {
            return;
        }

        for (std::size_t i = 0; i < count; ++i) {
            self->stream.writeBuf[i].re = samples[i].i;
            self->stream.writeBuf[i].im = samples[i].q;
        }
        self->stream.swap(static_cast<int>(count));
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    double sampleRate = qmx::kSampleRate;
    double freq = 7000000.0;

    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    qmx::QmxDevice device;

    OptionList<int, int> baudRates;
    int baudId = 3;

#ifndef __ANDROID__
    OptionList<std::string, AudioChoice> audioDevices;
    OptionList<std::string, SerialChoice> serialPorts;
    std::string selectedAudioDevice;
    std::string selectedSerialPort;
    int audioDevId = 0;
    int serialPortId = 0;
#endif
};

MOD_EXPORT void _INIT_() {
    json def = json::object();
    def["frequency"] = 7000000.0;
    def["baudRate"] = 38400;
#ifndef __ANDROID__
    def["audioDevice"] = "";
    def["serialPort"] = "";
#endif
    config.setPath(core::args["root"].s() + "/qmx_source_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new QMXSourceModule(std::move(name));
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete static_cast<QMXSourceModule*>(instance);
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}

