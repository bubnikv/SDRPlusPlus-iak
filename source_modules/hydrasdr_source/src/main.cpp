#include <imgui.h>
#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <config.h>
#include <gui/smgui.h>
#include <hydrasdr.h>
#include <utils/optionlist.h>
#include <cmath>
#include <cstring>
#include <chrono>

#ifdef __ANDROID__
#include <android_backend.h>
#endif

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "hydrasdr_source",
    /* Description:     */ "HydraSDR source module for SDR++",
    /* Author:          */ "Ryzerth/B.VERNOUX",
    /* Version:         */ 0, 1, 1,
    /* Max instances    */ 1
};

ConfigManager config;

class HydraSDRSourceModule : public ModuleManager::Instance {
public:
    HydraSDRSourceModule(std::string name) {
        this->name = name;

        serverMode = (bool)core::args["server"];

        // Query library version for API compatibility (requires >= 1.1.0)
        hydrasdr_lib_version(&libVersion);
        hasNewApi = (libVersion.major_version > 1) ||
                    (libVersion.major_version == 1 && libVersion.minor_version >= 1);
        flog::info("HydraSDR: Library version {}.{}.{}",
                   libVersion.major_version, libVersion.minor_version,
                   libVersion.revision);
        if (!hasNewApi) {
            flog::error("HydraSDR: Library version >= 1.1.0 required, found {}.{}.{}",
                        libVersion.major_version, libVersion.minor_version,
                        libVersion.revision);
        }

        // Build source name with library version for display in dropdown
        snprintf(sourceName, sizeof(sourceName), "HydraSDR lib v%d.%d.%d",
                 libVersion.major_version, libVersion.minor_version,
                 libVersion.revision);

        // Default ports (will be updated dynamically when device is selected)
        ports.define("rx0", "RX0", HYDRASDR_RF_PORT_RX0);

        // Populate DDC algorithms list
        refreshAlgorithms();

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

        // Select device from config
        config.acquire();
        std::string devSerial = config.conf["device"];
        config.release();
        selectByString(devSerial);

        sigpath::sourceManager.registerSource(sourceName, &handler);
    }

    ~HydraSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource(sourceName);
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
#ifndef __ANDROID__
        devices.clear();

        uint64_t serials[256];
        int n = hydrasdr_list_devices(serials, 256);
        char serialBuf[64];
        char displayBuf[256];
        char versionBuf[128];
        for (int i = 0; i < n; i++) {
            snprintf(serialBuf, sizeof(serialBuf), "%016" PRIX64, serials[i]);
            // Try to get firmware version by opening device temporarily
            hydrasdr_device* dev;
            if (hydrasdr_open_sn(&dev, serials[i]) == HYDRASDR_SUCCESS) {
                versionBuf[0] = 0;
                hydrasdr_device_info_t devInfo;
                memset(&devInfo, 0, sizeof(devInfo));
                if (hydrasdr_get_device_info(dev, &devInfo) == HYDRASDR_SUCCESS) {
                    snprintf(versionBuf, sizeof(versionBuf), "%s", devInfo.firmware_version);
                }
                hydrasdr_close(dev);
                // Extract part after "HydraSDR " if present
                const char* nameVer = versionBuf;
                if (strncmp(versionBuf, "HydraSDR ", 9) == 0) {
                    nameVer = versionBuf + 9;
                }
                snprintf(displayBuf, sizeof(displayBuf), "%s %s", nameVer, serialBuf);
            } else {
                snprintf(displayBuf, sizeof(displayBuf), "%s", serialBuf);
            }
            devices.define(serialBuf, displayBuf, serials[i]);
        }
#else
        devices.clear();

        // Check for device presence
        int vid, pid;
        devFd = backend::getDeviceFD(vid, pid, backend::HYDRASDR_VIDPIDS);
        if (devFd < 0) { return; }

