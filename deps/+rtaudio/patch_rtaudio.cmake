#
# Run with -DSRC=<source-dir> -P this-script.
#
# CoreAudio device-name encoding fix, kept as a proper git patch because it is
# intended for an upstream pull request (github.com/thestk/rtaudio).
#
# RtApiCore::getDeviceInfo() selects the CFStringGetCString() encoding behind
# `#if defined( UNICODE ) || defined( _UNICODE )` — Win32 TCHAR macros that are
# never defined on an Apple build — so the UTF-8 branch is dead and every macOS
# device name is converted with the locale-dependent
# CFStringGetSystemEncoding(). The conversion's return value is ignored too, so
# a failed conversion leaves the malloc'd buffer uninitialized and the
# following strlen() reads uninitialized heap.
#
# Non-ASCII macOS device names therefore reach us in an unspecified encoding.
# Anything that stores the name as UTF-8 — our ConfigManager writes device
# names into config.json — either mangles it or fails to serialize it. See
# doc/research/forks/aurimasniekis.md for the full analysis.
#
# No effect on non-Apple builds: the patched code is inside __MACOSX_CORE__.
#
include(${CMAKE_CURRENT_LIST_DIR}/../cmake/patch_helpers.cmake)

patch_apply_git_or_fail("${SRC}" "${CMAKE_CURRENT_LIST_DIR}/patches/0001-coreaudio-utf8-device-names.patch")
