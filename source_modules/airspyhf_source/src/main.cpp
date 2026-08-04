#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <airspyhf.h>
#include <gui/widgets/stepped_slider.h>

#ifdef __ANDROID__
#include <android_backend.h>
#endif

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "airspyhf_source",
    /* Description:     */ "Airspy HF+ source module for SDR++",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

const char* AGG_MODES_STR = "Off\0Low\0High\0";

class AirspyHFSourceModule : public ModuleManager::Instance {
public:
    AirspyHFSourceModule(std::string name) {
        this->name = name;

        sampleRate = 768000.0;

        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;

        refresh();

        std::string devSerial = config.value("device", std::string());
        selectByString(devSerial);

        sigpath::sourceManager.registerSource("Airspy HF+", &handler);
    }

    ~AirspyHFSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("Airspy HF+");
    }

    void postInit() {}

    enum AGCMode {
        AGC_MODE_OFF,
        AGC_MODE_LOW,
        AGC_MODE_HIGG
    };

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
        int n = airspyhf_list_devices(serials, 256);

        char buf[1024];
        for (int i = 0; i < n; i++) {
            sprintf(buf, "%016" PRIX64, serials[i]);
            devList.push_back(serials[i]);
            devListTxt += buf;
            devListTxt += '\0';
        }
#else
        // Check for device presence
        if (!backend::hasUsbDeviceAvailable(backend::AIRSPYHF_VIDPIDS)) { return; }

        // Get device info
        std::string fakeName = "Airspy HF+ USB";
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
        backend::UsbDeviceLease usbHandle(backend::AIRSPYHF_VIDPIDS);
        if (!usbHandle.valid()) {
            selectedSerial = 0;
            selectedSerStr.clear();
            return;
        }
#endif
        airspyhf_device_t* dev;
        try {
#ifndef __ANDROID__
            int err = airspyhf_open_sn(&dev, serial);
#else
            flog::warn("====  CALLING airspyhf_open_fd  ====");
            int err = airspyhf_open_fd(&dev, usbHandle.fd());
            flog::warn("====  CALLED airspyhf_open_fd  => ({0}) ====", err);
#endif
            if (err != 0) {
                char buf[1024];
                sprintf(buf, "%016" PRIX64, serial);
                flog::error("Could not open Airspy HF+ {0}", buf);
                selectedSerial = 0;
                return;
            }
        }
        catch (const std::exception& e) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, serial);
            flog::error("Could not open Airspy HF+ {}", buf);
            return;
        }

        selectedSerial = serial;

        uint32_t sampleRates[256];
        airspyhf_get_samplerates(dev, sampleRates, 0);
        int n = sampleRates[0];
        airspyhf_get_samplerates(dev, sampleRates, n);
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
            auto txn = config.transaction();
            ConfigManager::Section dev = txn.section("devices", selectedSerStr);

            // Seed whatever this device is missing, which for a device never seen
            // before is the whole block.
            dev.ensure("sampleRate", 768000);
            dev.ensure("agcMode", 0);
            dev.ensure("lna", false);
            dev.ensure("attenuation", 0);

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

            // Load Gains
            dev.tryGet("agcMode", agcMode);
            dev.tryGet("lna", hfLNA);
            dev.tryGet("attenuation", atten);
        }

        airspyhf_close(dev);
    }

