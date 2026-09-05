#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <airspy.h>

#ifdef __ANDROID__
#include <android_backend.h>
#endif

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "airspy_source",
    /* Description:     */ "Airspy source module for SDRIAK",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class AirspySourceModule : public ModuleManager::Instance {
public:
    AirspySourceModule(std::string name) {
        this->name = name;

        airspy_init();

        sampleRate = 10000000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();
        if (sampleRateList.size() > 0) {
            sampleRate = sampleRateList[0];
        }

        // Select device from config
        std::string devSerial;
        {
            auto configAccess = config.edit();
            configAccess.ensure("device", "");
            configAccess.tryGet("device", devSerial);
        }
        selectByString(devSerial);

        sigpath::sourceManager.registerSource("Airspy", &handler);
    }

    ~AirspySourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Airspy");
        airspy_exit();
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

    void refresh() {
        devList.clear();
        devListTxt = "";
#ifndef __ANDROID__
        uint64_t serials[256];
        int n = airspy_list_devices(serials, 256);

        char buf[1024];
        for (int i = 0; i < n; i++) {
            sprintf(buf, "%016" PRIX64, serials[i]);
            devList.push_back(serials[i]);
            devListTxt += buf;
            devListTxt += '\0';
        }
#else
        // Check for device presence
        if (!backend::hasUsbDeviceAvailable(backend::AIRSPY_VIDPIDS)) { return; }

        // Get device info
        std::string fakeName = "Airspy USB";
        devList.push_back(0xDEADBEEF);
        devListTxt += fakeName;
        devListTxt += '\0';
#endif
    }

    void selectFirst() {
        if (devList.size() != 0) {
            selectBySerial(devList[0]);
            return;
        }
        selectedSerial = 0;
        selectedSerStr.clear();
        devId = 0;
    }

    void selectByString(std::string serial) {
        char buf[1024];
        for (int i = 0; i < devList.size(); i++) {
            sprintf(buf, "%016" PRIX64, devList[i]);
            std::string str = buf;
            if (serial == str) {
                selectBySerial(devList[i]);
                return;
            }
        }
        selectFirst();
    }

    void selectBySerial(uint64_t serial) {
#ifdef __ANDROID__
        backend::UsbDeviceLease usbHandle(backend::AIRSPY_VIDPIDS);
        if (!usbHandle.valid()) {
            selectedSerial = 0;
            selectedSerStr.clear();
            return;
        }
#endif
        airspy_device* dev;
        try {
#ifndef __ANDROID__
            int err = airspy_open_sn(&dev, serial);
#else
            int err = airspy_open_fd(&dev, usbHandle.fd());
#endif
            if (err != 0) {
                char buf[1024];
                sprintf(buf, "%016" PRIX64, serial);
                flog::error("Could not open Airspy {0}", buf);
                selectedSerial = 0;
                return;
            }
        }
        catch (const std::exception& e) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, serial);
            flog::error("Could not open Airspy {}", buf);
            return;
        }
        selectedSerial = serial;

        uint32_t sampleRates[256];
        airspy_get_samplerates(dev, sampleRates, 0);
        int n = sampleRates[0];
        airspy_get_samplerates(dev, sampleRates, n);
        sampleRateList.clear();
        sampleRateListTxt = "";
        for (int i = 0; i < n; i++) {
            sampleRateList.push_back(sampleRates[i]);
            sampleRateListTxt += getBandwdithScaled(sampleRates[i]);
            sampleRateListTxt += '\0';
        }

        char buf[1024];
        sprintf(buf, "%016" PRIX64, serial);
        selectedSerStr = std::string(buf);

        // Load config here
        {
            auto configAccess = config.edit();
            ConfigManager::EditSection dev = configAccess.section("devices", selectedSerStr);

            // Seed whatever this device is missing, which for a device never seen
            // before is the whole block.
            dev.ensure("sampleRate", 10000000);
            dev.ensure("gainMode", 0);
            dev.ensure("sensitiveGain", 0);
            dev.ensure("linearGain", 0);
            dev.ensure("lnaGain", 0);
            dev.ensure("mixerGain", 0);
            dev.ensure("vgaGain", 0);
            dev.ensure("lnaAgc", false);
            dev.ensure("mixerAgc", false);
            dev.ensure("biasT", false);

            // Load sample rate
            srId = 0;
            sampleRate = sampleRateList[0];
            int selectedSr = 0;
            if (dev.tryGet("sampleRate", selectedSr)) {
                for (int i = 0; i < sampleRateList.size(); i++) {
                    if (sampleRateList[i] == selectedSr) {
                        srId = i;
                        sampleRate = selectedSr;
                        break;
                    }
                }
            }

            // Load gains
            dev.tryGet("gainMode", gainMode);
            dev.tryGet("sensitiveGain", sensitiveGain);
            dev.tryGet("linearGain", linearGain);
            dev.tryGet("lnaGain", lnaGain);
            dev.tryGet("mixerGain", mixerGain);
            dev.tryGet("vgaGain", vgaGain);
            dev.tryGet("lnaAgc", lnaAgc);
            dev.tryGet("mixerAgc", mixerAgc);

            // Load Bias-T
            dev.tryGet("biasT", biasT);
        }

        airspy_close(dev);
    }

