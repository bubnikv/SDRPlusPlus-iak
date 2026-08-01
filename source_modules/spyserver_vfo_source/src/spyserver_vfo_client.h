#pragma once
#include <utils/networking.h>
#include <spyserver_vfo_protocol.h>
#include <dsp/stream.h>
#include <dsp/types.h>
#include <vector>

namespace spyservervfo {

    // Called whenever a full FFT frame has been decoded from the server.
    // 'data' contains 'count' dB-scaled magnitude values, left to right
    // across the server's currently configured FFT span. The buffer is
    // only valid for the duration of the callback.
    typedef void (*FFTHandler)(const float* data, int count, void* ctx);

    class SpyServerVFOClientClass {
    public:
        SpyServerVFOClientClass(net::Conn conn, dsp::stream<dsp::complex_t>* iqOut, FFTHandler fftHandler, void* fftCtx);
        ~SpyServerVFOClientClass();

        bool waitForDevInfo(int timeoutMS);

        void startStream();
        void stopStream();

        void setSetting(uint32_t setting, uint32_t arg);

        void close();
        bool isOpen();

        int computeDigitalGain(int serverBits, int deviceGain, int decimationId);

        // Reads and RESETS the accumulated raw-IQ level metrics for the Auto
        // digital-gain servo (main.cpp poll thread). 'peak' is the per-component
        // maximum as a fraction of full scale (0..1), 'railedFrac' the fraction
        // of components sitting on the integer rail (0..1). Returns false when no
        // integer IQ has been measured since the last read (Float32 stream or
        // idle) - the servo then leaves the gain untouched.
        bool readIqMeter(float& peak, float& railedFrac);

        SpyServerDeviceInfo devInfo;
        uint32_t canControl = 1;

        // Used to decode incoming UINT8 FFT frames (see handleFFTFrame()
        // in the .cpp). Keep these in sync with whatever you send via
        // setSetting(SPYSERVER_SETTING_FFT_DB_OFFSET/_DB_RANGE, ...) -
        // main.cpp's UI does this whenever the sliders change.
        int fftDbOffset = 0;
        int fftDbRange = 150;

    private:
        void sendCommand(uint32_t command, void* data, int len);
        void sendHandshake(std::string appName);

        int readSize(int count, uint8_t* buffer);

        static void dataHandler(int count, uint8_t* buf, void* ctx);
        void handleFFTFrame(int mtype, int mflags, int bodySize);

        net::Conn client;

        uint8_t* readBuf;
        uint8_t* writeBuf;
        std::mutex writeMtx; // guards writeBuf + the command write - setSetting() can now be called from more than one thread

        bool deviceInfoAvailable = false;
        std::mutex deviceInfoMtx;
        std::condition_variable deviceInfoCnd;

        SpyServerMessageHeader receivedHeader;

        dsp::stream<dsp::complex_t>* iqOutput;

        FFTHandler fftHandlerCb;
        void* fftHandlerCtx;
        std::vector<float> fftConvBuf; // scratch buffer for FFT dB conversion

        // Raw-IQ level meter, filled per IQ message on the network thread and
        // drained by the poll thread via readIqMeter(). Only UInt8/Int16 update
        // it; Float32 never does (nothing clips there).
        void updateIqMeter(float peak, uint64_t railed, uint64_t total);
        std::mutex meterMtx;
        float    meterPeak = 0.0f;
        uint64_t meterRailed = 0;
        uint64_t meterTotal = 0;
    };

    typedef std::unique_ptr<SpyServerVFOClientClass> SpyServerVFOClient;

    SpyServerVFOClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* iqOut, FFTHandler fftHandler, void* fftCtx);

}
