#include <utils/flog.h>
#include <module.h>
#include <gui/gui.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <gui/style.h>
#include <gui/smgui.h>
#include <iio.h>
#include <ad9361.h>
#include <utils/optionlist.h>
#include <algorithm>
#include <atomic>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "plutosdr_source",
    /* Description:     */ "PlutoSDR source module for SDR++",
    /* Author:          */ "Ryzerth;F5OEO",
    /* Version:         */ 0, 2, 1,
    /* Max instances    */ 1
};

ConfigManager config;

const std::vector<const char*> deviceWhiteList = {
    "PlutoSDR",
    "ANTSDR",
    "LibreSDR",
    "Pluto+",
    "ad9361",
    "FISH"
};

class PlutoSDRSourceModule : public ModuleManager::Instance {
public:
    PlutoSDRSourceModule(std::string name) {
        this->name = name;

        // Define valid samplerates
        for (int sr = 1000000; sr <= 61440000; sr += 500000) {
            samplerates.define(sr, getBandwdithScaled(sr), sr);
        }
        samplerates.define(61440000, getBandwdithScaled(61440000.0), 61440000.0);

        // Define valid bandwidths
        bandwidths.define(0, "Auto", 0);
        for (int bw = 1000000.0; bw <= 52000000; bw += 500000) {
            bandwidths.define(bw, getBandwdithScaled(bw), bw);
        }

        // Define gain modes
        gainModes.define("manual", "Manual", "manual");
        gainModes.define("fast_attack", "Fast Attack", "fast_attack");
        gainModes.define("slow_attack", "Slow Attack", "slow_attack");
        gainModes.define("hybrid", "Hybrid", "hybrid");

        // Define RF inputs (second input on Pluto+, ANTSDR, LibreSDR RevC)
        rfInputs.define("rx1", "RX 1", "rx1");
        rfInputs.define("rx2", "RX 2", "rx2");

        // Enumerate devices
        refresh();

        // Select device
        devDesc = config.value("device", std::string());
        select(devDesc);

        // Register source
        handler.ctx = this;
        handler.selectHandler = menuSelected;
        handler.deselectHandler = menuDeselected;
        handler.menuHandler = menuHandler;
        handler.startHandler = start;
        handler.stopHandler = stop;
        handler.tuneHandler = tune;
        handler.stream = &stream;
        sigpath::sourceManager.registerSource("PlutoSDR", &handler);
    }

    ~PlutoSDRSourceModule() {
        stop(this);
        sigpath::sourceManager.unregisterSource("PlutoSDR");
    }

    void postInit() {}

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = true;
    }

    bool isEnabled() {
        return enabled;
    }

