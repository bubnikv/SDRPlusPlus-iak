#include <spyserver_vfo_client.h>
#include <volk/volk.h>
#include <cstring>
#include <chrono>
#include <cmath>
#include <utils/flog.h>

using namespace std::chrono_literals;

namespace spyservervfo {
    SpyServerVFOClientClass::SpyServerVFOClientClass(net::Conn conn, dsp::stream<dsp::complex_t>* iqOut, FFTHandler fftHandler, void* fftCtx) {
        readBuf = new uint8_t[SPYSERVER_MAX_MESSAGE_BODY_SIZE];
        writeBuf = new uint8_t[SPYSERVER_MAX_MESSAGE_BODY_SIZE];
        client = std::move(conn);
        iqOutput = iqOut;
        fftHandlerCb = fftHandler;
        fftHandlerCtx = fftCtx;

        iqOutput->clearWriteStop();

        sendHandshake("SDR++ (VFO+FFT)");

        client->readAsync(sizeof(SpyServerMessageHeader), (uint8_t*)&receivedHeader, dataHandler, this);
    }

    SpyServerVFOClientClass::~SpyServerVFOClientClass() {
        close();
        delete[] readBuf;
        delete[] writeBuf;
    }

    void SpyServerVFOClientClass::startStream() {
        iqOutput->clearWriteStop();
        setSetting(SPYSERVER_SETTING_STREAMING_ENABLED, true);
    }

    void SpyServerVFOClientClass::stopStream() {
        iqOutput->stopWriter();
        setSetting(SPYSERVER_SETTING_STREAMING_ENABLED, false);
    }

    void SpyServerVFOClientClass::close() {
        iqOutput->stopWriter();
        client->close();
    }

    bool SpyServerVFOClientClass::isOpen() {
        return client->isOpen();
    }

    int SpyServerVFOClientClass::computeDigitalGain(int serverBits, int deviceGain, int decimationId) {
        if (devInfo.DeviceType == SPYSERVER_DEVICE_AIRSPY_ONE) {
            return (devInfo.MaximumGainIndex - deviceGain) + (decimationId * 3.01f);
        }
        else if (devInfo.DeviceType == SPYSERVER_DEVICE_AIRSPY_HF) {
            return decimationId * 3.01f;
        }
        else if (devInfo.DeviceType == SPYSERVER_DEVICE_RTLSDR) {
            return decimationId * 3.01f;
        }
        else {
            // Error, unknown device
            return -1;
        }
    }

    bool SpyServerVFOClientClass::waitForDevInfo(int timeoutMS) {
        std::unique_lock lck(deviceInfoMtx);
        auto now = std::chrono::system_clock::now();
        deviceInfoCnd.wait_until(lck, now + (timeoutMS * 1ms), [this]() { return deviceInfoAvailable; });
        return deviceInfoAvailable;
    }

    void SpyServerVFOClientClass::sendCommand(uint32_t command, void* data, int len) {
        std::lock_guard lck(writeMtx);
        SpyServerCommandHeader* hdr = (SpyServerCommandHeader*)writeBuf;
        hdr->CommandType = command;
        hdr->BodySize = len;
        memcpy(&writeBuf[sizeof(SpyServerCommandHeader)], data, len);
        client->write(sizeof(SpyServerCommandHeader) + len, writeBuf);
    }

    void SpyServerVFOClientClass::sendHandshake(std::string appName) {
        int totSize = sizeof(SpyServerClientHandshake) + appName.size();
        uint8_t* buf = new uint8_t[totSize];

        SpyServerClientHandshake* cmdHandshake = (SpyServerClientHandshake*)buf;
        cmdHandshake->ProtocolVersion = SPYSERVER_PROTOCOL_VERSION;

        memcpy(&buf[sizeof(SpyServerClientHandshake)], appName.c_str(), appName.size());
        sendCommand(SPYSERVER_CMD_HELLO, buf, totSize);

        delete[] buf;
    }

    void SpyServerVFOClientClass::setSetting(uint32_t setting, uint32_t arg) {
        SpyServerSettingTarget target;
        target.Setting = setting;
        target.Value = arg;
        sendCommand(SPYSERVER_CMD_SET_SETTING, &target, sizeof(SpyServerSettingTarget));
    }

    int SpyServerVFOClientClass::readSize(int count, uint8_t* buffer) {
        int read = 0;
        int len = 0;
        while (read < count) {
            len = client->read(count - read, &buffer[read]);
            if (len <= 0) { return len; }
            read += len;
        }
        return read;
    }

