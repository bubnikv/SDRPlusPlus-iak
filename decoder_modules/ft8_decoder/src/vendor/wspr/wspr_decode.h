/*
 * wspr_decode.h - Callable interface around the K9AN/K1JT WSPR decoder.
 *
 * This is a thin wrapper around the proven `wsprd` decode loop (Steven
 * Franke K9AN / Joe Taylor K1JT, GPL-3.0). The original command-line
 * front-end (WAV/C2 file reading, argv parsing, stdout printing) has been
 * replaced by an in-memory array interface and a result callback so the
 * decoder can be driven directly from a real-time SDR pipeline.
 *
 * The decode core itself (sync_and_demodulate, subtract_signal2, the Fano
 * sequential decoder, the candidate search and message unpacking) is kept
 * verbatim from the upstream sources.
 */
#ifndef WSPR_DECODE_H
#define WSPR_DECODE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Sample rate of the baseband I/Q fed to wspr_decode(). */
#define WSPR_IQ_RATE   375
/* Number of complex samples in one 2-minute WSPR slot (120 s * 375 Hz). */
#define WSPR_NPOINTS   45000

/*
 * Result callback, invoked once per decoded (and de-duplicated) message.
 *   ctx       : opaque pointer passed through from wspr_decode()
 *   time_utc  : 4-char "HHMM" string of the slot start
 *   snr       : estimated SNR in dB (2500 Hz reference bandwidth)
 *   dt        : time offset in seconds
 *   freq_MHz  : absolute on-air frequency in MHz
 *   drift     : frequency drift in Hz over the transmission
 *   message   : decoded WSPR message (callsign / locator / power, etc.)
 */
typedef void (*wspr_emit_cb)(void* ctx,
                             const char* time_utc,
                             double snr,
                             double dt,
                             double freq_MHz,
                             double drift,
                             const char* message);

/*
 * Decode one WSPR slot.
 *
 *   idat_in, qdat_in : baseband I/Q at 375 Hz, DC == dialfreq + 1500 Hz,
 *                      npoints_in samples each (use WSPR_NPOINTS).
 *   dialfreq_in      : dial frequency in MHz (the WSPR sub-band dial freq).
 *   usehashtable     : non-zero to maintain a persistent callsign hashtable.
 *   workdir          : writable directory for the hashtable / wisdom files
 *                      (may be NULL -> current directory).
 *   date_in          : 6-char "YYMMDD" slot date (used only for labelling).
 *   uttime_in        : 4-char "HHMM"   slot time (used only for labelling).
 *   emit_cb, emit_ctx: result callback and its context (emit_cb may be NULL).
 *
 * Returns 0 on success.
 */
int wspr_decode(float* idat_in, float* qdat_in, unsigned int npoints_in,
                double dialfreq_in, int usehashtable, const char* workdir,
                const char* date_in, const char* uttime_in,
                wspr_emit_cb emit_cb, void* emit_ctx);

#ifdef __cplusplus
}
#endif

#endif /* WSPR_DECODE_H */