        // Get device info
        std::string fakeName = "HydraSDR USB";
        devices.define(fakeName, 0);
#endif
    }

    void refreshAlgorithms() {
        algorithms.clear();

        // hydrasdr_list_conversion_algorithms() is only available in v1.1.0+
        if (hasNewApi) {
            const char *names[16];
            const char *descriptions[16];
            int count = hydrasdr_list_conversion_algorithms(names, descriptions, 16);

            for (int i = 0; i < count; i++) {
                algorithms.define(names[i], descriptions[i], i);
            }

            if (count == 0) {
                flog::warn("No DDC algorithms found, using default");
                algorithms.define("47_opt", "47-Tap Optimized (default)", 0);
            }
        }
        // No algorithms available for lib < 1.1.0
    }

    void selectFirst() {
        if (!devices.empty()) {
            selectBySerial(devices.value(0));
            return;
        }
        selectedSerial = 0;
        selectedSerStr.clear();
        devId = 0;
    }

    void selectByString(std::string serial) {
        if (devices.keyExists(serial)) {
            selectBySerial(devices.value(devices.keyId(serial)));
            return;
        }
        selectFirst();
    }

    void selectBySerial(uint64_t serial) {
        hydrasdr_device* dev;
        try {
#ifndef __ANDROID__
            int err = hydrasdr_open_sn(&dev, serial);
#else
            int err = hydrasdr_open_fd(&dev, devFd);
#endif
            if (err != 0) {
                char buf[1024];
                snprintf(buf, sizeof(buf), "%016" PRIX64, serial);
                flog::error("Could not open HydraSDR {0} (err={1})", buf, err);
                selectedSerial = 0;
                return;
            }
        }
        catch (const std::exception& e) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "%016" PRIX64, serial);
            flog::error("Could not open HydraSDR {} (exception: {})", buf, e.what());
            return;
        }
        devId = devices.valueId(serial);
        selectedSerial = serial;
        selectedSerStr = devices.key(devId);

        // Query device capabilities (gains, ports, temperature, bandwidths)
        {
            hydrasdr_device_info_t devInfo;
            memset(&devInfo, 0, sizeof(devInfo));
            if (hydrasdr_get_device_info(dev, &devInfo) == HYDRASDR_SUCCESS) {
                flog::info("HydraSDR: Device info - board: {}, fw: {}",
                           devInfo.board_name, devInfo.firmware_version);
                // Only enable a gain if capability flag set, max > 0 and step > 0
                hasLnaGain = (devInfo.features & HYDRASDR_CAP_LNA_GAIN) &&
                             (devInfo.lna_gain.max_value > 0) && (devInfo.lna_gain.step_value > 0);
                hasRfGain = (devInfo.features & HYDRASDR_CAP_RF_GAIN) &&
                            (devInfo.rf_gain.max_value > 0) && (devInfo.rf_gain.step_value > 0);
                hasMixerGain = (devInfo.features & HYDRASDR_CAP_MIXER_GAIN) &&
                               (devInfo.mixer_gain.max_value > 0) && (devInfo.mixer_gain.step_value > 0);
                hasFilterGain = (devInfo.features & HYDRASDR_CAP_FILTER_GAIN) &&
                                (devInfo.filter_gain.max_value > 0) && (devInfo.filter_gain.step_value > 0);
                hasVgaGain = (devInfo.features & HYDRASDR_CAP_VGA_GAIN) &&
                             (devInfo.vga_gain.max_value > 0) && (devInfo.vga_gain.step_value > 0);
                hasLinearityGain = (devInfo.features & HYDRASDR_CAP_LINEARITY_GAIN) &&
                                   (devInfo.linearity_gain.max_value > 0) && (devInfo.linearity_gain.step_value > 0);
                hasSensitivityGain = (devInfo.features & HYDRASDR_CAP_SENSITIVITY_GAIN) &&
                                     (devInfo.sensitivity_gain.max_value > 0) && (devInfo.sensitivity_gain.step_value > 0);
                hasLnaAgc = (devInfo.features & HYDRASDR_CAP_LNA_AGC) != 0;
                hasRfAgc = (devInfo.features & HYDRASDR_CAP_RF_AGC) != 0;
                hasMixerAgc = (devInfo.features & HYDRASDR_CAP_MIXER_AGC) != 0;
                hasFilterAgc = (devInfo.features & HYDRASDR_CAP_FILTER_AGC) != 0;

                // Store gain ranges
                if (hasLnaGain) {
                    lnaGainMin = devInfo.lna_gain.min_value;
                    lnaGainMax = devInfo.lna_gain.max_value;
                }
                if (hasRfGain) {
                    rfGainMin = devInfo.rf_gain.min_value;
                    rfGainMax = devInfo.rf_gain.max_value;
                }
                if (hasMixerGain) {
                    mixerGainMin = devInfo.mixer_gain.min_value;
                    mixerGainMax = devInfo.mixer_gain.max_value;
                }
                if (hasFilterGain) {
                    filterGainMin = devInfo.filter_gain.min_value;
                    filterGainMax = devInfo.filter_gain.max_value;
                }
                if (hasVgaGain) {
                    vgaGainMin = devInfo.vga_gain.min_value;
                    vgaGainMax = devInfo.vga_gain.max_value;
                }
                if (hasLinearityGain) {
                    linearGainMin = devInfo.linearity_gain.min_value;
                    linearGainMax = devInfo.linearity_gain.max_value;
                }
                if (hasSensitivityGain) {
                    sensitiveGainMin = devInfo.sensitivity_gain.min_value;
                    sensitiveGainMax = devInfo.sensitivity_gain.max_value;
                }

                // Check temperature sensor capability
                hasTemperatureSensor = (devInfo.features & HYDRASDR_CAP_TEMPERATURE_SENSOR) != 0;

                // Populate RF ports dynamically from device info
                ports.clear();
                for (int i = 0; i < devInfo.rf_port_count && i < 32; i++) {
                    if (devInfo.rf_ports & (1 << i)) {
                        const char* portName = devInfo.rf_port_info[i].name;
                        char keyBuf[32];
                        snprintf(keyBuf, sizeof(keyBuf), "%s", portName);
                        for (char* p = keyBuf; *p; p++) *p = tolower(*p);
                        ports.define(keyBuf, portName, (hydrasdr_rf_port_t)i);
                    }
                }
                // Ensure at least one port exists
                if (ports.empty()) {
                    ports.define("rx0", "RX0", HYDRASDR_RF_PORT_RX0);
                }
            }
        }

        uint32_t sampleRates[256];
        memset(sampleRates, 0, sizeof(sampleRates));
        hydrasdr_get_samplerates(dev, sampleRates, 0);
        int n = sampleRates[0];
        flog::info("HydraSDR: Found {} sample rates", n);
        if (n > 0 && n < 256) {
            hydrasdr_get_samplerates(dev, sampleRates, n);
            samplerates.clear();
            for (int i = 0; i < n; i++) {
                samplerates.define(sampleRates[i], getBandwidthScaled(sampleRates[i]), sampleRates[i]);
            }
        } else {
            flog::warn("HydraSDR: Invalid sample rate count: {}", n);
            samplerates.clear();
        }

        // Query available bandwidths (hydrasdr_get_bandwidths is v1.1.0+ only)
        bandwidths.clear();
        if (hasNewApi) {
            uint32_t bwList[256];
            memset(bwList, 0, sizeof(bwList));
            // Add "Auto" as first option (HYDRASDR_BANDWIDTH_AUTO = auto-select bandwidth)
            bandwidths.define(HYDRASDR_BANDWIDTH_AUTO, "Auto", HYDRASDR_BANDWIDTH_AUTO);
            int ret = hydrasdr_get_bandwidths(dev, bwList, 0);
            if (ret == 0 && bwList[0] > 0 && bwList[0] < 256) {
                int bwCount = bwList[0];
                hydrasdr_get_bandwidths(dev, bwList, bwCount);
                for (int i = 0; i < bwCount; i++) {
                    std::string bwStr = getBandwidthScaled(bwList[i]);
                    bandwidths.define(bwList[i], bwStr, bwList[i]);
                }
                flog::info("HydraSDR: Found {} bandwidths", bwCount);
            } else {
                flog::warn("HydraSDR: Bandwidth query failed or returned invalid count");
            }
        }
        // No bandwidth selection for lib < 1.1.0

        // Load config here
        config.acquire();
        bool created = false;
        if (!config.conf["devices"].contains(selectedSerStr)) {
            created = true;
            config.conf["devices"][selectedSerStr]["sampleRate"] = 10000000;
            config.conf["devices"][selectedSerStr]["bandwidth"] = 0;
            config.conf["devices"][selectedSerStr]["gainMode"] = 0;
            config.conf["devices"][selectedSerStr]["sensitiveGain"] = 0;
            config.conf["devices"][selectedSerStr]["linearGain"] = 0;
            config.conf["devices"][selectedSerStr]["lnaGain"] = 0;
            config.conf["devices"][selectedSerStr]["rfGain"] = 0;
            config.conf["devices"][selectedSerStr]["mixerGain"] = 0;
            config.conf["devices"][selectedSerStr]["filterGain"] = 0;
            config.conf["devices"][selectedSerStr]["vgaGain"] = 0;
            config.conf["devices"][selectedSerStr]["lnaAgc"] = false;
            config.conf["devices"][selectedSerStr]["rfAgc"] = false;
            config.conf["devices"][selectedSerStr]["mixerAgc"] = false;
            config.conf["devices"][selectedSerStr]["filterAgc"] = false;
            config.conf["devices"][selectedSerStr]["biasT"] = false;
            config.conf["devices"][selectedSerStr]["algorithm"] = "47_opt";
            config.conf["devices"][selectedSerStr]["highDefMode"] = false;
        }

        // Load sample rate
        srId = 0;
        sampleRate = samplerates.value(0);
        if (config.conf["devices"][selectedSerStr].contains("sampleRate")) {
            int selectedSr = config.conf["devices"][selectedSerStr]["sampleRate"];
            if (samplerates.keyExists(selectedSr)) {
                srId = samplerates.keyId(selectedSr);
                sampleRate = samplerates[srId];
            }
        }

        // Load bandwidth
        bwId = 0;
        if (!bandwidths.empty() && config.conf["devices"][selectedSerStr].contains("bandwidth")) {
            int selectedBw = config.conf["devices"][selectedSerStr]["bandwidth"];
            if (bandwidths.keyExists(selectedBw)) {
                bwId = bandwidths.keyId(selectedBw);
            }
        }

        // Load high definition mode (decimation mode)
        highDefMode = false;
        if (config.conf["devices"][selectedSerStr].contains("highDefMode")) {
            highDefMode = config.conf["devices"][selectedSerStr]["highDefMode"];
        }

        // Load port
        if (config.conf["devices"][selectedSerStr].contains("port")) {
            std::string portStr = config.conf["devices"][selectedSerStr]["port"];
            if (ports.keyExists(portStr)) {
                portId = ports.keyId(portStr);
            }
        }

        // Load gains
        if (config.conf["devices"][selectedSerStr].contains("gainMode")) {
            gainMode = config.conf["devices"][selectedSerStr]["gainMode"];
        }
        if (config.conf["devices"][selectedSerStr].contains("sensitiveGain")) {
            sensitiveGain = config.conf["devices"][selectedSerStr]["sensitiveGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("linearGain")) {
            linearGain = config.conf["devices"][selectedSerStr]["linearGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("lnaGain")) {
            lnaGain = config.conf["devices"][selectedSerStr]["lnaGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("rfGain")) {
            rfGain = config.conf["devices"][selectedSerStr]["rfGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("mixerGain")) {
            mixerGain = config.conf["devices"][selectedSerStr]["mixerGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("filterGain")) {
            filterGain = config.conf["devices"][selectedSerStr]["filterGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("vgaGain")) {
            vgaGain = config.conf["devices"][selectedSerStr]["vgaGain"];
        }
        if (config.conf["devices"][selectedSerStr].contains("lnaAgc")) {
            lnaAgc = config.conf["devices"][selectedSerStr]["lnaAgc"];
        }
        if (config.conf["devices"][selectedSerStr].contains("rfAgc")) {
            rfAgc = config.conf["devices"][selectedSerStr]["rfAgc"];
        }
        if (config.conf["devices"][selectedSerStr].contains("mixerAgc")) {
            mixerAgc = config.conf["devices"][selectedSerStr]["mixerAgc"];
        }
        if (config.conf["devices"][selectedSerStr].contains("filterAgc")) {
            filterAgc = config.conf["devices"][selectedSerStr]["filterAgc"];
        }

        // Load Bias-T
        if (config.conf["devices"][selectedSerStr].contains("biasT")) {
            biasT = config.conf["devices"][selectedSerStr]["biasT"];
        }

        // Load DDC algorithm
        algoId = 0; // Default to 47_opt
        if (config.conf["devices"][selectedSerStr].contains("algorithm")) {
            std::string algoStr = config.conf["devices"][selectedSerStr]["algorithm"];
            if (algorithms.keyExists(algoStr)) {
                algoId = algorithms.keyId(algoStr);
            }
        }

        config.release(created);

        hydrasdr_close(dev);
    }

private:
#ifdef __ANDROID__
    void refreshAndroidSelection() {
        refresh();
        config.acquire();
        std::string devSerial = config.conf["device"];
        config.release();
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

    std::string getBandwidthScaled(double bw) {
        char buf[1024];
        if (bw >= 1000000.0) {
            double mhz = bw / 1000000.0;
            // Use 2 decimal places if needed to avoid duplicates, otherwise 1
            if (fmod(mhz * 10.0, 1.0) > 0.001) {
                snprintf(buf, sizeof(buf), "%.2lfMHz", mhz);
            } else {
                snprintf(buf, sizeof(buf), "%.1lfMHz", mhz);
            }
        }
        else if (bw >= 1000.0) {
            snprintf(buf, sizeof(buf), "%.1lfKHz", bw / 1000.0);
        }
        else {
            snprintf(buf, sizeof(buf), "%.1lfHz", bw);
        }
        return std::string(buf);
    }

    // Apply all gain settings based on current gainMode and capabilities
    void applyGains() {
        if (gainMode == 0 && hasSensitivityGain) {
            // Sensitive mode - disable AGCs first
            if (hasLnaAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_LNA_AGC, 0);
            if (hasRfAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_RF_AGC, 0);
            if (hasMixerAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_MIXER_AGC, 0);
            if (hasFilterAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_FILTER_AGC, 0);
            hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_SENSITIVITY, sensitiveGain);
        }
        else if (gainMode == 1 && hasLinearityGain) {
            // Linear mode - disable AGCs first
            if (hasLnaAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_LNA_AGC, 0);
            if (hasRfAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_RF_AGC, 0);
            if (hasMixerAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_MIXER_AGC, 0);
            if (hasFilterAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_FILTER_AGC, 0);
            hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_LINEARITY, linearGain);
        }
        else if (gainMode == 2) {
            // Free mode - individual gain controls
            if (hasLnaAgc && lnaAgc) {
                hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_LNA_AGC, 1);
            } else {
                if (hasLnaAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_LNA_AGC, 0);
                if (hasLnaGain) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_LNA, lnaGain);
            }
            if (hasRfAgc && rfAgc) {
                hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_RF_AGC, 1);
            } else {
                if (hasRfAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_RF_AGC, 0);
                if (hasRfGain) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_RF, rfGain);
            }
            if (hasMixerAgc && mixerAgc) {
                hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_MIXER_AGC, 1);
            } else {
                if (hasMixerAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_MIXER_AGC, 0);
                if (hasMixerGain) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_MIXER, mixerGain);
            }
            if (hasFilterAgc && filterAgc) {
                hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_FILTER_AGC, 1);
            } else {
                if (hasFilterAgc) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_FILTER_AGC, 0);
                if (hasFilterGain) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_FILTER, filterGain);
            }
            if (hasVgaGain) hydrasdr_set_gain(openDev, HYDRASDR_GAIN_TYPE_VGA, vgaGain);
        }
    }

    static void menuSelected(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->sampleRate);
        flog::info("HydraSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        flog::info("HydraSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        if (_this->running) { return; }
#ifdef __ANDROID__
        _this->refreshAndroidSelectionIfNeeded();
        if (_this->devFd < 0) {
            flog::error("Tried to start HydraSDR source with invalid fd");
            return;
        }
        int err = hydrasdr_open_fd(&_this->openDev, _this->devFd);
#else
        if (_this->selectedSerial == 0) {
            flog::error("Tried to start HydraSDR source with null serial");
            return;
        }
        int err = hydrasdr_open_sn(&_this->openDev, _this->selectedSerial);
#endif
        if (err != 0) {
            char buf[1024];
            snprintf(buf, sizeof(buf), "%016" PRIX64, _this->selectedSerial);
            flog::error("Could not open HydraSDR {0}", buf);
            return;
        }

        // Set decimation mode before sample rate
        {
            enum hydrasdr_decimation_mode mode = _this->highDefMode ?
                HYDRASDR_DEC_MODE_HIGH_DEFINITION : HYDRASDR_DEC_MODE_LOW_BANDWIDTH;
            hydrasdr_set_decimation_mode(_this->openDev, mode);
        }
        // Set bandwidth (HYDRASDR_BANDWIDTH_AUTO = auto-select based on sample rate)
        if (!_this->bandwidths.empty()) {
            hydrasdr_set_bandwidth(_this->openDev, _this->bandwidths[_this->bwId]);
        }
        // Set samplerate
        hydrasdr_set_samplerate(_this->openDev, _this->samplerates[_this->srId]);
        hydrasdr_set_freq(_this->openDev, _this->freq);
        hydrasdr_set_rf_port(_this->openDev, _this->ports[_this->portId]);

        // Set DDC algorithm
        if (!_this->algorithms.empty()) {
            hydrasdr_set_conversion_algorithm(_this->openDev, _this->algorithms.key(_this->algoId).c_str());
        }

        _this->applyGains();
        hydrasdr_set_rf_bias(_this->openDev, _this->biasT);

        hydrasdr_start_rx(_this->openDev, callback, _this);

        _this->running = true;
        flog::info("HydraSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        if (!_this->running) { return; }
        _this->running = false;
        _this->stream.stopWriter();
        hydrasdr_close(_this->openDev);
        _this->stream.clearWriteStop();
        flog::info("HydraSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;
        if (_this->running) {
            hydrasdr_set_freq(_this->openDev, freq);
        }
        _this->freq = freq;
        flog::info("HydraSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)ctx;

#ifdef __ANDROID__
        _this->refreshAndroidSelectionIfNeeded();
#endif
        if (_this->running) { SmGui::BeginDisabled(); }

        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_hydrasdr_dev_sel_", _this->name), &_this->devId, _this->devices.txt)) {
            _this->selectBySerial(_this->devices[_this->devId]);
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["device"] = _this->selectedSerStr;
                config.release(true);
            }
        }
        if (!_this->serverMode && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Select HydraSDR device by serial number");
        }

        if (SmGui::Combo(CONCAT("##_hydrasdr_sr_sel_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->sampleRate = _this->samplerates[_this->srId];
            core::setInputSampleRate(_this->sampleRate);
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["sampleRate"] = _this->samplerates.key(_this->srId);
                config.release(true);
            }
        }
        if (!_this->serverMode && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Sample rate (cannot be changed while running)");
        }

        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_hydrasdr_refr_", _this->name))) {
#ifdef __ANDROID__
            _this->refreshAndroidSelection();
#else
            _this->refresh();
            config.acquire();
            std::string devSerial = config.conf["device"];
            config.release();
            _this->selectByString(devSerial);
            core::setInputSampleRate(_this->sampleRate);
#endif
        }
        if (!_this->serverMode && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Refresh device list and sample rates");
        }

        // High Definition mode checkbox (cannot be changed while running)
        if (SmGui::Checkbox(CONCAT("HD##_hydrasdr_highdef_", _this->name), &_this->highDefMode)) {
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["highDefMode"] = _this->highDefMode;
                config.release(true);
            }
        }
        if (!_this->serverMode && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("High Definition: better signal quality but more USB bandwidth");
        }
        // Display HW sample rate, bandwidth, and decimation factor when streaming
        if (_this->running && _this->openDev) {
            SmGui::SameLine();
            hydrasdr_device_info_t devInfo;
            memset(&devInfo, 0, sizeof(devInfo));
            if (hydrasdr_get_device_info(_this->openDev, &devInfo) == HYDRASDR_SUCCESS) {
                char infoStr[128];
                char hwSrStr[32], bwStr[32];
                // Format HW sample rate with smart units
                double hwSr = devInfo.current_hw_samplerate;
                const char* hwSrUnit = (hwSr >= 1e6) ? "MHz" : "kHz";
                double hwSrVal = (hwSr >= 1e6) ? (hwSr / 1e6) : (hwSr / 1e3);
                if (hwSrVal == (int)hwSrVal)
                    snprintf(hwSrStr, sizeof(hwSrStr), "%d%s", (int)hwSrVal, hwSrUnit);
                else
                    snprintf(hwSrStr, sizeof(hwSrStr), "%.2f%s", hwSrVal, hwSrUnit);
                // Format bandwidth with smart units (0 = Auto)
                double bw = devInfo.current_bandwidth;
                if (bw <= 0) {
                    snprintf(bwStr, sizeof(bwStr), "Auto");
                } else {
                    const char* bwUnit = (bw >= 1e6) ? "MHz" : "kHz";
                    double bwVal = (bw >= 1e6) ? (bw / 1e6) : (bw / 1e3);
                    if (bwVal == (int)bwVal)
                        snprintf(bwStr, sizeof(bwStr), "%d%s", (int)bwVal, bwUnit);
                    else
                        snprintf(bwStr, sizeof(bwStr), "%.2f%s", bwVal, bwUnit);
                }
                snprintf(infoStr, sizeof(infoStr), "HW SR:%s BW:%s Decim:%d",
                         hwSrStr, bwStr, devInfo.current_decimation_factor);
                SmGui::Text(infoStr);
            }
        }

        if (_this->running) { SmGui::EndDisabled(); }

        // Bandwidth selector (can be changed while running)
        if (!_this->bandwidths.empty()) {
            SmGui::LeftLabel("Bandwidth");
            SmGui::FillWidth();
            if (SmGui::Combo(CONCAT("##_hydrasdr_bw_sel_", _this->name), &_this->bwId, _this->bandwidths.txt)) {
                // Set bandwidth (HYDRASDR_BANDWIDTH_AUTO = Auto mode, resets to auto-bandwidth selection)
                if (_this->running) {
                    hydrasdr_set_bandwidth(_this->openDev, _this->bandwidths[_this->bwId]);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["bandwidth"] = _this->bandwidths.key(_this->bwId);
                    config.release(true);
                }
            }
            if (!_this->serverMode && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("RF bandwidth filter (Auto = auto-select based on sample rate)");
            }
        }

        // DDC Algorithm selector
        if (!_this->algorithms.empty()) {
            if (_this->running) { SmGui::BeginDisabled(); }

            SmGui::LeftLabel("DDC Algorithm");
            SmGui::FillWidth();
            if (SmGui::Combo(CONCAT("##_hydrasdr_algo_sel_", _this->name), &_this->algoId, _this->algorithms.txt)) {
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["algorithm"] = _this->algorithms.key(_this->algoId);
                    config.release(true);
                }
            }
            if (!_this->serverMode && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Digital Down Converter algorithm for decimation");
            }

            if (_this->running) { SmGui::EndDisabled(); }
        }

        // Temperature and streaming stats display
        if (_this->running) {
            // Auto-refresh temperature if enabled
            if (_this->hasTemperatureSensor && _this->autoRefreshTemp) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - _this->lastTempUpdate).count();
                if (elapsed >= 1000) {
                    hydrasdr_temperature_t temp;
                    if (hydrasdr_get_temperature(_this->openDev, &temp) == HYDRASDR_SUCCESS && temp.valid) {
                        _this->currentTemperature = temp.temperature_celsius;
                        _this->temperatureValid = true;
                    } else {
                        _this->temperatureValid = false;
                    }
                    _this->lastTempUpdate = now;
                }
            }

            // Update streaming stats
            hydrasdr_streaming_stats_t stats;
            if (hydrasdr_get_streaming_stats(_this->openDev, &stats) == HYDRASDR_SUCCESS) {
                _this->droppedBuffers = stats.buffers_dropped;
                _this->receivedBuffers = stats.buffers_received;
            }

            // Temperature display with auto-refresh checkbox
            if (_this->hasTemperatureSensor) {
                char tempStr[32];
                SmGui::ForceSync();
                if (SmGui::Checkbox(CONCAT("Temperature Auto##_hydrasdr_temp_", _this->name), &_this->autoRefreshTemp)) {
                    if (_this->autoRefreshTemp) {
                        // Trigger immediate refresh
                        _this->lastTempUpdate = std::chrono::steady_clock::now() - std::chrono::seconds(2);
                    }
                }
                SmGui::SameLine();
                if (_this->temperatureValid) {
                    snprintf(tempStr, sizeof(tempStr), "%.1f DegC", _this->currentTemperature);
                } else {
                    snprintf(tempStr, sizeof(tempStr), "-- DegC");
                }
                SmGui::Text(tempStr);
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Auto-refresh temperature every second");
                }
            }

            // Dropped buffers display (only show if there are dropped buffers)
            if (_this->droppedBuffers > 0) {
                char statsStr[64];
                snprintf(statsStr, sizeof(statsStr), "%llu dropped!", (unsigned long long)_this->droppedBuffers);
                SmGui::LeftLabel("Buffers");
                SmGui::Text(statsStr);
            }
        }

        SmGui::LeftLabel("Antenna Port");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_hydrasdr_port_", _this->name), &_this->portId, _this->ports.txt)) {
            if (_this->running) {
                hydrasdr_set_rf_port(_this->openDev, _this->ports[_this->portId]);
            }
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["port"] = _this->ports.key(_this->portId);
                config.release(true);
            }
        }
        if (!_this->serverMode && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("RF input port selection");
        }

        // Gain mode selection - dynamic based on device capabilities
        // Check which modes are available
        bool hasFreeMode = _this->hasLnaGain || _this->hasRfGain || _this->hasMixerGain ||
                           _this->hasFilterGain || _this->hasVgaGain;
        bool hasGlobalModes = _this->hasSensitivityGain || _this->hasLinearityGain;

        // If only Free mode is available (no Sensitive/Linear), force gainMode to 2
        // and don't show mode selection at all
        if (hasFreeMode && !hasGlobalModes) {
            _this->gainMode = 2;
        }

        // Only show mode selection if there are multiple options
        if (hasGlobalModes) {
            int availableModes = 0;
            if (_this->hasSensitivityGain) availableModes++;
            if (_this->hasLinearityGain) availableModes++;
            if (hasFreeMode) availableModes++;

            SmGui::BeginGroup();
            SmGui::Columns(availableModes, CONCAT("HydraSDRGainModeColumns##_", _this->name), false);

            // Sensitive mode (if available)
            if (_this->hasSensitivityGain) {
                SmGui::ForceSync();
                if (SmGui::RadioButton(CONCAT("Sensitive##_hydrasdr_gm_", _this->name), _this->gainMode == 0)) {
                    _this->gainMode = 0;
                    if (_this->running) {
                        _this->applyGains();
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["gainMode"] = 0;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Optimized for weak signal reception");
                }
                SmGui::NextColumn();
            }

            // Linear mode (if available)
            if (_this->hasLinearityGain) {
                SmGui::ForceSync();
                if (SmGui::RadioButton(CONCAT("Linear##_hydrasdr_gm_", _this->name), _this->gainMode == 1)) {
                    _this->gainMode = 1;
                    if (_this->running) {
                        _this->applyGains();
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["gainMode"] = 1;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Linear gain for strong signals, reduces distortion");
                }
                SmGui::NextColumn();
            }

            // Free mode (if any individual gain controls available)
            if (hasFreeMode) {
                SmGui::ForceSync();
                if (SmGui::RadioButton(CONCAT("Free##_hydrasdr_gm_", _this->name), _this->gainMode == 2)) {
                    _this->gainMode = 2;
                    if (_this->running) {
                        _this->applyGains();
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["gainMode"] = 2;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Manual control of individual gain stages");
                }
            }

            SmGui::Columns(1, CONCAT("EndHydraSDRGainModeColumns##_", _this->name), false);
            SmGui::EndGroup();
        }

        // Gain sliders - dynamic based on mode and device capabilities
        if (_this->gainMode == 0 && _this->hasSensitivityGain) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_hydrasdr_sens_gain_", _this->name), &_this->sensitiveGain,
                                 _this->sensitiveGainMin, _this->sensitiveGainMax)) {
                if (_this->running) {
                    hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_SENSITIVITY, _this->sensitiveGain);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["sensitiveGain"] = _this->sensitiveGain;
                    config.release(true);
                }
            }
            if (!_this->serverMode && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Sensitivity gain: higher = more sensitive to weak signals");
            }
        }
        else if (_this->gainMode == 1 && _this->hasLinearityGain) {
            SmGui::LeftLabel("Gain");
            SmGui::FillWidth();
            if (SmGui::SliderInt(CONCAT("##_hydrasdr_lin_gain_", _this->name), &_this->linearGain,
                                 _this->linearGainMin, _this->linearGainMax)) {
                if (_this->running) {
                    hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_LINEARITY, _this->linearGain);
                }
                if (_this->selectedSerStr != "") {
                    config.acquire();
                    config.conf["devices"][_this->selectedSerStr]["linearGain"] = _this->linearGain;
                    config.release(true);
                }
            }
            if (!_this->serverMode && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Linearity gain: lower = less distortion for strong signals");
            }
        }
        else if (_this->gainMode == 2 || (hasFreeMode && !hasGlobalModes)) {
            // Free mode - show available individual gain controls
            // LNA Gain
            if (_this->hasLnaGain) {
                if (_this->hasLnaAgc && _this->lnaAgc) { SmGui::BeginDisabled(); }
                SmGui::LeftLabel("LNA Gain");
                SmGui::FillWidth();
                if (SmGui::SliderInt(CONCAT("##_hydrasdr_lna_gain_", _this->name), &_this->lnaGain,
                                     _this->lnaGainMin, _this->lnaGainMax)) {
                    if (_this->running) {
                        hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_LNA, _this->lnaGain);
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["lnaGain"] = _this->lnaGain;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Low Noise Amplifier gain (first stage)");
                }
                if (_this->hasLnaAgc && _this->lnaAgc) { SmGui::EndDisabled(); }
            }

            // RF Gain
            if (_this->hasRfGain) {
                if (_this->hasRfAgc && _this->rfAgc) { SmGui::BeginDisabled(); }
                SmGui::LeftLabel("RF Gain");
                SmGui::FillWidth();
                if (SmGui::SliderInt(CONCAT("##_hydrasdr_rf_gain_", _this->name), &_this->rfGain,
                                     _this->rfGainMin, _this->rfGainMax)) {
                    if (_this->running) {
                        hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_RF, _this->rfGain);
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["rfGain"] = _this->rfGain;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("RF amplifier gain (after LNA)");
                }
                if (_this->hasRfAgc && _this->rfAgc) { SmGui::EndDisabled(); }
            }

            // Mixer Gain
            if (_this->hasMixerGain) {
                if (_this->hasMixerAgc && _this->mixerAgc) { SmGui::BeginDisabled(); }
                SmGui::LeftLabel("Mixer Gain");
                SmGui::FillWidth();
                if (SmGui::SliderInt(CONCAT("##_hydrasdr_mix_gain_", _this->name), &_this->mixerGain,
                                     _this->mixerGainMin, _this->mixerGainMax)) {
                    if (_this->running) {
                        hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_MIXER, _this->mixerGain);
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["mixerGain"] = _this->mixerGain;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Mixer stage gain");
                }
                if (_this->hasMixerAgc && _this->mixerAgc) { SmGui::EndDisabled(); }
            }

            // Filter Gain
            if (_this->hasFilterGain) {
                if (_this->hasFilterAgc && _this->filterAgc) { SmGui::BeginDisabled(); }
                SmGui::LeftLabel("Filter Gain");
                SmGui::FillWidth();
                if (SmGui::SliderInt(CONCAT("##_hydrasdr_filt_gain_", _this->name), &_this->filterGain,
                                     _this->filterGainMin, _this->filterGainMax)) {
                    if (_this->running) {
                        hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_FILTER, _this->filterGain);
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["filterGain"] = _this->filterGain;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("IF filter stage gain");
                }
                if (_this->hasFilterAgc && _this->filterAgc) { SmGui::EndDisabled(); }
            }

            // VGA Gain
            if (_this->hasVgaGain) {
                SmGui::LeftLabel("VGA Gain");
                SmGui::FillWidth();
                if (SmGui::SliderInt(CONCAT("##_hydrasdr_vga_gain_", _this->name), &_this->vgaGain,
                                     _this->vgaGainMin, _this->vgaGainMax)) {
                    if (_this->running) {
                        hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_VGA, _this->vgaGain);
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["vgaGain"] = _this->vgaGain;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Variable Gain Amplifier (final IF stage)");
                }
            }

            // AGC Control - only show if device supports AGC AND the corresponding gain is displayed
            if (_this->hasLnaAgc && _this->hasLnaGain) {
                SmGui::ForceSync();
                if (SmGui::Checkbox(CONCAT("LNA AGC##_hydrasdr_", _this->name), &_this->lnaAgc)) {
                    if (_this->running) {
                        if (_this->lnaAgc) {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_LNA_AGC, 1);
                        }
                        else {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_LNA_AGC, 0);
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_LNA, _this->lnaGain);
                        }
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["lnaAgc"] = _this->lnaAgc;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Automatic Gain Control for LNA stage");
                }
            }
            if (_this->hasRfAgc && _this->hasRfGain) {
                SmGui::ForceSync();
                if (SmGui::Checkbox(CONCAT("RF AGC##_hydrasdr_", _this->name), &_this->rfAgc)) {
                    if (_this->running) {
                        if (_this->rfAgc) {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_RF_AGC, 1);
                        }
                        else {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_RF_AGC, 0);
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_RF, _this->rfGain);
                        }
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["rfAgc"] = _this->rfAgc;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Automatic Gain Control for RF stage");
                }
            }
            if (_this->hasMixerAgc && _this->hasMixerGain) {
                SmGui::ForceSync();
                if (SmGui::Checkbox(CONCAT("Mixer AGC##_hydrasdr_", _this->name), &_this->mixerAgc)) {
                    if (_this->running) {
                        if (_this->mixerAgc) {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_MIXER_AGC, 1);
                        }
                        else {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_MIXER_AGC, 0);
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_MIXER, _this->mixerGain);
                        }
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["mixerAgc"] = _this->mixerAgc;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Automatic Gain Control for Mixer stage");
                }
            }
            if (_this->hasFilterAgc && _this->hasFilterGain) {
                SmGui::ForceSync();
                if (SmGui::Checkbox(CONCAT("Filter AGC##_hydrasdr_", _this->name), &_this->filterAgc)) {
                    if (_this->running) {
                        if (_this->filterAgc) {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_FILTER_AGC, 1);
                        }
                        else {
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_FILTER_AGC, 0);
                            hydrasdr_set_gain(_this->openDev, HYDRASDR_GAIN_TYPE_FILTER, _this->filterGain);
                        }
                    }
                    if (_this->selectedSerStr != "") {
                        config.acquire();
                        config.conf["devices"][_this->selectedSerStr]["filterAgc"] = _this->filterAgc;
                        config.release(true);
                    }
                }
                if (!_this->serverMode && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Automatic Gain Control for IF Filter stage");
                }
            }
        }

        // Bias T
        if (SmGui::Checkbox(CONCAT("Bias T##_hydrasdr_", _this->name), &_this->biasT)) {
            if (_this->running) {
                hydrasdr_set_rf_bias(_this->openDev, _this->biasT);
            }
            if (_this->selectedSerStr != "") {
                config.acquire();
                config.conf["devices"][_this->selectedSerStr]["biasT"] = _this->biasT;
                config.release(true);
            }
        }
        if (!_this->serverMode && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable DC bias voltage on antenna input (for active antennas/LNAs)");
        }
    }

    static int callback(hydrasdr_transfer_t* transfer) {
        HydraSDRSourceModule* _this = (HydraSDRSourceModule*)transfer->ctx;
        memcpy(_this->stream.writeBuf, transfer->samples, transfer->sample_count * sizeof(dsp::complex_t));
        if (!_this->stream.swap(transfer->sample_count)) { return -1; }
        return 0;
    }

    std::string name;
    hydrasdr_device* openDev;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    double sampleRate;
    SourceManager::SourceHandler handler;
    bool running = false;
    bool serverMode = false;
    double freq;
    uint64_t selectedSerial = 0;
    std::string selectedSerStr = "";
    int devId = 0;
    int srId = 0;
    int portId = 0;

    bool biasT = false;

    int lnaGain = 0;
    int rfGain = 0;
    int mixerGain = 0;
    int filterGain = 0;
    int vgaGain = 0;
    int linearGain = 0;
    int sensitiveGain = 0;

    int gainMode = 0;

    bool lnaAgc = false;
    bool rfAgc = false;
    bool mixerAgc = false;
    bool filterAgc = false;