private:
    // Every menu widget persists one field under this device's own block. Does
    // nothing when no device is selected, since there'd be nowhere to put it.
    template <class T>
    static void saveDeviceSetting(const std::string& serial, std::string_view key, const T& value) {
        if (serial.empty()) { return; }
        config.edit().section("devices", serial).set(key, value);
    }

#ifdef __ANDROID__
    void refreshAndroidSelection() {
        refresh();
        std::string devSerial = config.read().value("device", std::string());
        selectByString(devSerial);
        core::setInputSampleRate(sampleRate);
        lastAndroidUsbHotplugGeneration = backend::usbHotplugGeneration.load(std::memory_order_relaxed);
    }

    void refreshAndroidSelectionIfNeeded() {
        if (running) {
            return;
        }

        int generation = backend::usbHotplugGeneration.load(std::memory_order_relaxed);
        if (generation == lastAndroidUsbHotplugGeneration) {
            return;
        }

        refreshAndroidSelection();
    }
#endif

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
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        // Airspy R2 / Mini native RX range (mfr spec); the manager shifts this
        // by the tuning offset for up/down-converters.
        sigpath::sourceManager.setTuningLimits(24000000.0, 1750000000.0); // 24 MHz - 1.75 GHz
        flog::info("AirspySourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        sigpath::sourceManager.clearTuningLimits();
        flog::info("AirspySourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        if (_this->running) { return; }
#ifdef __ANDROID__
        _this->refreshAndroidSelectionIfNeeded();
#endif
        if (_this->selectedSerial == 0) {
            flog::error("Tried to start Airspy source with null serial");
            return;
        }

#ifndef __ANDROID__
        int err = airspy_open_sn(&_this->openDev, _this->selectedSerial);
#else
        if (!_this->androidUsbHandle.acquire(backend::AIRSPY_VIDPIDS)) {
            flog::error("Tried to start Airspy source without a valid USB handle");
            return;
        }
        int err = airspy_open_fd(&_this->openDev, _this->androidUsbHandle.fd());
#endif
        if (err != 0) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, _this->selectedSerial);
            flog::error("Could not open Airspy {0}", buf);
#ifdef __ANDROID__
            _this->androidUsbHandle.reset();
#endif
            return;
        }

        airspy_set_samplerate(_this->openDev, _this->sampleRateList[_this->srId]);
        airspy_set_freq(_this->openDev, _this->freq);

        if (_this->gainMode == 0) {
            airspy_set_lna_agc(_this->openDev, 0);
            airspy_set_mixer_agc(_this->openDev, 0);
            airspy_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
        }
        else if (_this->gainMode == 1) {
            airspy_set_lna_agc(_this->openDev, 0);
            airspy_set_mixer_agc(_this->openDev, 0);
            airspy_set_linearity_gain(_this->openDev, _this->linearGain);
        }
        else if (_this->gainMode == 2) {
            if (_this->lnaAgc) {
                airspy_set_lna_agc(_this->openDev, 1);
            }
            else {
                airspy_set_lna_agc(_this->openDev, 0);
                airspy_set_lna_gain(_this->openDev, _this->lnaGain);
            }
            if (_this->mixerAgc) {
                airspy_set_mixer_agc(_this->openDev, 1);
            }
            else {
                airspy_set_mixer_agc(_this->openDev, 0);
                airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
            }
            airspy_set_vga_gain(_this->openDev, _this->vgaGain);
        }

        airspy_set_rf_bias(_this->openDev, _this->biasT);

        airspy_start_rx(_this->openDev, callback, _this);

        _this->running = true;
        flog::info("AirspySourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        airspy_close(_this->openDev);
        _this->stream.clearWriteStop();
#ifdef __ANDROID__
        _this->androidUsbHandle.reset();
#endif
        flog::info("AirspySourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;
        if (_this->running) {
            airspy_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        flog::info("AirspySourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        AirspySourceModule* _this = (AirspySourceModule*)ctx;

#ifdef __ANDROID__
        _this->refreshAndroidSelectionIfNeeded();
#endif
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_airspy_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectBySerial(_this->devList[_this->devId]);
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.edit().set("device", _this->selectedSerStr);
            }
        }

        if (SmGui::Combo(CONCAT("##_airspy_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            _this->sampleRate = _this->sampleRateList[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            saveDeviceSetting(_this->selectedSerStr, "sampleRate", _this->sampleRate);
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_airspy_refr_", _this->name))) {
#ifdef __ANDROID__
            _this->refreshAndroidSelection();
#else
            _this->refresh();
            std::string devSerial = config.read().value("device", std::string());
            _this->selectByString(devSerial);
            core::setInputSampleRate(_this->sampleRate);
#endif
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::BeginGroup();
        SmGui::Columns(3, CONCAT("AirspyGainModeColumns##_", _this->name), false);
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Sensitive##_airspy_gm_", _this->name), _this->gainMode == 0)) {
            _this->gainMode = 0;
            if (_this->running) {
                airspy_set_lna_agc(_this->openDev, 0);
                airspy_set_mixer_agc(_this->openDev, 0);
                airspy_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
            }
            saveDeviceSetting(_this->selectedSerStr, "gainMode", 0);
        }
        SmGui::NextColumn();
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Linear##_airspy_gm_", _this->name), _this->gainMode == 1)) {
            _this->gainMode = 1;
            if (_this->running) {
                airspy_set_lna_agc(_this->openDev, 0);
                airspy_set_mixer_agc(_this->openDev, 0);
                airspy_set_linearity_gain(_this->openDev, _this->linearGain);
            }
            saveDeviceSetting(_this->selectedSerStr, "gainMode", 1);
        }
        SmGui::NextColumn();
        SmGui::ForceSync();
        if (SmGui::RadioButton(CONCAT("Free##_airspy_gm_", _this->name), _this->gainMode == 2)) {
            _this->gainMode = 2;
            if (_this->running) {
                if (_this->lnaAgc) {
                    airspy_set_lna_agc(_this->openDev, 1);
                }
                else {
                    airspy_set_lna_agc(_this->openDev, 0);
                    airspy_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                if (_this->mixerAgc) {
                    airspy_set_mixer_agc(_this->openDev, 1);
                }
                else {
                    airspy_set_mixer_agc(_this->openDev, 0);
                    airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
                }
                airspy_set_vga_gain(_this->openDev, _this->vgaGain);
            }
            saveDeviceSetting(_this->selectedSerStr, "gainMode", 2);
        }
        SmGui::Columns(1, CONCAT("EndAirspyGainModeColumns##_", _this->name), false);
        SmGui::EndGroup();

        // Gain menus

        if (_this->gainMode == 0) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_sens_gain_", _this->name), &_this->sensitiveGain, 0, 21)) {
                if (_this->running) {
                    airspy_set_sensitivity_gain(_this->openDev, _this->sensitiveGain);
                }
                saveDeviceSetting(_this->selectedSerStr, "sensitiveGain", _this->sensitiveGain);
            }
        }
        else if (_this->gainMode == 1) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_lin_gain_", _this->name), &_this->linearGain, 0, 21)) {
                if (_this->running) {
                    airspy_set_linearity_gain(_this->openDev, _this->linearGain);
                }
                saveDeviceSetting(_this->selectedSerStr, "linearGain", _this->linearGain);
            }
        }
        else if (_this->gainMode == 2) {
            // TODO: Switch to a table for alignment
            if (_this->lnaAgc) { SmGui::BeginDisabled(); }
            SmGui::LeftLabel("LNA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_lna_gain_", _this->name), &_this->lnaGain, 0, 15)) {
                if (_this->running) {
                    airspy_set_lna_gain(_this->openDev, _this->lnaGain);
                }
                saveDeviceSetting(_this->selectedSerStr, "lnaGain", _this->lnaGain);
            }
            if (_this->lnaAgc) { SmGui::EndDisabled(); }

            if (_this->mixerAgc) { SmGui::BeginDisabled(); }
            SmGui::LeftLabel("Mixer Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_mix_gain_", _this->name), &_this->mixerGain, 0, 15)) {
                if (_this->running) {
                    airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
                }
                saveDeviceSetting(_this->selectedSerStr, "mixerGain", _this->mixerGain);
            }
            if (_this->mixerAgc) { SmGui::EndDisabled(); }

            SmGui::LeftLabel("VGA Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_airspy_vga_gain_", _this->name), &_this->vgaGain, 0, 15)) {
                if (_this->running) {
                    airspy_set_vga_gain(_this->openDev, _this->vgaGain);
                }
                saveDeviceSetting(_this->selectedSerStr, "vgaGain", _this->vgaGain);
            }

            // AGC Control
            SmGui::ForceSync();
            if (SmGui::Checkbox(CONCAT("LNA AGC##_airspy_", _this->name), &_this->lnaAgc)) {
                if (_this->running) {
                    if (_this->lnaAgc) {
                        airspy_set_lna_agc(_this->openDev, 1);
                    }
                    else {
                        airspy_set_lna_agc(_this->openDev, 0);
                        airspy_set_lna_gain(_this->openDev, _this->lnaGain);
                    }
                }
                saveDeviceSetting(_this->selectedSerStr, "lnaAgc", _this->lnaAgc);
            }
            SmGui::ForceSync();
            if (SmGui::Checkbox(CONCAT("Mixer AGC##_airspy_", _this->name), &_this->mixerAgc)) {
                if (_this->running) {
                    if (_this->mixerAgc) {
                        airspy_set_mixer_agc(_this->openDev, 1);
                    }
                    else {
                        airspy_set_mixer_agc(_this->openDev, 0);
                        airspy_set_mixer_gain(_this->openDev, _this->mixerGain);
                    }
                }
                saveDeviceSetting(_this->selectedSerStr, "mixerAgc", _this->mixerAgc);
            }
        }

        // Bias T
        if (SmGui::Checkbox(CONCAT("Bias T##_airspy_", _this->name), &_this->biasT)) {
            if (_this->running) {
                airspy_set_rf_bias(_this->openDev, _this->biasT);
            }
            saveDeviceSetting(_this->selectedSerStr, "biasT", _this->biasT);
        }
    }

    static int callback(airspy_transfer_t* transfer) {
        AirspySourceModule* _this = (AirspySourceModule*)transfer->ctx;
        memcpy(_this->stream.writeBuf, transfer->samples, transfer->sample_count * sizeof(dsp::complex_t));
        if (!_this->stream.swap(transfer->sample_count)) { return -1; }
        return 0;
    }

    std::string name;
    airspy_device* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    uint64_t selectedSerial = 0;
    std::string selectedSerStr = "";
    int devId = 0;
    int srId = 0;

    bool biasT = false;

    int lnaGain = 0;
    int vgaGain = 0;
    int mixerGain = 0;
    int linearGain = 0;
    int sensitiveGain = 0;

    int gainMode = 0;

    bool lnaAgc = false;
    bool mixerAgc = false;

#ifdef __ANDROID__
    backend::UsbDeviceLease androidUsbHandle;
    int lastAndroidUsbHotplugGeneration = 0;
#endif

    std::vector<uint64_t> devList;
    std::string devListTxt;
    std::vector<uint32_t> sampleRateList;
    std::string sampleRateListTxt;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/airspy_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new AirspySourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (AirspySourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.shutdown();
}
