#include <signal_path/sink.h>
#include <utils/flog.h>
#include <imgui/imgui.h>
#include <gui/style.h>
#include <gui/icons.h>

#include <core.h>

#define CONCAT(a, b) ((std::string(a) + b).c_str())

SinkManager::SinkManager() {
    SinkManager::SinkProvider prov;
    prov.create = SinkManager::NullSink::create;
    registerSinkProvider("None", prov);
}

SinkManager::Stream::Stream(dsp::stream<dsp::stereo_t>* in, EventHandler<float>* srChangeHandler, float sampleRate) {
    init(in, srChangeHandler, sampleRate);
}

void SinkManager::Stream::init(dsp::stream<dsp::stereo_t>* in, EventHandler<float>* srChangeHandler, float sampleRate) {
    _in = in;
    srChange.bindHandler(srChangeHandler);
    _sampleRate = sampleRate;
    splitter.init(_in);
    splitter.bindStream(&volumeInput);
    volumeAjust.init(&volumeInput, 1.0f, false);
    sinkOut = &volumeAjust.out;
}

void SinkManager::Stream::start() {
    if (running) {
        return;
    }

    splitter.start();
    volumeAjust.start();
    sink->start();
    running = true;
}

void SinkManager::Stream::stop() {
    if (!running) {
        return;
    }
    splitter.stop();
    volumeAjust.stop();
    sink->stop();
    running = false;
}

void SinkManager::Stream::setVolume(float volume) {
    guiVolume = volume;
    volumeAjust.setVolume(volume);
}

float SinkManager::Stream::getVolume() {
    return guiVolume;
}

float SinkManager::Stream::getSampleRate() {
    return _sampleRate;
}

void SinkManager::Stream::setInput(dsp::stream<dsp::stereo_t>* in) {
    std::lock_guard<std::mutex> lck(ctrlMtx);
    _in = in;
    splitter.setInput(_in);
}

dsp::stream<dsp::stereo_t>* SinkManager::Stream::bindStream() {
    dsp::stream<dsp::stereo_t>* stream = new dsp::stream<dsp::stereo_t>;
    splitter.bindStream(stream);
    return stream;
}

void SinkManager::Stream::unbindStream(dsp::stream<dsp::stereo_t>* stream) {
    splitter.unbindStream(stream);
    delete stream;
}

void SinkManager::Stream::setSampleRate(float sampleRate) {
    std::lock_guard<std::mutex> lck(ctrlMtx);
    _sampleRate = sampleRate;
    srChange.emit(sampleRate);
}

void SinkManager::registerSinkProvider(std::string name, SinkProvider provider) {
    if (providers.find(name) != providers.end()) {
        flog::error("Cannot register sink provider '{0}', this name is already taken", name);
        return;
    }

    // Add the provider to the lists
    providers[name] = provider;
    providerNames.push_back(name);

    // Recreatd the text list for the menu
    refreshProviders();

    // Update the IDs of every stream
    for (auto& [streamName, stream] : streams) {
        stream->providerId = std::distance(providerNames.begin(), std::find(providerNames.begin(), providerNames.end(), stream->providerName));
    }

    onSinkProviderRegistered.emit(name);
}

void SinkManager::unregisterSinkProvider(std::string name) {
    if (providers.find(name) == providers.end()) {
        flog::error("Cannot unregister sink provider '{0}', no such provider exists.", name);
        return;
    }

    onSinkProviderUnregister.emit(name);

    // Switch all sinks using it to a null sink
    for (auto& [streamName, stream] : streams) {
        if (providerNames[stream->providerId] != name) { continue; }
        setStreamSink(streamName, "None");
    }

    // Erase the provider from the lists
    providers.erase(name);
    providerNames.erase(std::find(providerNames.begin(), providerNames.end(), name));

    // Recreatd the text list for the menu
    refreshProviders();

    // Update the IDs of every stream
    for (auto& [streamName, stream] : streams) {
        stream->providerId = std::distance(providerNames.begin(), std::find(providerNames.begin(), providerNames.end(), stream->providerName));
    }

    onSinkProviderUnregistered.emit(name);
}

void SinkManager::registerStream(std::string name, SinkManager::Stream* stream) {
    if (streams.find(name) != streams.end()) {
        flog::error("Cannot register stream '{0}', this name is already taken", name);
        return;
    }

    SinkManager::SinkProvider provider;

    provider = providers["None"];

    stream->sink = provider.create(stream, name, provider.ctx);
    stream->providerId = std::distance(providerNames.begin(), std::find(providerNames.begin(), providerNames.end(), "None"));
    stream->providerName = "None";

    streams[name] = stream;
    streamNames.push_back(name);

    // Load config
    loadStreamConfig(name);

    onStreamRegistered.emit(name);
}