#ifdef __ANDROID__
    int devFd = 0;
    int lastAndroidUsbHotplugGeneration = 0;
#endif

    OptionList<std::string, uint64_t> devices;
    OptionList<uint32_t, uint32_t> samplerates;
    OptionList<uint32_t, uint32_t> bandwidths;
    OptionList<std::string, hydrasdr_rf_port_t> ports;
    OptionList<std::string, int> algorithms;
    int algoId = 0;
    int bwId = 0;
    bool highDefMode = false;  // Decimation mode: false=Low Bandwidth, true=High Definition

    // Library version info
    hydrasdr_lib_version_t libVersion = {0, 0, 0};
    char sourceName[64] = "HydraSDR";  // Source name with version for dropdown display
    bool hasNewApi = false;  // true if lib version >= 1.1.0

    // Gain ranges (populated from device_info)
    uint8_t lnaGainMin = 0, lnaGainMax = 15;
    uint8_t rfGainMin = 0, rfGainMax = 15;
    uint8_t mixerGainMin = 0, mixerGainMax = 15;
    uint8_t filterGainMin = 0, filterGainMax = 15;
    uint8_t vgaGainMin = 0, vgaGainMax = 15;
    uint8_t linearGainMin = 0, linearGainMax = 21;
    uint8_t sensitiveGainMin = 0, sensitiveGainMax = 21;
    // Gain availability flags (updated from device capabilities and valid ranges)
    bool hasLnaGain = false, hasRfGain = false, hasMixerGain = false;
    bool hasFilterGain = false, hasVgaGain = false;
    bool hasLinearityGain = false, hasSensitivityGain = false;
    bool hasLnaAgc = false, hasRfAgc = false, hasMixerAgc = false, hasFilterAgc = false;

    // Temperature sensor (v1.1.0+)
    bool hasTemperatureSensor = false;
    float currentTemperature = 0.0f;
    bool temperatureValid = false;
    bool autoRefreshTemp = false;
    std::chrono::steady_clock::time_point lastTempUpdate;

    // Streaming statistics (v1.1.0+)
    uint64_t droppedBuffers = 0;
    uint64_t receivedBuffers = 0;
};

MOD_EXPORT void _INIT_() {
    json def = json({});
    def["devices"] = json({});
    def["device"] = "";
    config.setPath(core::args["root"].s() + "/hydrasdr_config.json");
    config.load(def);
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new HydraSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (HydraSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
