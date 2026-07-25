# libcurl integration

Binding rule for plugin authors. Split out of the old `doc/Forks.txt`, where it
had been appended below an unrelated fork survey.

libcurl is the sanctioned HTTPS/WebSocket implementation dependency in core.
Plugins must not include <curl/curl.h>, add find_package(CURL), or link libcurl
directly. They should use the HTTP/WebSocket wrappers exported by sdrpp_core
instead. Static-linking libcurl into multiple DSOs in the same process is a known
source of nondeterministic TLS failures (curl has process-global state for TLS
callbacks, mutex tables, PRNG).

Per-platform TLS backend table:
    Platform   libcurl source             TLS backend         Cert store
    --------   -------------------------  ------------------  ----------------------------
    Windows    bundled (deps/+libcurl)    Schannel            Windows Cert Store
    macOS      bundled                    Secure Transport    Keychain
    Linux      system OR bundled          OpenSSL             /etc/ssl/certs
    Android    bundled (+mbedtls dep)     MbedTLS             /system/etc/security/cacerts

libcurl is owned by the deps system (deps/+libcurl/libcurl.cmake). Its
classification (deps/cmake/DepClassification.cmake) defaults to `system`
on the distro profile and `bundled` on portable/android profiles. Override
per-build with the standard dep knobs:
    -DSDRPP_DEP_FORCE_BUNDLED=libcurl  # AppImage/Flatpak or old distros
    -DSDRPP_DEP_FORCE_SYSTEM=libcurl   # use distro libcurl on a portable build

System libcurl must be ≥ 8.5 with WebSocket support compiled in
(curl_ws_send must exist). core/CMakeLists.txt probes the symbol after
linking and configure fails cleanly otherwise.

No CA bundle is ever shipped. On Android, curl_init.cpp sets CURLOPT_CAPATH to
the OS trust store via curl::make_easy(); core networking wrappers use that
handle factory so plugins never have to touch libcurl directly.

curl_global_init/cleanup are called from sdrpp_main() in core. Plugins must
not call them.
