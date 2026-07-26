#pragma once

enum {
    RADIO_IFACE_CMD_GET_MODE,
    RADIO_IFACE_CMD_SET_MODE,
    RADIO_IFACE_CMD_GET_BANDWIDTH,
    RADIO_IFACE_CMD_SET_BANDWIDTH,
    RADIO_IFACE_CMD_GET_SQUELCH_MODE,
    RADIO_IFACE_CMD_SET_SQUELCH_MODE,
    RADIO_IFACE_CMD_GET_SQUELCH_LEVEL,
    RADIO_IFACE_CMD_SET_SQUELCH_LEVEL,
    RADIO_IFACE_CMD_GET_CTCSS_TONE,
    RADIO_IFACE_CMD_SET_CTCSS_TONE,
    RADIO_IFACE_CMD_GET_HIGHPASS,
    RADIO_IFACE_CMD_SET_HIGHPASS,

    // Runtime injection of external processors into the audio (AF) chain.
    // `in` is a dsp::Processor<dsp::stereo_t, dsp::stereo_t>*. Appended after
    // the upstream commands so existing numeric values stay stable.
    // Introduced in SDRPPBrown for baseband noise suppression.
    RADIO_IFACE_CMD_ADD_TO_AFCHAIN,
    RADIO_IFACE_CMD_REMOVE_FROM_AFCHAIN,
    RADIO_IFACE_CMD_ENABLE_IN_AFCHAIN,
    RADIO_IFACE_CMD_DISABLE_IN_AFCHAIN,
};

enum {
    RADIO_IFACE_MODE_NFM,
    RADIO_IFACE_MODE_WFM,
    RADIO_IFACE_MODE_AM,
    RADIO_IFACE_MODE_DSB,
    RADIO_IFACE_MODE_USB,
    RADIO_IFACE_MODE_CW,
    RADIO_IFACE_MODE_LSB,
    RADIO_IFACE_MODE_RAW,
    RADIO_IFACE_MODE_CWR,
    _RADIO_IFACE_MODE_COUNT
};

// The one spelling of a mode, indexed by the enum above. Every place a mode is
// written or shown as text uses this: the radio menu buttons, the radio's own
// per-mode config keys, band stacking registers, bookmarks, recorder filenames,
// Discord presence, band plan `def_mode`.
//
// "NFM"/"WFM" rather than Hamlib's "FM"/"WFM" because this is a receiver, and
// every receiver its users come from (SDR#, SDRangel, Gqrx, SDRuno) names both
// explicitly -- a transceiver can call narrow FM just "FM" only because it is
// the sole FM it transmits. See doc/design/radio-modes.md.
//
// Exactly one name deviates anywhere, and it is a wire protocol rather than a
// choice: rigctl_server must speak Hamlib, where RIG_MODE_FM is documented as
// "narrow" band FM. That is a one-line exception in hamlibModeName() there.
// The bound is deduced, not written: with an explicit one, appending an
// enumerator without a name would leave a value-initialized null in the array
// and radioModeName() would hand it out past its own bounds check. The assert
// below turns that omission into a compile error instead -- which is the whole
// point of this table, RADIO_IFACE_MODE_CWR having already been appended once
// to an enum whose name tables did not follow.
inline constexpr const char* RADIO_IFACE_MODE_NAMES[] = {
    "NFM", "WFM", "AM", "DSB", "USB", "CW", "LSB", "RAW", "CWR"
};
static_assert(sizeof(RADIO_IFACE_MODE_NAMES) / sizeof(*RADIO_IFACE_MODE_NAMES) == _RADIO_IFACE_MODE_COUNT,
              "every RADIO_IFACE_MODE_* needs an entry in RADIO_IFACE_MODE_NAMES");

// Name of a mode, or "--" for a value that is not one (-1 = nothing stored).
constexpr const char* radioModeName(int mode) {
    return (mode >= 0 && mode < _RADIO_IFACE_MODE_COUNT) ? RADIO_IFACE_MODE_NAMES[mode] : "--";
}

// strcmp is not constexpr in C++17, and these strings are four bytes at most.
constexpr bool radioModeNameEq(const char* a, const char* b) {
    while (*a && *a == *b) { ++a; ++b; }
    return *a == *b;
}

// Mode for a name, -1 if unrecognised. Foreign spellings are accepted on input
// so that a hand-written band plan `def_mode` or a config carried over from
// another program still parses; they are never emitted.
constexpr int radioModeFromName(const char* name) {
    if (!name) { return -1; }
    for (int i = 0; i < _RADIO_IFACE_MODE_COUNT; i++) {
        if (radioModeNameEq(name, RADIO_IFACE_MODE_NAMES[i])) { return i; }
    }
    if (radioModeNameEq(name, "FM")) { return RADIO_IFACE_MODE_NFM; }    // Hamlib / transceiver spelling
    if (radioModeNameEq(name, "CW-R")) { return RADIO_IFACE_MODE_CWR; }  // Icom / earlier builds
    return -1;
}

// Both directions agree, checked at compile time rather than trusted.
static_assert(radioModeFromName(radioModeName(RADIO_IFACE_MODE_NFM)) == RADIO_IFACE_MODE_NFM, "NFM does not round-trip");
static_assert(radioModeFromName(radioModeName(RADIO_IFACE_MODE_WFM)) == RADIO_IFACE_MODE_WFM, "WFM does not round-trip");
static_assert(radioModeFromName(radioModeName(RADIO_IFACE_MODE_CWR)) == RADIO_IFACE_MODE_CWR, "CWR does not round-trip");
static_assert(radioModeFromName("FM") == RADIO_IFACE_MODE_NFM, "the Hamlib spelling must still parse");
static_assert(radioModeFromName("CW-R") == RADIO_IFACE_MODE_CWR, "the Icom spelling must still parse");
static_assert(radioModeFromName("nope") == -1, "an unknown name must not resolve");