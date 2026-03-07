#pragma once
#include <utils/networking.h>
#include <qmxserver_protocol.h>
#include <dsp/stream.h>
#include <dsp/types.h>

#include <atomic>
#include <string>
#include <vector>
#include <cstdint>

#include <enet/enet.h>

#define EXTIO_EXPORTS       1
#define HWNAME              "ExtIO_Omnia-0.3"
#define HWMODEL             "ExtIO_Omnia"
#define SETTINGS_IDENTIFIER "ExtIO_Omnia-0.3"
// 5.3ms latency
#define EXT_BLOCKLEN        (512)           /* only multiples of 512 */
#define ZEROS_TO_MUTE       (32)
#define SAMPLE_RATE         (48000)
#define MUTE_ENVELOPE_LEN   (96*2)
#define CW_IQ_TONE_OFFSET   (1000)

enum KeyerMode {
    KEYER_MODE_SK = 0,
    KEYER_MODE_IAMBIC_A = 1,
    KEYER_MODE_IAMBIC_B = 2,
};

struct Config
{
    // Serialize the config into a list of key=value pairs, separated by newlines.
    std::string serialize() const;
    // Deserialize the config from a list of key=value pairs, separated by newlines.
    void        deserialize(const char *str);
    // Validate the config data, set reasonable defaults if the values are out of bounds.
    void        validate();

    // TX IQ amplitude balance correction, 0.8 to 1.2;
    double      tx_iq_balance_amplitude_correction  = 1.;
    // TX IQ phase balance adjustment correcton, - 15 degrees to + 15 degrees.
    double      tx_iq_balance_phase_correction      = 0.;
    // TX power adjustment, from 0 to 1.
    double      tx_power                            = 1.;

    KeyerMode   keyer_mode                          = KEYER_MODE_IAMBIC_B;
    int         keyer_wpm                           = 18;

    bool        amp_enabled                         = false;
    int         tx_delay                            = 8000; // 8ms
    int         tx_hang                             = 500000; // 0.5s

    bool        network_client                      = false;
    std::string network_server_name;
    int         network_server_port                 = 1234;
};

struct _ENetPacket;

namespace qmxserver {

    class QmxServerClientClass {
    public:
        QmxServerClientClass(ENetHost *client, ENetPeer *peer, ENetAddress address, dsp::stream<dsp::complex_t>* out);
        ~QmxServerClientClass();

        bool waitForDevInfo(int timeoutMS);

        void startStream();
        void stopStream();

        void setSetting(uint32_t setting, uint32_t arg);
        bool set_freq(int64_t frequency);

        void close();
        bool isOpen();

        int computeDigitalGain(int serverBits, int deviceGain, int decimationId);

        QmxServerDeviceInfo devInfo;

        const std::string get_error() const { return error; }
        std::string error;

    private:
        void sendCommand(uint32_t command, void* data, int len);
        void sendHandshake(std::string appName);

        int readSize(int count, uint8_t* buffer);

        static void dataHandler(int count, uint8_t* buf, void* ctx);

        // net::Conn client;

        uint8_t* readBuf;
        uint8_t* writeBuf;

        bool deviceInfoAvailable = false;
        std::mutex deviceInfoMtx;
        std::condition_variable deviceInfoCnd;

        QmxServerMessageHeader receivedHeader;

        dsp::stream<dsp::complex_t>* output;

        void run();
        void thread_function();
        void send_packet(char* buf, int buflen);
        void send_packet(_ENetPacket *packet);

        ENetHost                   *m_client{ nullptr };
        ENetPeer                   *m_peer { nullptr };
        ENetAddress                 m_address;
        std::thread                 m_thread;
        std::mutex                  m_mutex;
        std::vector<_ENetPacket*>   m_queue;
        bool                        m_connected { false };
        std::atomic<bool>           m_exit_thread { false };
    };

    typedef std::unique_ptr<QmxServerClientClass> QmxServerClient;

    QmxServerClient connect(std::string host, uint16_t port, dsp::stream<dsp::complex_t>* out);

}
