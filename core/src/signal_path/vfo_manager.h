#pragma once
#include "../dsp/channel/rx_vfo.h"
#include <gui/widgets/waterfall.h>
#include <utils/event.h>

class VFOManager {
public:
    VFOManager();

    class VFO {
    public:
        VFO(std::string name, int reference, double offset, double bandwidth, double sampleRate, double minBandwidth, double maxBandwidth, bool bandwidthLocked);
        ~VFO();

        void setOffset(double offset);
        double getOffset();
        // The reference-adjusted offset actually fed to the DSP mixer in
        // the normal (non-double-shifted) case: equals getOffset() for
        // REF_CENTER modes (AM, NFM, WFM...), but differs by half the VFO
        // bandwidth for REF_LOWER/REF_UPPER modes (USB, LSB...), since
        // their passband isn't centered on the tuned/clicked frequency.
        double getCenterOffset();
        void setCenterOffset(double offset);

        // Sets the DSP mixer's own offset directly, independent of the
        // visual/waterfall offset (wtfVFO, used for click math and
        // display). Exists for sources whose delivered IQ is already
        // centered on the visually-indicated frequency (e.g. a narrowband
        // stream retuned server-side to follow the VFO) - for those, the
        // normal setOffset() would double-apply the shift once server-side
        // and once again here locally. Call this with 0 in that case.
        void setDspOffset(double offset);
        void setBandwidth(double bandwidth, bool updateWaterfall = true);
        void setSampleRate(double sampleRate, double bandwidth);
        void setReference(int ref);
        void setSnapInterval(double interval);
        void setBandwidthLimits(double minBandwidth, double maxBandwidth, bool bandwidthLocked);
        bool getBandwidthChanged(bool erase = true);
        double getBandwidth();
        int getReference();
        void setColor(ImU32 color);
        std::string getName();

        dsp::stream<dsp::complex_t>* output;

        friend class VFOManager;

        dsp::channel::RxVFO* dspVFO;
        ImGui::WaterfallVFO* wtfVFO;

    private:
        std::string name;
        double _bandwidth;

    };

    VFOManager::VFO* createVFO(std::string name, int reference, double offset, double bandwidth, double sampleRate, double minBandwidth, double maxBandwidth, bool bandwidthLocked);
    void deleteVFO(VFOManager::VFO* vfo);

    void setOffset(std::string name, double offset);
    double getOffset(std::string name);
    double getCenterOffset(std::string name);
    void setDspOffset(std::string name, double offset);
    void setCenterOffset(std::string name, double offset);
    void setBandwidth(std::string name, double bandwidth, bool updateWaterfall = true);
    void setSampleRate(std::string name, double sampleRate, double bandwidth);
    void setReference(std::string name, int ref);
    void setBandwidthLimits(std::string name, double minBandwidth, double maxBandwidth, bool bandwidthLocked);
    bool getBandwidthChanged(std::string name, bool erase = true);
    double getBandwidth(std::string name);
    void setColor(std::string name, ImU32 color);
    std::string getName();
    int getReference(std::string name);
    bool vfoExists(std::string name);

    void updateFromWaterfall(ImGui::WaterFall* wtf);

    Event<VFOManager::VFO*> onVfoCreated;
    Event<VFOManager::VFO*> onVfoDelete;
    Event<std::string> onVfoDeleted;

private:
    std::map<std::string, VFO*> vfos;
};
