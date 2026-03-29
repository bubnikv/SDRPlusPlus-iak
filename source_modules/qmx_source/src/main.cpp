#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif // _WIN32

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
#ifndef __ANDROID__
        if (config.conf.contains("audioDevice")) {
            selectedAudioDevice = config.conf["audioDevice"];
        }
        if (config.conf.contains("serialPort")) {
            selectedSerialPort = config.conf["serialPort"];
        }
#else
        if (config.conf.contains("device")) {
            selectedAndroidDevice = config.conf["device"];
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
#else
        selectAndroidDeviceByName(selectedAndroidDevice);
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
#else
        androidDevices.clear();
        androidDeviceListTxt.clear();
        selectedAndroidDevice.clear();
        androidDevId = 0;
        androidDevFd = -1;
        androidVid = -1;
        androidPid = -1;

        int vid = -1;
        int pid = -1;
        int fd = backend::getDeviceFD(vid, pid, backend::QMX_VIDPIDS);
        if (fd < 0) {
            return;
        }

        androidDevFd = fd;
        androidVid = vid;
        androidPid = pid;
        androidDevices.push_back("QMX USB");
        androidDeviceListTxt += androidDevices.back();
        androidDeviceListTxt += '\0';
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
#else
    void selectAndroidFirstDevice() {
        if (!androidDevices.empty()) {
            selectAndroidDeviceById(0);
        }
        else {
            selectedAndroidDevice.clear();
            androidDevId = 0;
        }
    }

    void selectAndroidDeviceByName(const std::string& name) {
        for (int i = 0; i < static_cast<int>(androidDevices.size()); ++i) {
            if (androidDevices[i] == name) {
                selectAndroidDeviceById(i);
                return;
            }
        }
        selectAndroidFirstDevice();
    }

    void selectAndroidDeviceById(int id) {
        if (id < 0 || id >= static_cast<int>(androidDevices.size())) {
            selectedAndroidDevice.clear();
            androidDevId = 0;
            return;
        }
        androidDevId = id;
        selectedAndroidDevice = androidDevices[id];
    }

    void refreshAndroidDeviceSelection(const std::string& preferredDevice = "") {
        std::string deviceToRestore = preferredDevice.empty() ? selectedAndroidDevice : preferredDevice;
        refresh();
        selectAndroidDeviceByName(deviceToRestore);

        config.acquire();
        config.conf["device"] = selectedAndroidDevice;
        config.release(true);

        lastAndroidUsbHotplugGeneration = backend::usbHotplugGeneration.load(std::memory_order_relaxed);
    }

    void refreshAndroidDeviceSelectionIfNeeded() {
        if (running) {
            return;
        }

        int generation = backend::usbHotplugGeneration.load(std::memory_order_relaxed);
        if (generation == lastAndroidUsbHotplugGeneration) {
            return;
        }

        refreshAndroidDeviceSelection();
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
#else
        self->refreshAndroidDeviceSelectionIfNeeded();
        if (self->selectedAndroidDevice.empty() || self->androidDevFd < 0) {
            flog::error("QMXSourceModule: No authorized QMX USB device available on Android");
            return;
        }
        options.androidUsb.fd = self->androidDevFd;
        options.androidUsb.vid = self->androidVid;
        options.androidUsb.pid = self->androidPid;
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

        float refreshBtnWidth = std::max(90.0f, ImGui::CalcTextSize("Refresh").x + (ImGui::GetStyle().FramePadding.x * 2.0f) + 4.0f);
        float serialComboWidth = ImGui::GetContentRegionAvail().x - refreshBtnWidth - ImGui::GetStyle().ItemSpacing.x;
        SmGui::SetNextItemWidth(std::max(1.0f, serialComboWidth));
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_qmx_serial_dev_", self->name), &self->serialPortId, self->serialPorts.txt)) {
            std::string port = self->serialPorts.key(self->serialPortId);
            self->selectSerialPort(port);
            config.acquire();
            config.conf["serialPort"] = port;
            config.release(true);
        }

        SmGui::SameLine();
        if (SmGui::Button(CONCAT("Refresh##_qmx_refr_", self->name), ImVec2(refreshBtnWidth, 0))) {
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
        self->refreshAndroidDeviceSelectionIfNeeded();
        if (self->running)
            SmGui::BeginDisabled();

        float refreshBtnWidth = std::max(90.0f, ImGui::CalcTextSize("Refresh").x + (ImGui::GetStyle().FramePadding.x * 2.0f) + 4.0f);
        float deviceComboWidth = ImGui::GetContentRegionAvail().x - refreshBtnWidth - ImGui::GetStyle().ItemSpacing.x;
        SmGui::SetNextItemWidth(std::max(1.0f, deviceComboWidth));
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_qmx_android_dev_", self->name), &self->androidDevId, self->androidDeviceListTxt.c_str())) {
            self->selectAndroidDeviceById(self->androidDevId);
            config.acquire();
            config.conf["device"] = self->selectedAndroidDevice;
            config.release(true);
        }

        SmGui::SameLine();
        if (SmGui::Button(CONCAT("Refresh##_qmx_refr_", self->name), ImVec2(refreshBtnWidth, 0)))
            self->refreshAndroidDeviceSelection();

        if (self->running)
            SmGui::EndDisabled();

            SmGui::Text("Device:");
        SmGui::SameLine();
        if (self->selectedAndroidDevice.empty())
            SmGui::Text("Not connected");
        else if (self->running)
            SmGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), self->selectedAndroidDevice.c_str());
        else
            SmGui::Text(self->selectedAndroidDevice.c_str());
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

#ifndef __ANDROID__
    OptionList<std::string, AudioChoice> audioDevices;
    OptionList<std::string, SerialChoice> serialPorts;
    std::string selectedAudioDevice;
    std::string selectedSerialPort;
    int audioDevId = 0;
    int serialPortId = 0;
#else
    std::vector<std::string> androidDevices;
    std::string androidDeviceListTxt;
    std::string selectedAndroidDevice;
    int androidDevId = 0;
    int androidDevFd = -1;
    int androidVid = -1;
    int androidPid = -1;
    int lastAndroidUsbHotplugGeneration = 0;
#endif
};

MOD_EXPORT void _INIT_() {
    json def = json::object();
    def["frequency"] = 7000000.0;
#ifndef __ANDROID__
    def["audioDevice"] = "";
    def["serialPort"] = "";
#else
    def["device"] = "";
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