void SinkManager::unregisterStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        flog::error("Cannot unregister stream '{0}', this stream doesn't exist", name);
        return;
    }
    onStreamUnregister.emit(name);
    SinkManager::Stream* stream = streams[name];
    stream->stop();
    delete stream->sink;
    streams.erase(name);
    streamNames.erase(std::remove(streamNames.begin(), streamNames.end(), name), streamNames.end());
    onStreamUnregistered.emit(name);
}

void SinkManager::startStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        flog::error("Cannot start stream '{0}', this stream doesn't exist", name);
        return;
    }
    streams[name]->start();
}

void SinkManager::stopStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        flog::error("Cannot stop stream '{0}', this stream doesn't exist", name);
        return;
    }
    streams[name]->stop();
}

float SinkManager::getStreamSampleRate(std::string name) {
    if (streams.find(name) == streams.end()) {
        flog::error("Cannot get sample rate of stream '{0}', this stream doesn't exist", name);
        return -1.0f;
    }
    return streams[name]->getSampleRate();
}

dsp::stream<dsp::stereo_t>* SinkManager::bindStream(std::string name) {
    if (streams.find(name) == streams.end()) {
        flog::error("Cannot bind to stream '{0}'. Stream doesn't exist", name);
        return NULL;
    }
    return streams[name]->bindStream();
}

void SinkManager::unbindStream(std::string name, dsp::stream<dsp::stereo_t>* stream) {
    if (streams.find(name) == streams.end()) {
        flog::error("Cannot unbind from stream '{0}'. Stream doesn't exist", name);
        return;
    }
    streams[name]->unbindStream(stream);
}

void SinkManager::setStreamSink(std::string name, std::string providerName) {
    if (streams.find(name) == streams.end()) {
        flog::error("Cannot set sink for stream '{0}'. Stream doesn't exist", name);
        return;
    }
    Stream* stream = streams[name];
    if (providers.find(providerName) == providers.end()) {
        flog::error("Unknown sink provider '{0}'", providerName);
        return;
    }

    if (stream->running) {
        stream->sink->stop();
    }
    delete stream->sink;
    stream->providerId = std::distance(providerNames.begin(), std::find(providerNames.begin(), providerNames.end(), providerName));
    stream->providerName = providerName;
    SinkManager::SinkProvider prov = providers[providerName];
    stream->sink = prov.create(stream, name, prov.ctx);
    if (stream->running) {
        stream->sink->start();
    }
}

void SinkManager::showVolumeSlider(std::string name, std::string prefix, float width, float btnHeight, int btnBorder, bool sameLine) {
    // TODO: Replace map with some hashmap for it to be faster
    float height = ImGui::GetTextLineHeightWithSpacing() + 2;
    if (btnHeight > 0) {
        height = btnHeight;
    }
    // The mute button is drawn as a `height`-sized glyph inside `btnBorder`
    // padding, so it occupies more than `height`. Budgeting with `height` alone
    // made the group overrun its requested width by 2 * btnBorder, shifting
    // everything after it in the top bar.
    float btnFootprint = height + 2.0f * (float)btnBorder;
    // The slider's own height, not a stand-in: GetTextLineHeightWithSpacing()+2
    // happens to equal it under the desktop style (ItemSpacing.y 4 + 2 ==
    // 2 * FramePadding.y 3) but is 13 px short under the touch style, which
    // pushed the slider off the button's centre line on Android. Centre it in
    // whichever of the two is taller -- in the menu (no button border) the
    // slider is the taller one, and it must not be pulled above the row.
    float sliderHeight = ImGui::GetFrameHeight();
    float sliderYOfs   = std::max(btnFootprint - sliderHeight, 0.0f) / 2.0f;

    float ypos = ImGui::GetCursorPosY();
    float sliderOffset = ImGui::GetStyle().ItemSpacing.x; // SameLine() gap between button and slider
    float minSliderWidth = style::dp(80.0f);
    width = std::max(width, btnFootprint + sliderOffset + minSliderWidth);

    if (streams.find(name) == streams.end() || name == "") {
        float dummy = 0.0f;
        style::beginDisabled();
        ImGui::PushID(ImGui::GetID(("sdrpp_unmute_btn_" + name).c_str()));
        ImGui::ImageButton(icons::MUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), btnBorder, ImVec4(0, 0, 0, 0), ImGui::GetStyleColorVec4(ImGuiCol_Text));
        ImGui::PopID();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(std::max(width - btnFootprint - sliderOffset, minSliderWidth));
        ImGui::SetCursorPosY(ypos + sliderYOfs);
        ImGui::SliderFloat((prefix + name).c_str(), &dummy, 0.0f, 1.0f, "");
        style::endDisabled();
        if (sameLine) { ImGui::SetCursorPosY(ypos); }
        return;
    }

    SinkManager::Stream* stream = streams[name];

    if (stream->volumeAjust.getMuted()) {
        ImGui::PushID(ImGui::GetID(("sdrpp_unmute_btn_" + name).c_str()));
        if (ImGui::ImageButton(icons::MUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), btnBorder, ImVec4(0, 0, 0, 0), ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            stream->volumeAjust.setMuted(false);
            saveStreamConfig(name);
        }
        ImGui::PopID();
    }
    else {
        ImGui::PushID(ImGui::GetID(("sdrpp_mute_btn_" + name).c_str()));
        if (ImGui::ImageButton(icons::UNMUTED, ImVec2(height, height), ImVec2(0, 0), ImVec2(1, 1), btnBorder, ImVec4(0, 0, 0, 0), ImGui::GetStyleColorVec4(ImGuiCol_Text))) {
            stream->volumeAjust.setMuted(true);
            saveStreamConfig(name);
        }
        ImGui::PopID();
    }

    ImGui::SameLine();

    ImGui::SetNextItemWidth(std::max(width - btnFootprint - sliderOffset, minSliderWidth));
    ImGui::SetCursorPosY(ypos + sliderYOfs);
    if (ImGui::SliderFloat((prefix + name).c_str(), &stream->guiVolume, 0.0f, 1.0f, "")) {
        stream->setVolume(stream->guiVolume);
        saveStreamConfig(name);
    }
    if (sameLine) { ImGui::SetCursorPosY(ypos); }
    //ImGui::SetCursorPosY(ypos);
}

