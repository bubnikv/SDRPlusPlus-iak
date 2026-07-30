#pragma once
#include <config.h>
#include <module.h>
#include <module_com.h>
#include <string>
#include "command_args.h"

namespace frequency_catalog {
    class FrequencyCatalog;
    class CatalogStore;
}

namespace core {
    SDRPP_EXPORT ConfigManager configManager;
    SDRPP_EXPORT ModuleManager moduleManager;
    SDRPP_EXPORT ModuleComManager modComManager;
    SDRPP_EXPORT CommandArgsParser args;

    void setInputSampleRate(double samplerate);

    // Effective resource paths. Returns the value from configManager.conf,
    // except under AppImage builds (BUILD_APPIMAGE) on Linux when $APPDIR
    // is set — in which case the bundled paths inside the AppImage mount
    // are returned. The accessor pattern keeps the FUSE mount point out
    // of the persisted config file.
    SDRPP_CPP_EXPORT std::string getModulesDirectory();
    SDRPP_CPP_EXPORT std::string getResourcesDirectory();

    // Resolve a possibly relative path from the config against the
    // executable's directory (see core.cpp for rationale).
    SDRPP_CPP_EXPORT std::string resolveConfigPath(const std::string& path);

    // Process-wide catalog for stable Bands, plan Segments, persisted
    // bookmarks and dynamic frequency-data providers.
    SDRPP_CPP_EXPORT frequency_catalog::FrequencyCatalog& getFrequencyCatalog();
    SDRPP_CPP_EXPORT frequency_catalog::CatalogStore& getFrequencyCatalogStore();
};

int sdrpp_main(int argc, char* argv[]);
