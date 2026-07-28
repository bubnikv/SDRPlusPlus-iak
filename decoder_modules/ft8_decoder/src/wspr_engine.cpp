#include "wspr_engine.h"

#include <cmath>
#include <cstring>

extern "C" {
#include "vendor/wspr/wspr_decode.h"
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ---------------------------------------------------------------------------
// Bridge from the C result callback (with void* ctx) to a std::function.
// ---------------------------------------------------------------------------
namespace {
    struct EmitCtx {
        const std::function<void(const WSPRResult&)>* emit;
    };

    void wspr_trampoline(void* ctx, const char* time_utc, double snr, double dt,
                         double freq_MHz, double drift, const char* message) {
        EmitCtx* e = reinterpret_cast<EmitCtx*>(ctx);
        WSPRResult r;
        r.time    = time_utc ? time_utc : "";
        r.snr     = (float)snr;
        r.dt      = (float)dt;
        r.freq    = freq_MHz;
        r.drift   = (float)drift;
        r.message = message ? message : "";
        (*e->emit)(r);
    }
}

// ---------------------------------------------------------------------------
WSPREngine::WSPREngine() {
    decim = 32;          // 12000 Hz -> 375 Hz
    buildFilter();
}

void WSPREngine::buildFilter() {
    // Hann-windowed sinc low-pass. Cutoff 180 Hz at 12 kHz keeps the whole
    // +/-150 Hz WSPR sub-band (centred at DC after mixing) with margin.
    const int   L  = 16 * decim + 1;     // 513 taps
    const float fs = 12000.0f;
    const float fc = 180.0f;
    const float fcn = fc / fs;           // normalised cutoff (0..0.5)
    const int   c  = (L - 1) / 2;

    fir.resize(L);
    double sum = 0.0;
    for (int n = 0; n < L; ++n) {
        float x = (float)(n - c);
        float sinc = (n == c) ? (2.0f * fcn)
                              : sinf(2.0f * (float)M_PI * fcn * x) / ((float)M_PI * x);
        float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * n / (float)(L - 1))); // Hann
        fir[n] = sinc * w;
        sum += fir[n];
    }
    // Normalise to unity DC gain.
    for (int n = 0; n < L; ++n) { fir[n] /= (float)sum; }
}

// ---------------------------------------------------------------------------
void WSPREngine::decodeSlot(const float* audio, int nSamples, int sampleRate,
                            double dialFreqMHz,
                            const char* yymmdd, const char* hhmm,
                            const std::string& workdir,
                            const std::function<void(const WSPRResult&)>& emit) {
    if (sampleRate != 12000 || audio == nullptr || nSamples <= 0) { return; }

    // 1) Mix the 1500 Hz sub-band centre down to DC.
    //    theta[n] = 2*pi*1500/12000 * n = (pi/4) * n  -> period 8.
    //    b[n] = audio[n] * exp(-j*theta) = audio*cos - j*audio*sin
    static const float COS8[8] = {
        1.0f, 0.70710678f, 0.0f, -0.70710678f, -1.0f, -0.70710678f, 0.0f, 0.70710678f
    };
    static const float SIN8[8] = {
        0.0f, 0.70710678f, 1.0f, 0.70710678f, 0.0f, -0.70710678f, -1.0f, -0.70710678f
    };

    std::vector<float> bi(nSamples), bq(nSamples);
    for (int n = 0; n < nSamples; ++n) {
        int p = n & 7;
        bi[n] =  audio[n] * COS8[p];
        bq[n] = -audio[n] * SIN8[p];
    }

    // 2) Low-pass + decimate by 32 -> 375 Hz complex baseband (45000 samples).
    const int   L = (int)fir.size();
    const int   c = (L - 1) / 2;
    const int   N = WSPR_NPOINTS;          // 45000

    std::vector<float> idat(N, 0.0f), qdat(N, 0.0f);
    for (int m = 0; m < N; ++m) {
        long base = (long)m * decim - c;   // centre the FIR around the output sample
        float acc_i = 0.0f, acc_q = 0.0f;
        for (int k = 0; k < L; ++k) {
            long s = base + k;
            if (s >= 0 && s < nSamples) {
                acc_i += fir[k] * bi[s];
                acc_q += fir[k] * bq[s];
            }
        }
        idat[m] = acc_i;
        qdat[m] = acc_q;
    }

    // 3) Run the WSPR decode core.
    EmitCtx ctx{ &emit };
    wspr_decode(idat.data(), qdat.data(), N, dialFreqMHz,
                /*usehashtable=*/1, workdir.c_str(),
                yymmdd ? yymmdd : "000000",
                hhmm   ? hhmm   : "0000",
                wspr_trampoline, &ctx);
}
