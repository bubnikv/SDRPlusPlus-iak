#include "ft8_engine.h"

#include <cstring>
#include <cstdio>
#include <vector>

extern "C" {
#include <ft8/message.h>
#include <common/monitor.h>
}

// ---------------------------------------------------------------------------
// Decoder tuning constants (same defaults as the ft8_lib reference decoder).
// ---------------------------------------------------------------------------
static const int kMin_score        = 10;   // Minimum Costas sync score
static const int kMax_candidates   = 140;
static const int kLDPC_iterations   = 25;
static const int kMax_decoded_msgs = 50;
static const int kFreq_osr         = 2;    // frequency oversampling
static const int kTime_osr         = 2;    // time oversampling
static const float kFreqMin        = 100.0f;
static const float kFreqMax        = 3000.0f;

// ---------------------------------------------------------------------------
// Persistent callsign hash table (ported from ft8_lib's decode_ft8 demo).
// Decoding is serialised on a single worker thread, so a file-scope table is
// safe and lets hashed callsigns resolve across consecutive slots.
// ---------------------------------------------------------------------------
#define CALLSIGN_HASHTABLE_SIZE 256

static struct {
    char     callsign[12];
    uint32_t hash;
} g_callsign_hashtable[CALLSIGN_HASHTABLE_SIZE];
static int g_callsign_hashtable_size = 0;

static void hashtable_cleanup(uint8_t max_age) {
    for (int i = 0; i < CALLSIGN_HASHTABLE_SIZE; ++i) {
        if (g_callsign_hashtable[i].callsign[0] != '\0') {
            uint8_t age = (uint8_t)(g_callsign_hashtable[i].hash >> 24);
            if (age > max_age) {
                g_callsign_hashtable[i].callsign[0] = '\0';
                g_callsign_hashtable[i].hash = 0;
                g_callsign_hashtable_size--;
            }
            else {
                g_callsign_hashtable[i].hash =
                    (((uint32_t)age + 1u) << 24) | (g_callsign_hashtable[i].hash & 0x3FFFFFu);
            }
        }
    }
}

static void hashtable_add(const char* callsign, uint32_t hash) {
    uint16_t hash10 = (hash >> 12) & 0x3FFu;
    int idx = (hash10 * 23) % CALLSIGN_HASHTABLE_SIZE;
    while (g_callsign_hashtable[idx].callsign[0] != '\0') {
        if (((g_callsign_hashtable[idx].hash & 0x3FFFFFu) == hash) &&
            (0 == strcmp(g_callsign_hashtable[idx].callsign, callsign))) {
            g_callsign_hashtable[idx].hash &= 0x3FFFFFu; // reset age
            return;
        }
        idx = (idx + 1) % CALLSIGN_HASHTABLE_SIZE;
    }
    g_callsign_hashtable_size++;
    strncpy(g_callsign_hashtable[idx].callsign, callsign, 11);
    g_callsign_hashtable[idx].callsign[11] = '\0';
    g_callsign_hashtable[idx].hash = hash;
}

static bool hashtable_lookup(ftx_callsign_hash_type_t hash_type, uint32_t hash, char* callsign) {
    uint8_t hash_shift = (hash_type == FTX_CALLSIGN_HASH_10_BITS) ? 12
                       : (hash_type == FTX_CALLSIGN_HASH_12_BITS ? 10 : 0);
    uint16_t hash10 = (hash >> (12 - hash_shift)) & 0x3FFu;
    int idx = (hash10 * 23) % CALLSIGN_HASHTABLE_SIZE;
    while (g_callsign_hashtable[idx].callsign[0] != '\0') {
        if (((g_callsign_hashtable[idx].hash & 0x3FFFFFu) >> hash_shift) == hash) {
            strcpy(callsign, g_callsign_hashtable[idx].callsign);
            return true;
        }
        idx = (idx + 1) % CALLSIGN_HASHTABLE_SIZE;
    }
    callsign[0] = '\0';
    return false;
}

static ftx_callsign_hash_interface_t g_hash_if = {
    hashtable_lookup,
    hashtable_add
};

// ---------------------------------------------------------------------------
void FT8Engine::decodeSlot(ftx_protocol_t protocol, int sampleRate,
                           const float* audio, int nSamples,
                           const char* hhmmss,
                           const std::function<void(const FTxResult&)>& emit) {
    if (nSamples <= 0 || audio == nullptr) { return; }

    monitor_config_t cfg;
    cfg.f_min       = kFreqMin;
    cfg.f_max       = kFreqMax;
    cfg.sample_rate = sampleRate;
    cfg.time_osr    = kTime_osr;
    cfg.freq_osr    = kFreq_osr;
    cfg.protocol    = protocol;

    monitor_t mon;
    monitor_init(&mon, &cfg);

    // Feed the slot audio to the monitor one symbol-block at a time.
    int pos = 0;
    while (pos + mon.block_size <= nSamples && mon.wf.num_blocks < mon.wf.max_blocks) {
        monitor_process(&mon, audio + pos);
        pos += mon.block_size;
    }

    const ftx_waterfall_t* wf = &mon.wf;

    // Locate the strongest Costas-sync candidates.
    std::vector<ftx_candidate_t> candidates(kMax_candidates);
    int num_candidates = ftx_find_candidates(wf, kMax_candidates, candidates.data(), kMin_score);

    // De-duplication hash table for the messages decoded in this slot.
    std::vector<ftx_message_t>  decoded(kMax_decoded_msgs);
    std::vector<ftx_message_t*> decoded_ht(kMax_decoded_msgs, nullptr);

    for (int idx = 0; idx < num_candidates; ++idx) {
        const ftx_candidate_t* cand = &candidates[idx];

        float freq_hz = (mon.min_bin + cand->freq_offset + (float)cand->freq_sub / wf->freq_osr) / mon.symbol_period;
        float time_sec = (cand->time_offset + (float)cand->time_sub / wf->time_osr) * mon.symbol_period;

        ftx_message_t message;
        ftx_decode_status_t status;
        if (!ftx_decode_candidate(wf, cand, kLDPC_iterations, &message, &status)) {
            continue; // LDPC or CRC failure
        }

        // Check this slot's de-dup hash table.
        int h = message.hash % kMax_decoded_msgs;
        bool empty_slot = false, duplicate = false;
        do {
            if (decoded_ht[h] == nullptr) { empty_slot = true; }
            else if ((decoded_ht[h]->hash == message.hash) &&
                     (0 == memcmp(decoded_ht[h]->payload, message.payload, sizeof(message.payload)))) {
                duplicate = true;
            }
            else { h = (h + 1) % kMax_decoded_msgs; }
        } while (!empty_slot && !duplicate);

        if (!empty_slot) { continue; }

        decoded[h] = message;
        decoded_ht[h] = &decoded[h];

        char text[FTX_MAX_MESSAGE_LENGTH];
        ftx_message_offsets_t offsets;
        if (ftx_message_decode(&message, &g_hash_if, text, &offsets) != FTX_MESSAGE_RC_OK) {
            continue; // could not unpack
        }

        FTxResult r;
        r.time = hhmmss ? hhmmss : "";
        // ft8_lib does not expose a WSJT-X-equivalent SNR; the sync score is
        // a monotonic proxy and is reported as an approximate value.
        r.snr  = cand->score * 0.5f - 24.0f;
        r.dt   = time_sec;
        r.freq = freq_hz;
        r.text = text;
        emit(r);
    }

    // Age out stale callsign-hash entries.
    hashtable_cleanup(10);

    monitor_free(&mon);
}
