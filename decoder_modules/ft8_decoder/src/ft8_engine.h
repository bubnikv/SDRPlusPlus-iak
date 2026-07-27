#pragma once
#include <string>
#include <functional>

extern "C" {
#include <ft8/decode.h>
}

// One decoded FT8/FT4 message.
struct FTxResult {
    std::string time;   // "HHMMSS" of the slot start (UTC)
    float       snr;    // approximate SNR in dB
    float       dt;     // time offset in seconds
    float       freq;   // audio frequency in Hz (offset above the dial)
    std::string text;   // decoded message text
};

// Stateless-per-call FT8/FT4 decoder wrapping ft8_lib.
//
// One whole time slot of mono audio (real, USB-demodulated) is passed in at
// once. The Costas-sync search, LDPC decode, CRC check and message unpacking
// are all performed by ft8_lib. A persistent callsign hash table is kept so
// that non-standard / hashed callsigns can be resolved across slots.
class FT8Engine {
public:
    // protocol: FTX_PROTOCOL_FT8 or FTX_PROTOCOL_FT4
    // sampleRate: audio sample rate (Hz), nominally 12000
    // audio/nSamples: the slot's mono audio
    // hhmmss: 6-char slot timestamp used only for labelling
    // emit: called once per unique decoded message
    void decodeSlot(ftx_protocol_t protocol, int sampleRate,
                    const float* audio, int nSamples,
                    const char* hhmmss,
                    const std::function<void(const FTxResult&)>& emit);
};
