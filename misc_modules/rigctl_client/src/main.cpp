#include <utils/proto/rigctl.h>
#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <gui/style.h>
#include <signal_path/signal_path.h>
#include <core.h>
#include <recorder_interface.h>
#include <meteor_demodulator_interface.h>
#include <config.h>
#include <cctype>
#include <radio_interface.h>
#define CONCAT(a, b) ((std::string(a) + b).c_str())

SDRPP_MOD_INFO{
    /* Name:            */ "rigctl_client",
    /* Description:     */ "Client for the RigCTL protocol",
    /* Author:          */ "Ryzerth",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ 1
};

ConfigManager config;

class RigctlClientModule : public ModuleManager::Instance {
public:
    RigctlClientModule(std::string name) {
        this->name = name;

        // Load default
        strcpy(host, "127.0.0.1");

        // Load config
        {
            auto configAccess = config.read();
            ConfigManager::ReadSection inst = configAccess.section(name);
            std::string h;
            if (inst.tryGet("host", h)) { strcpy(host, h.c_str()); }
            if (inst.tryGet("port", port)) { port = std::clamp<int>(port, 1, 65535); }
            inst.tryGet("ifFreq", ifFreq);
        }

        _retuneHandler.ctx = this;
        _retuneHandler.handler = retuneHandler;

        gui::menu.registerEntry(name, menuHandler, this, NULL);
    }

    ~RigctlClientModule() {
        stop();
        gui::menu.removeEntry(name);
    }

    void postInit() {
        
    }

    void enable() {
        enabled = true;
    }

    void disable() {
        enabled = false;
    }

    bool isEnabled() {
        return enabled;
    }

    void start() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (running) { return; }

        // Connect to rigctl server. Runs on the GUI thread, so bound DNS +
        // TCP connect instead of freezing the UI for the OS-level timeout.
        try {
            client = net::rigctl::connect(host, port, 5000);
        }
        catch (const std::exception& e) {
            flog::error("Could not connect: {}", e.what());
            return;
        }

        // Switch source to panadapter mode
        sigpath::sourceManager.setPanadapterIF(ifFreq);
        sigpath::sourceManager.setTuningMode(SourceManager::TuningMode::PANADAPTER);
        sigpath::sourceManager.onRetune.bindHandler(&_retuneHandler);

        running = true;
    }

    void stop() {
        std::lock_guard<std::recursive_mutex> lck(mtx);
        if (!running) { return; }

        // Switch source back to normal mode
        sigpath::sourceManager.onRetune.unbindHandler(&_retuneHandler);
        sigpath::sourceManager.setTuningMode(SourceManager::TuningMode::NORMAL);

        // Disconnect from rigctl server
        client->close();

        running = false;
    }

private:
    static void menuHandler(void* ctx) {
        RigctlClientModule* _this = (RigctlClientModule*)ctx;
        float menuWidth = ImGui::GetContentRegionAvail().x;

        if (_this->running) { style::beginDisabled(); }
        if (ImGui::InputText(CONCAT("##_rigctl_cli_host_", _this->name), _this->host, 1023)) {
            config.edit().section(_this->name).set("host", std::string(_this->host));
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(menuWidth - ImGui::GetCursorPosX());
        if (ImGui::InputInt(CONCAT("##_rigctl_cli_port_", _this->name), &_this->port, 0, 0)) {
            config.edit().section(_this->name).set("port", _this->port);
        }
        if (_this->running) { style::endDisabled(); }

        ImGui::LeftLabel("IF Frequency");
        ImGui::FillWidth();
        if (ImGui::InputDouble(CONCAT("##_rigctl_if_freq_", _this->name), &_this->ifFreq, 100.0, 100000.0, "%.0f")) {
            if (_this->running) {
                sigpath::sourceManager.setPanadapterIF(_this->ifFreq);
            }
            config.edit().section(_this->name).set("ifFreq", _this->ifFreq);
        }

        if (_this->running && ImGui::ActionButton(CONCAT("Stop##_rigctl_cli_stop_", _this->name))) {
            _this->stop();
        }
        else if (!_this->running && ImGui::ActionButton(CONCAT("Start##_rigctl_cli_stop_", _this->name))) {
            _this->start();
        }

        ImGui::TextUnformatted("Status:");
        ImGui::SameLine();
        if (_this->client && _this->client->isOpen() && _this->running) {
            ImGui::TextColored(ImVec4(0.0, 1.0, 0.0, 1.0), "Connected");
        }
        else if (_this->client && _this->running) {
            ImGui::TextColored(ImVec4(1.0, 1.0, 0.0, 1.0), "Disconnected");
        }
        else {
            ImGui::TextUnformatted("Idle");
        }
    }

    static void retuneHandler(double freq, void* ctx) {
        RigctlClientModule* _this = (RigctlClientModule*)ctx;
        if (!_this->client || !_this->client->isOpen()) { return; }
        if (_this->client->setFreq(freq)) {
            flog::error("Could not set frequency");
        }
    }

    std::string name;
    bool enabled = true;
    bool running = false;
    std::recursive_mutex mtx;

    char host[1024];
    int port = 4532;
    std::shared_ptr<net::rigctl::Client> client;

    double ifFreq = 8830000.0;

    EventHandler<double> _retuneHandler;
};

MOD_EXPORT void _INIT_() {
    config.setPath(core::args["root"].s() + "/rigctl_client_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new RigctlClientModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(void* instance) {
    delete (RigctlClientModule*)instance;
}

MOD_EXPORT void _END_() {
    config.shutdown();
}
