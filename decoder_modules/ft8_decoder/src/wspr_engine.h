#pragma once
#include <string>
#include <vector>
#include <functional>

// One decoded WSPR message.
struct WSPRResult {
    std::string time;     // "HHMM" slot start (UTC)
    float       snr;      // SNR in dB
    float       dt;       // time offset in seconds
    double      freq;     // absolute on-air frequency in MHz
    float       drift;    // frequency drift in Hz
    std::string message;  // decoded WSPR message
};

// WSPR decoder.
//
// Takes one 120-second slot of USB-demodulated mono audio at 12 kHz, mixes the
// nominal 1500 Hz sub-band centre down to DC, low-pass filters and decimates to
// 375 Hz complex baseband, then runs the K9AN/K1JT wsprd decode core.
class WSPREngine {
public:
    WSPREngine();

    // audio/nSamples : slot audio (mono, real, USB), ~120 s at 12 kHz
    // sampleRate     : audio sample rate (must be 12000)
    // dialFreqMHz    : WSPR dial frequency in MHz
    // yymmdd, hhmm   : slot date/time labels (UTC)
    // workdir        : writable dir for the persistent callsign hashtable
    // emit           : called once per unique decoded message
    void decodeSlot(const float* audio, int nSamples, int sampleRate,
                    double dialFreqMHz,
                    const char* yymmdd, const char* hhmm,
                    const std::string& workdir,
                    const std::function<void(const WSPRResult&)>& emit);

private:
    void buildFilter();
    std::vector<float> fir;  // low-pass FIR (decimation prototype)
    int   decim;             // decimation factor (12000 -> 375 = 32)
};
