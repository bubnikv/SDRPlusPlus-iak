#pragma once
#include <string>
#include <vector>
#include <module.h>
#include <map>

namespace colormaps {
    struct Map {
        std::string name;
        std::string author;
        std::vector<float> colors;

        int entryCount() const noexcept {
            return static_cast<int>(colors.size() / 3);
        }
    };

    void loadMap(const std::string& path);

    SDRPP_EXPORT std::map<std::string, Map> maps;
}