    void SpyServerVFOClientClass::handleFFTFrame(int mtype, int mflags, int bodySize) {
        if (!fftHandlerCb) { return; }

        // NOTE: SpyServer's on-the-wire FFT dB mapping is not officially
        // documented (the server binary is closed-source). The formula
        // below is a best-effort reconstruction: byte 0 -> (dbOffset -
        // dbRange), byte 255 -> dbOffset, using the same DB_OFFSET/DB_RANGE
        // values this client requested via setSetting(). If the waterfall
        // looks off (too hot/cold, clipped, inverted) once you're testing
        // against your own server, adjust the "FFT dB Offset"/"FFT dB
        // Range" sliders in the module UI until it looks right - those
        // values feed directly into this decode.
        if (mtype == SPYSERVER_MSG_TYPE_UINT8_FFT) {
            int count = bodySize / sizeof(uint8_t);
            fftConvBuf.resize(count);
            float range = (float)fftDbRange;
            float offset = (float)fftDbOffset;
            for (int i = 0; i < count; i++) {
                fftConvBuf[i] = offset - range * (1.0f - ((float)readBuf[i] / 255.0f));
            }
            fftHandlerCb(fftConvBuf.data(), count, fftHandlerCtx);
        }
        else if (mtype == SPYSERVER_MSG_TYPE_DINT4_FFT) {
            // 4-bit differential-coded FFT frames are not implemented -
            // the encoding isn't documented anywhere I could verify
            // without the server source. This client always requests
            // SPYSERVER_STREAM_FORMAT_UINT8 for the FFT stream (see
            // main.cpp), so in practice this branch should not be hit.
            // Left in place only so an unexpected server response doesn't
            // silently corrupt the display.
        }
    }

    void SpyServerVFOClientClass::dataHandler(int count, uint8_t* buf, void* ctx) {
        SpyServerVFOClientClass* _this = (SpyServerVFOClientClass*)ctx;

        if (count < sizeof(SpyServerMessageHeader)) {
            _this->readSize(sizeof(SpyServerMessageHeader) - count, &buf[count]);
        }

        int size = _this->readSize(_this->receivedHeader.BodySize, _this->readBuf);
        if (size <= 0) {
            printf("ERROR: Disconnected\n");
            return;
        }

        int mtype = _this->receivedHeader.MessageType & 0xFFFF;
        int mflags = (_this->receivedHeader.MessageType & 0xFFFF0000) >> 16;

        if (mtype == SPYSERVER_MSG_TYPE_DEVICE_INFO) {
            {
                std::lock_guard lck(_this->deviceInfoMtx);
                SpyServerDeviceInfo* _devInfo = (SpyServerDeviceInfo*)_this->readBuf;
                _this->devInfo = *_devInfo;
                _this->deviceInfoAvailable = true;
            }
            _this->deviceInfoCnd.notify_all();
        }
        else if (mtype == SPYSERVER_MSG_TYPE_UINT8_IQ) {
            int sampCount = _this->receivedHeader.BodySize / (sizeof(uint8_t) * 2);
            float gain = pow(10, (double)mflags / 20.0);
            float scale = 1.0f / (gain * 128.0f);
            for (int i = 0; i < sampCount; i++) {
                _this->iqOutput->writeBuf[i].re = ((float)_this->readBuf[(2 * i)] - 128.0f) * scale;
                _this->iqOutput->writeBuf[i].im = ((float)_this->readBuf[(2 * i) + 1] - 128.0f) * scale;
            }
            _this->iqOutput->swap(sampCount);
        }
        else if (mtype == SPYSERVER_MSG_TYPE_INT16_IQ) {
            int sampCount = _this->receivedHeader.BodySize / (sizeof(int16_t) * 2);
            float gain = pow(10, (double)mflags / 20.0);
            volk_16i_s32f_convert_32f((float*)_this->iqOutput->writeBuf, (int16_t*)_this->readBuf, 32768.0 * gain, sampCount * 2);
            _this->iqOutput->swap(sampCount);
        }
        else if (mtype == SPYSERVER_MSG_TYPE_INT24_IQ) {
            printf("ERROR: IQ format not supported\n");
            return;
        }
        else if (mtype == SPYSERVER_MSG_TYPE_FLOAT_IQ) {
            int sampCount = _this->receivedHeader.BodySize / sizeof(dsp::complex_t);
            float gain = pow(10, (double)mflags / 20.0);
            volk_32f_s32f_multiply_32f((float*)_this->iqOutput->writeBuf, (float*)_this->readBuf, gain, sampCount * 2);
            _this->iqOutput->swap(sampCount);
        }
        else if (mtype == SPYSERVER_MSG_TYPE_UINT8_FFT || mtype == SPYSERVER_MSG_TYPE_DINT4_FFT) {
            _this->handleFFTFrame(mtype, mflags, _this->receivedHeader.BodySize);
        }

        _this->client->readAsync(sizeof(SpyServerMessageHeader), (uint8_t*)&_this->receivedHeader, dataHandler, _this);
    }

    SpyServerVFOClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* iqOut, FFTHandler fftHandler, void* fftCtx) {
        net::Conn conn = net::connect(host, port);
        if (!conn) {
            return NULL;
        }
        return SpyServerVFOClient(new SpyServerVFOClientClass(std::move(conn), iqOut, fftHandler, fftCtx));
    }
}
