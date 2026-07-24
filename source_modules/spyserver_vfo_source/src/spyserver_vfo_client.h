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

        SpyServerDeviceInfo devInfo;

        // Used to decode incoming UINT8 FFT frames (see handleFFTFrame()
        // in the .cpp). Keep these in sync with whatever you send via
        // setSetting(SPYSERVER_SETTING_FFT_DB_OFFSET/_DB_RANGE, ...) -
        // main.cpp's UI does this whenever the sliders change.
        int fftDbOffset = -10;
        int fftDbRange = 100;

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
    };

    typedef std::unique_ptr<SpyServerVFOClientClass> SpyServerVFOClient;

    SpyServerVFOClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* iqOut, FFTHandler fftHandler, void* fftCtx);

}
