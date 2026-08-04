#pragma once
#include <config.h>
#include <module.h>
#include <module_com.h>
#include <string>
#include "command_args.h"

namespace core {
    SDRPP_EXPORT ConfigManager configManager;
    SDRPP_EXPORT ModuleManager moduleManager;
    SDRPP_EXPORT ModuleComManager modComManager;
    SDRPP_EXPORT CommandArgsParser args;

    void setInputSampleRate(double samplerate);

    // Commit live UI state that is not continuously persisted, then force the
    // application configuration to disk. Used by lifecycle events where the
    // process may be terminated before the auto-save interval expires.
    SDRPP_CPP_EXPORT void saveState();

    // Effective resource paths. Returns the value from the config,
    // except under AppImage builds (BUILD_APPIMAGE) on Linux when $APPDIR
    // is set — in which case the bundled paths inside the AppImage mount
    // are returned. The accessor pattern keeps the FUSE mount point out
    // of the persisted config file. Both take the config lock themselves, so
    // they must not be called with a transaction open.
    SDRPP_CPP_EXPORT std::string getModulesDirectory();
    SDRPP_CPP_EXPORT std::string getResourcesDirectory();

    // Resolve a possibly relative path from the config against the
    // executable's directory (see core.cpp for rationale).
    SDRPP_CPP_EXPORT std::string resolveConfigPath(const std::string& path);
};

int sdrpp_main(int argc, char* argv[]);