void SinkManager::loadStreamConfig(std::string name) {
    // Read the stored block under its own lock and let go of it before applying:
    // creating and starting a sink runs module code, which reaches for the config
    // itself and would deadlock on the non-recursive lock.
    json conf;
    {
        auto txn = core::configManager.transaction();
        conf = txn.section("streams").value(name, json());
    }
    if (!conf.is_object()) { return; }

    SinkManager::Stream* stream = streams[name];
    std::string provName = conf.value("sink", providerNames[0]);
    if (providers.find(provName) == providers.end()) {
        provName = providerNames[0];
    }
    if (stream->running) {
        stream->sink->stop();
    }
    delete stream->sink;
    SinkManager::SinkProvider prov = providers[provName];
    stream->providerId = std::distance(providerNames.begin(), std::find(providerNames.begin(), providerNames.end(), provName));
    stream->providerName = provName;
    stream->sink = prov.create(stream, name, prov.ctx);
    if (stream->running) {
        stream->sink->start();
    }
    stream->setVolume(conf.value("volume", stream->getVolume()));
    stream->volumeAjust.setMuted(conf.value("muted", false));
}

void SinkManager::saveStreamConfig(std::string name) {
    SinkManager::Stream* stream = streams[name];
    json conf;
    conf["sink"] = providerNames[stream->providerId];
    conf["volume"] = stream->getVolume();
    conf["muted"] = stream->volumeAjust.getMuted();
    auto txn = core::configManager.transaction();
    txn.section("streams").set(name, conf);
}

void SinkManager::loadSinksFromConfig() {
    for (auto const& [name, stream] : streams) {
        loadStreamConfig(name);
    }
}

void SinkManager::showMenu() {
    float menuWidth = ImGui::GetContentRegionAvail().x;
    int count = 0;
    int maxCount = streams.size();

    std::string provStr = "";
    for (auto const& name : providerNames) {
        provStr += name;
        provStr += '\0';
    }

    for (auto const& [name, stream] : streams) {
        ImGui::SetCursorPosX((menuWidth / 2.0f) - (ImGui::CalcTextSize(name.c_str()).x / 2.0f));
        ImGui::Text("%s", name.c_str());

        ImGui::SetNextItemWidth(menuWidth);
        if (ImGui::Combo(CONCAT("##_sdrpp_sink_select_", name), &stream->providerId, provStr.c_str())) {
            setStreamSink(name, providerNames[stream->providerId]);
            saveStreamConfig(name);
        }

        stream->sink->menuHandler();

        showVolumeSlider(name, "##_sdrpp_sink_menu_vol_", menuWidth);

        count++;
        if (count < maxCount) {
            ImGui::Spacing();
            ImGui::Separator();
        }
        ImGui::Spacing();
    }
}

std::vector<std::string> SinkManager::getStreamNames() {
    return streamNames;
}

void SinkManager::refreshProviders() {
    providerNamesTxt.clear();
    for (auto& provName : providerNames) {
        providerNamesTxt += provName;
        providerNamesTxt += '\0';
    }
}
