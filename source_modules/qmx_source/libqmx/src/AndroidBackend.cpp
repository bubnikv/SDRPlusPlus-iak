#ifdef __ANDROID__

#include "QmxDevice_internal.h"

#include <array>
#include <atomic>
#include <cinttypes>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <libusb.h>
#include <sys/time.h>

extern "C" {
    libusb_device* libusb_get_device2(libusb_context* ctx, const char* device_path);
    int libusb_open2(libusb_device* device, libusb_device_handle** handle, intptr_t fd);
}

namespace {
    float convert24(const std::uint8_t* data) {
        std::int32_t value = (static_cast<std::int32_t>(data[2]) << 24) |
                             (static_cast<std::int32_t>(data[1]) << 16) |
                             (static_cast<std::int32_t>(data[0]) << 8);
        value >>= 8;
        return static_cast<float>(value) / 8388608.0f;
    }
}

namespace qmx::detail {
    class AndroidUsbImpl : public DeviceImpl {
    public:
        ~AndroidUsbImpl() override {
            stop();
        }

        bool start(const StartOptions& options, StreamCallback callback, void* ctx, std::string& error) override {
            stop();

            if (!options.androidUsb.valid()) {
                error = "No authorized QMX USB device available on Android";
                return false;
            }
            if (!callback) {
                error = "No IQ callback supplied";
                return false;
            }

            int rc = libusb_init(&usbContext);
            if (rc < 0) {
                error = std::string("libusb_init failed: ") + libusb_error_name(rc);
                return false;
            }

            libusb_device* device = libusb_get_device2(usbContext, options.androidUsb.path.c_str());
            if (!device) {
                error = "Failed to resolve Android USB device path for QMX";
                cleanup();
                return false;
            }

            rc = libusb_open2(device, &usbHandle, static_cast<intptr_t>(options.androidUsb.fd));
            if (rc < 0) {
                error = std::string("Failed to open Android USB device: ") + libusb_error_name(rc);
                cleanup();
                return false;
            }

            for (int iface : { 0, 1, 2, 3, 4 }) {
                rc = libusb_claim_interface(usbHandle, iface);
                if (rc < 0) {
                    error = std::string("Failed to claim QMX USB interface: ") + libusb_error_name(rc);
                    cleanup();
                    return false;
                }
            }

            rc = libusb_set_interface_alt_setting(usbHandle, 3, 1);
            if (rc < 0) {
                error = std::string("Failed to select QMX audio alt setting: ") + libusb_error_name(rc);
                cleanup();
                return false;
            }

            callbackFn = callback;
            callbackCtx = ctx;
            pending.assign(kStreamBlockSize, {});
            pendingCount = 0;
            running.store(true);

            if (options.enableIqMode && !sendCatCommand("Q91;")) {
                error = "Failed to enable QMX IQ mode over USB CAT";
                cleanup();
                return false;
            }

            if (!prepareTransfers(error)) {
                cleanup();
                return false;
            }

            eventThread = std::thread(&AndroidUsbImpl::run, this);
            return true;
        }

        void stop() override {
            bool wasRunning = running.exchange(false);
            if (wasRunning && usbHandle) {
                sendCatCommand("Q90;");
            }

            for (auto* transfer : transfers) {
                if (transfer) {
                    libusb_cancel_transfer(transfer);
                }
            }

            if (eventThread.joinable()) {
                eventThread.join();
            }

            cleanup();
            pendingCount = 0;
        }

        bool isStreaming() const override {
            return running.load();
        }

        bool setFrequency(std::int64_t hz, std::string& error) override {
            char cmd[32];
            std::snprintf(cmd, sizeof(cmd), "FA%011" PRId64 ";", hz);
            if (!sendCatCommand(cmd)) {
                error = "Failed to send QMX USB CAT frequency command";
                return false;
            }
            return true;
        }

    private:
        static constexpr int kNumIsoTransfers = 5;
        static constexpr int kIsoPacketSize = 300;
        static constexpr int kNumIsoPackets = 20;

        static void transferCallback(libusb_transfer* transfer) {
            auto* self = static_cast<AndroidUsbImpl*>(transfer->user_data);
            if (self) {
                self->handleTransfer(transfer);
            }
        }