private:
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

    void refresh() {
        // Clear device list
        devices.clear();

        // Create scan context (scan both the USB and network backends)
        iio_scan_context* sctx = iio_create_scan_context("usb:ip", 0);
        if (!sctx) {
            flog::error("Failed get scan context");
            return;
        }

        // Enumerate devices
        iio_context_info** ctxInfoList;
        ssize_t count = iio_scan_context_get_info_list(sctx, &ctxInfoList);
        if (count < 0) {
            flog::error("Failed to enumerate contexts");
            return;
        }
        for (ssize_t i = 0; i < count; i++) {
            iio_context_info* info = ctxInfoList[i];
            std::string desc = iio_context_info_get_description(info);
            std::string duri = iio_context_info_get_uri(info);

            // If the device is not a plutosdr, don't include it
            bool isPluto = false;
            for (const auto type : deviceWhiteList) {
                if (desc.find(type) != std::string::npos) {
                    isPluto = true;
                    break;
                }
            }
            if (!isPluto) {
                flog::warn("Ignored IIO device: [{}] {}", duri, desc);
                continue;
            }

            // Use the full description as the device name, it identifies the exact board and firmware
            std::string devName = desc;

            // Skip duplicate devices
            if (devices.keyExists(desc) || devices.nameExists(devName) || devices.valueExists(duri)) { continue; }

            // Save device
            devices.define(desc, devName, duri);
        }
        iio_context_info_list_free(ctxInfoList);
        
        // Destroy scan context
        iio_scan_context_destroy(sctx);

#ifdef __ANDROID__
        // On Android, a default IP entry must be made (TODO: This is not ideal since the IP cannot be changed)
        const char* androidURI = "ip:192.168.2.1";
        const char* androidName = "Default (192.168.2.1)";
        devices.define(androidName, androidName, androidURI);
#endif
    }

    void select(const std::string& desc) {
        // If no device is available, give up
        if (devices.empty()) {
            devDesc.clear();
            return;
        }

        // If the device is not available, select the first one
        if (!devices.keyExists(desc)) {
            select(devices.key(0));
        }

        // Update URI
        devDesc = desc;
        uri = devices.value(devices.keyId(desc));

        // TODO: Enumerate capabilities

        // Load defaults
        samplerate = 4000000;
        bandwidth = 0;
        gmId = 0;
        gain = -1.0f;
        rfId = 0;

        // Load device config
        {
            auto txn = config.transaction();
            ConfigManager::Section dev = txn.section("devices", devDesc);
            dev.tryGet("samplerate", samplerate);
            dev.tryGet("bandwidth", bandwidth);

            // Select given gain mode or default if invalid
            std::string gm;
            if (dev.tryGet("gainMode", gm) && gainModes.keyExists(gm)) {
                gmId = gainModes.keyId(gm);
            }

            if (dev.tryGet("gain", gain)) {
                gain = std::clamp<int>(gain, -1.0f, 73.0f);
            }

            // Select given RF input or default if invalid
            std::string rf;
            if (dev.tryGet("rfInput", rf) && rfInputs.keyExists(rf)) {
                rfId = rfInputs.keyId(rf);
            }
        }

        // Update samplerate ID
        if (samplerates.keyExists(samplerate)) {
            srId = samplerates.keyId(samplerate);
        }
        else {
            srId = 0;
            samplerate = samplerates.value(srId);
        }

        // Update bandwidth ID
        if (bandwidths.keyExists(bandwidth)) {
            bwId = bandwidths.keyId(bandwidth);
        }
        else {
            bwId = 0;
            bandwidth = bandwidths.value(bwId);
        }
    }

    static void menuSelected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        core::setInputSampleRate(_this->samplerate);
        flog::info("PlutoSDRSourceModule '{0}': Menu Select!", _this->name);
    }

    static void menuDeselected(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        flog::info("PlutoSDRSourceModule '{0}': Menu Deselect!", _this->name);
    }

    static void start(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (_this->running) { return; }

        // If no device is selected, give up
        if (_this->devDesc.empty() || _this->uri.empty()) { return; }

        // Open context
        _this->ctx = iio_create_context_from_uri(_this->uri.c_str());
        if (_this->ctx == NULL) {
            flog::error("Could not open pluto ({})", _this->uri);
            return;
        }

        // Get phy and device handle
        _this->phy = iio_context_find_device(_this->ctx, "ad9361-phy");
        if (_this->phy == NULL) {
            flog::error("Could not connect to pluto phy");
            iio_context_destroy(_this->ctx);
            return;
        }
        _this->dev = iio_context_find_device(_this->ctx, "cf-ad9361-lpc");
        if (_this->dev == NULL) {
            flog::error("Could not connect to pluto dev");
            iio_context_destroy(_this->ctx);
            return;
        }

        // Get RX channels, routing the selected RF input (RX2 exists on Pluto+, ANTSDR, LibreSDR RevC)
        long long twoRxMode = 0;
        iio_device_debug_attr_read_longlong(_this->phy, "adi,2rx-2tx-mode-enable", &twoRxMode);
        if (twoRxMode == 1) {
            // In 2RX mode both inputs are always active, just pick the matching PHY channel
            _this->rxChan = iio_device_find_channel(_this->phy, (_this->rfId == 1) ? "voltage1" : "voltage0", false);
        }
        else {
            // In 1RX mode, route the selected input to the single RX chain (AD9361 reg 0x3 bits [7:6])
            uint32_t val = 0;
            if (!iio_device_reg_read(_this->phy, 0x00000003, &val)) {
                val = (val & 0x3F) | ((_this->rfId + 1) << 6);
                iio_device_reg_write(_this->phy, 0x00000003, val);
                iio_device_debug_attr_write_longlong(_this->phy, "adi,1rx-1tx-mode-use-rx-num", _this->rfId + 1);
            }
            _this->rxChan = iio_device_find_channel(_this->phy, "voltage0", false);
        }
        _this->rxLO = iio_device_find_channel(_this->phy, "altvoltage0", true);

        // Enable RX LO and disable TX
        iio_channel_attr_write_bool(iio_device_find_channel(_this->phy, "altvoltage1", true), "powerdown", true);
        iio_channel_attr_write_bool(_this->rxLO, "powerdown", false);

        // Configure RX channel
        iio_channel_attr_write(_this->rxChan, "rf_port_select", "A_BALANCED");
        iio_channel_attr_write_longlong(_this->rxLO, "frequency", round(_this->freq));                              // Freq
        iio_channel_attr_write_bool(_this->rxChan, "filter_fir_en", true);                                          // Digital filter
        iio_channel_attr_write_longlong(_this->rxChan, "sampling_frequency", round(_this->samplerate));             // Sample rate
        iio_channel_attr_write_double(_this->rxChan, "hardwaregain", _this->gain);                                  // Gain
        iio_channel_attr_write(_this->rxChan, "gain_control_mode", _this->gainModes.value(_this->gmId).c_str());    // Gain mode
        _this->setBandwidth(_this->bandwidth);
        
        // Configure the ADC filters
        ad9361_set_bb_rate(_this->phy, round(_this->samplerate));

        // Start worker thread
        _this->underflow = false;
        _this->overload = false;
        _this->running = true;
        _this->workerThread = std::thread(worker, _this);
        flog::info("PlutoSDRSourceModule '{0}': Start!", _this->name);
    }

    static void stop(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        if (!_this->running) { return; }

        // Stop worker thread
        _this->running = false;
        _this->stream.stopWriter();
        _this->workerThread.join();
        _this->stream.clearWriteStop();

        // Close device
        if (_this->ctx != NULL) {
            iio_context_destroy(_this->ctx);
            _this->ctx = NULL;
        }

        flog::info("PlutoSDRSourceModule '{0}': Stop!", _this->name);
    }

    static void tune(double freq, void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;
        _this->freq = freq;
        if (_this->running) {
            // Tune device
            iio_channel_attr_write_longlong(_this->rxLO, "frequency", round(freq));
        }
        flog::info("PlutoSDRSourceModule '{0}': Tune: {1}!", _this->name, freq);
    }

    static void menuHandler(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;

        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo("##plutosdr_dev_sel", &_this->devId, _this->devices.txt)) {
            _this->select(_this->devices.key(_this->devId));
            core::setInputSampleRate(_this->samplerate);
            config.set("device", _this->devices.key(_this->devId));
        }

        if (SmGui::Combo(CONCAT("##_pluto_sr_", _this->name), &_this->srId, _this->samplerates.txt)) {
            _this->samplerate = _this->samplerates.value(_this->srId);
            core::setInputSampleRate(_this->samplerate);
            if (!_this->devDesc.empty()) {
                config.transaction().section("devices", _this->devDesc).set("samplerate", _this->samplerate);
            }
        }

        // Refresh button
        SmGui::SameLine();
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Button(CONCAT("Refresh##_pluto_refr_", _this->name))) {
            _this->refresh();
            _this->select(_this->devDesc);
            core::setInputSampleRate(_this->samplerate);
        }
        if (_this->running) { SmGui::EndDisabled(); }

        SmGui::LeftLabel("Bandwidth");
        SmGui::FillWidth();
        if (SmGui::Combo(CONCAT("##_pluto_bw_", _this->name), &_this->bwId, _this->bandwidths.txt)) {
            _this->bandwidth = _this->bandwidths.value(_this->bwId);
            if (_this->running) {
                _this->setBandwidth(_this->bandwidth);
            }
            if (!_this->devDesc.empty()) {
                config.transaction().section("devices", _this->devDesc).set("bandwidth", _this->bandwidth);
            }
        }

        SmGui::LeftLabel("Gain Mode");
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_pluto_gainmode_select_", _this->name), &_this->gmId, _this->gainModes.txt)) {
            if (_this->running) {
                iio_channel_attr_write(_this->rxChan, "gain_control_mode", _this->gainModes.value(_this->gmId).c_str());
            }
            if (!_this->devDesc.empty()) {
                config.transaction().section("devices", _this->devDesc).set("gainMode", _this->gainModes.key(_this->gmId));
            }
        }

        SmGui::LeftLabel("Gain");
        if (_this->gmId) { SmGui::BeginDisabled(); }
        SmGui::FillWidth();
        if (SmGui::SliderFloatWithSteps(CONCAT("##_pluto_gain__", _this->name), &_this->gain, -1.0f, 73.0f, 1.0f, SmGui::FMT_STR_FLOAT_DB_NO_DECIMAL)) {
            if (_this->running) {
                iio_channel_attr_write_double(_this->rxChan, "hardwaregain", _this->gain);
            }
            if (!_this->devDesc.empty()) {
                config.transaction().section("devices", _this->devDesc).set("gain", _this->gain);
            }
        }
        if (_this->gmId) { SmGui::EndDisabled(); }

        // RF input selection (only effective on hardware with a second RX input)
        if (_this->running) { SmGui::BeginDisabled(); }
        SmGui::LeftLabel("RF Input");
        SmGui::FillWidth();
        SmGui::ForceSync();
        if (SmGui::Combo(CONCAT("##_pluto_rfinput_", _this->name), &_this->rfId, _this->rfInputs.txt)) {
            if (!_this->devDesc.empty()) {
                config.transaction().section("devices", _this->devDesc).set("rfInput", _this->rfInputs.key(_this->rfId));
            }
        }
        if (_this->running) { SmGui::EndDisabled(); }

        // Stream health indicators
        if (_this->running) {
            if (_this->underflow) {
                SmGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Buffer: underflow");
            }
            else {
                SmGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Buffer: nominal");
            }
            if (_this->overload) {
                SmGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Gain: overdrive");
            }
            else {
                SmGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Gain: OK");
            }
        }
    }

    void setBandwidth(int bw) {
        if (bw > 0) {
            iio_channel_attr_write_longlong(rxChan, "rf_bandwidth", bw);
        }
        else {
            iio_channel_attr_write_longlong(rxChan, "rf_bandwidth", std::min<int>(samplerate, 52000000));
        }
    }

    static void worker(void* ctx) {
        PlutoSDRSourceModule* _this = (PlutoSDRSourceModule*)ctx;

        // Use large buffers (50ms) to avoid underflows at high samplerates
        int blockSize = std::min<int>(_this->samplerate / 20, STREAM_BUFFER_SIZE);

        // Acquire channels (in 2RX mode, the second input streams on voltage2/voltage3)
        long long twoRxMode = 0;
        iio_device_debug_attr_read_longlong(_this->phy, "adi,2rx-2tx-mode-enable", &twoRxMode);
        bool useRx2Chain = (_this->rfId == 1) && (twoRxMode == 1);
        iio_channel* rx0_i = iio_device_find_channel(_this->dev, useRx2Chain ? "voltage2" : "voltage0", 0);
        iio_channel* rx0_q = iio_device_find_channel(_this->dev, useRx2Chain ? "voltage3" : "voltage1", 0);
        if (!rx0_i || !rx0_q) {
            flog::error("Failed to acquire RX channels");
            return;
        }

        // Start streaming
        iio_channel_enable(rx0_i);
        iio_channel_enable(rx0_q);

        // Allocate buffer
        iio_device_set_kernel_buffers_count(_this->dev, 8);
        iio_buffer* rxbuf = iio_device_create_buffer(_this->dev, blockSize, false);
        if (!rxbuf) {
            flog::error("Could not create RX buffer");
            return;
        }

        // Clear a possibly stale underflow flag in the DMA status register
        uint32_t status = 0;
        if (!iio_device_reg_read(_this->dev, 0x80000088, &status)) {
            iio_device_reg_write(_this->dev, 0x80000088, status);
        }

        // Receive loop
        while (true) {
            // Read samples
            iio_buffer_refill(rxbuf);

            // Check for DMA underflow (writing the status back clears the flag)
            if (!iio_device_reg_read(_this->dev, 0x80000088, &status)) {
                bool uf = (status & 4) != 0;
                if (uf) {
                    iio_device_reg_write(_this->dev, 0x80000088, status);
                    if (!_this->underflow) { flog::warn("PlutoSDRSourceModule '{0}': Underflow!", _this->name); }
                }
                _this->underflow = uf;
            }

            // Check for ADC overload
            if (!iio_device_reg_read(_this->phy, useRx2Chain ? 0x0000005F : 0x0000005E, &status)) {
                bool ovl = (status & 1) != 0;
                if (ovl && !_this->overload) { flog::warn("PlutoSDRSourceModule '{0}': Overdrive!", _this->name); }
                _this->overload = ovl;
            }

            // Get buffer pointer
            int16_t* buf = (int16_t*)iio_buffer_first(rxbuf, rx0_i);
            if (!buf) { break; }

            // Convert samples to CF32
            volk_16i_s32f_convert_32f((float*)_this->stream.writeBuf, buf, 32768.0f, blockSize * 2);

            // Send out the samples
            if (!_this->stream.swap(blockSize)) { break; };
        }

        // Stop streaming
        iio_channel_disable(rx0_i);
        iio_channel_disable(rx0_q);

        // Free buffer
        iio_buffer_destroy(rxbuf);
    }

    std::string name;
    bool enabled = true;
    dsp::stream<dsp::complex_t> stream;
    SourceManager::SourceHandler handler;
    std::thread workerThread;
    iio_context* ctx = NULL;
    iio_device* phy = NULL;
    iio_device* dev = NULL;
    iio_channel* rxLO = NULL;
    iio_channel* rxChan = NULL;
    bool running = false;

    std::string devDesc = "";
    std::string uri = "";

    double freq;
    int samplerate = 4000000;
    int bandwidth = 0;
    float gain = -1;

    int devId = 0;
    int srId = 0;
    int bwId = 0;
    int gmId = 0;
    int rfId = 0;
    std::atomic<bool> underflow{ false };
    std::atomic<bool> overload{ false };

    OptionList<std::string, std::string> devices;
    OptionList<int, double> samplerates;
    OptionList<int, double> bandwidths;
    OptionList<std::string, std::string> gainModes;
    OptionList<std::string, std::string> rfInputs;
};

MOD_EXPORT void _INIT_() {
    json defConf = {};
    defConf["device"] = "";
    defConf["devices"] = {};
    config.setPath(core::args["root"].s() + "/plutosdr_source_config.json");
    config.load(defConf);
    config.enableAutoSave();

    // Reset the configuration if the old format is still used
    {
        auto txn = config.transaction();
        if (!txn.contains("device") || !txn.contains("devices")) {
            txn.reset(defConf);
        }
    }
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new PlutoSDRSourceModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (PlutoSDRSourceModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}