private:
#ifdef __ANDROID__
    void refreshAndroidSelection() {
        refresh();
        std::string devSerial = config.value("device", std::string());
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
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        // AirspyHF+ / HF+ Discovery tune 0-31 MHz and 64-260 MHz (mfr spec).
        // freqSelect only supports one contiguous window, so we clamp the
        // outer envelope; the 31-64 MHz gap cannot be excluded here. The
        // manager shifts this by the tuning offset for up/down-converters.
        sigpath::sourceManager.setTuningLimits(0.0, 260000000.0); // 0 - 260 MHz
        flog::info("AirspyHFSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        sigpath::sourceManager.clearTuningLimits();
        flog::info("AirspyHFSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (_this->running) { return; }
#ifdef __ANDROID__
        _this->refreshAndroidSelectionIfNeeded();
#endif
        if (_this->selectedSerial == 0) {
            flog::error("Tried to start AirspyHF+ source with null serial");
            return;
        }

#ifndef __ANDROID__
            int err = airspyhf_open_sn(&_this->openDev, _this->selectedSerial);
#else
            if (!_this->androidUsbHandle.acquire(backend::AIRSPYHF_VIDPIDS)) {
                flog::error("Tried to start AirspyHF+ source without a valid USB handle");
                return;
            }
            int err = airspyhf_open_fd(&_this->openDev, _this->androidUsbHandle.fd());
#endif
        if (err != 0) {
            char buf[1024];
            sprintf(buf, "%016" PRIX64, _this->selectedSerial);
            flog::error("Could not open Airspy HF+ {0}", buf);
#ifdef __ANDROID__
            _this->androidUsbHandle.reset();
#endif
            return;
        }

        airspyhf_set_samplerate(_this->openDev, _this->sampleRateList[_this->srId]);
        airspyhf_set_freq(_this->openDev, _this->freq);
        airspyhf_set_hf_agc(_this->openDev, (_this->agcMode != 0));
        if (_this->agcMode > 0) {
            airspyhf_set_hf_agc_threshold(_this->openDev, _this->agcMode - 1);
        }
        airspyhf_set_hf_att(_this->openDev, _this->atten / 6.0f);
        airspyhf_set_hf_lna(_this->openDev, _this->hfLNA);

        airspyhf_start(_this->openDev, callback, _this);

        _this->running = true;
        flog::info("AirspyHFSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        airspyhf_close(_this->openDev);
        _this->stream.clearWriteStop();
#ifdef __ANDROID__
        _this->androidUsbHandle.reset();
#endif
        flog::info("AirspyHFSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;
        if (_this->running) {
            airspyhf_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        flog::info("AirspyHFSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)ctx;

#ifdef __ANDROID__
        _this->refreshAndroidSelectionIfNeeded();
#endif
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_airspyhf_dev_sel_", _this->name), &_this->devId, _this->devListTxt.c_str())) {
            _this->selectBySerial(_this->devList[_this->devId]);
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.set("device", _this->selectedSerStr);
            }
        }

        if (SmGui::Combo(CONCAT("##_airspyhf_sr_sel_", _this->name), &_this->srId, _this->sampleRateListTxt.c_str())) {
            _this->sampleRate = _this->sampleRateList[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.transaction().section("devices", _this->selectedSerStr).set("sampleRate", _this->sampleRate);
            }
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_airspyhf_refr_", _this->name))) {
#ifdef __ANDROID__
            _this->refreshAndroidSelection();
#else
            _this->refresh();
            std::string devSerial = config.value("device", std::string());
            _this->selectByString(devSerial);
            core::setInputSampleRate(_this->sampleRate);
#endif
        }

        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("AGC Mode");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_airspyhf_agc_", _this->name), &_this->agcMode, AGG_MODES_STR)) {
            if (_this->running) {
                airspyhf_set_hf_agc(_this->openDev, (_this->agcMode != 0));
                if (_this->agcMode > 0) {
                    airspyhf_set_hf_agc_threshold(_this->openDev, _this->agcMode - 1);
                }
            }
            if (_this->selectedSerStr != "") {
                config.transaction().section("devices", _this->selectedSerStr).set("agcMode", _this->agcMode);
            }
        }

        SmGui::LeftLabel("Attenuation");
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_airspyhf_attn_", _this->name), &_this->atten, 0, 48, 6, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                airspyhf_set_hf_att(_this->openDev, _this->atten / 6.0f);
            }
            if (_this->selectedSerStr != "") {
                config.transaction().section("devices", _this->selectedSerStr).set("attenuation", _this->atten);
            }
        }

        if (SmGui::Checkbox(CONCAT("HF LNA##_airspyhf_lna_", _this->name), &_this->hfLNA)) {
            if (_this->running) {
                airspyhf_set_hf_lna(_this->openDev, _this->hfLNA);
            }
            if (_this->selectedSerStr != "") {
                config.transaction().section("devices", _this->selectedSerStr).set("lna", _this->hfLNA);
            }
        }
    }

    static int callback(airspyhf_transfer_t* transfer) {
        AirspyHFSourceModule* _this = (AirspyHFSourceModule*)transfer->ctx;
        memcpy(_this->stream.writeBuf, transfer->samples, transfer->sample_count * sizeof(dsp::complex_t));
        if (!_this->stream.swap(transfer->sample_count)) { return -1; }
        return 0;
    }

    std::string name;
    airspyhf_device_t* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    double freq;
    uint64_t selectedSerial = 0;
    int devId = 0;
    int srId = 0;
    int agcMode = AGC_MODE_OFF;
    bool hfLNA = false;
    float atten = 0.0f;
    std::string selectedSerStr = "";

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
    config.setPath(core::args["root"].s() + "/airspyhf_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new AirspyHFSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (AirspyHFSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