        bool prepareTransfers(std::string& error) {
            for (int i = 0; i < kNumIsoTransfers; ++i) {
                transfers[i] = libusb_alloc_transfer(kNumIsoPackets);
                if (!transfers[i]) {
                    error = "Failed to allocate QMX isochronous transfer";
                    return false;
                }

                libusb_fill_iso_transfer(
                    transfers[i],
                    usbHandle,
                    0x83,
                    transferBuffers[i].data(),
                    static_cast<int>(transferBuffers[i].size()),
                    kNumIsoPackets,
                    &AndroidUsbImpl::transferCallback,
                    this,
                    1000);
                libusb_set_iso_packet_lengths(transfers[i], kIsoPacketSize);

                int rc = libusb_submit_transfer(transfers[i]);
                if (rc < 0) {
                    error = std::string("Failed to submit QMX isochronous transfer: ") + libusb_error_name(rc);
                    return false;
                }
            }
            return true;
        }

        bool sendCatCommand(const std::string& command) {
            if (!usbHandle) {
                return false;
            }

            std::lock_guard<std::mutex> lock(catMutex);
            int transferred = 0;
            int rc = libusb_bulk_transfer(usbHandle, 0x01, reinterpret_cast<unsigned char*>(const_cast<char*>(command.data())), static_cast<int>(command.size()), &transferred, 100);
            return rc == 0 && transferred == static_cast<int>(command.size());
        }

        void push(float i, float q) {
            pending[pendingCount++] = { i, q };
            if (pendingCount == pending.size()) {
                callbackFn(pending.data(), pending.size(), callbackCtx);
                pendingCount = 0;
            }
        }

        void handleTransfer(libusb_transfer* transfer) {
            if (transfer->status != LIBUSB_TRANSFER_COMPLETED) {
                running.store(false);
                return;
            }

            for (int packet = 0; packet < transfer->num_iso_packets; ++packet) {
                libusb_iso_packet_descriptor* desc = &transfer->iso_packet_desc[packet];
                if (desc->status != LIBUSB_TRANSFER_COMPLETED || desc->actual_length <= 0) {
                    continue;
                }

                const std::uint8_t* data = libusb_get_iso_packet_buffer_simple(transfer, packet);
                int frames = desc->actual_length / 6;
                for (int i = 0; i < frames; ++i) {
                    push(convert24(data), convert24(data + 3));
                    data += 6;
                }
            }

            if (running.load()) {
                int rc = libusb_submit_transfer(transfer);
                if (rc < 0) {
                    running.store(false);
                }
            }
        }

        void run() {
            while (running.load()) {
                timeval timeout = { 0, 100000 };
                int rc = libusb_handle_events_timeout_completed(usbContext, &timeout, nullptr);
                if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_TIMEOUT) {
                    break;
                }
            }
        }

        void cleanup() {
            for (auto*& transfer : transfers) {
                if (transfer) {
                    libusb_free_transfer(transfer);
                    transfer = nullptr;
                }
            }

            if (usbHandle) {
                for (int iface : { 0, 1, 2, 3, 4 }) {
                    libusb_release_interface(usbHandle, iface);
                }
                libusb_close(usbHandle);
                usbHandle = nullptr;
            }
            if (usbContext) {
                libusb_exit(usbContext);
                usbContext = nullptr;
            }
        }

        libusb_context* usbContext = nullptr;
        libusb_device_handle* usbHandle = nullptr;
        std::array<std::array<unsigned char, kIsoPacketSize * kNumIsoPackets>, kNumIsoTransfers> transferBuffers{};
        std::array<libusb_transfer*, kNumIsoTransfers> transfers{};
        StreamCallback callbackFn = nullptr;
        void* callbackCtx = nullptr;
        std::vector<IQSample> pending;
        std::size_t pendingCount = 0;
        std::thread eventThread;
        std::mutex catMutex;
        std::atomic<bool> running = false;
    };

    std::vector<AudioDeviceInfo> listPlatformAudioDevices() {
        return {};
    }

    std::vector<SerialPortInfo> listPlatformSerialPorts() {
        return {};
    }

    std::unique_ptr<DeviceImpl> createDeviceImpl() {
        return std::make_unique<AndroidUsbImpl>();
    }
}

#endif
