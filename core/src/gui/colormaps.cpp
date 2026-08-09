#include <gui/colormaps.h>
#include <filesystem>
#include <utils/flog.h>
#include <utils/hex_color.h>
#include <fstream>
#include <json.hpp>
#include <utility>

using nlohmann::json;

namespace colormaps {
    std::map<std::string, Map> maps;

    void loadMap(const std::string& path) {
        if (!std::filesystem::is_regular_file(path)) {
            flog::error("Could not load {0}, file doesn't exist", path);
            return;
        }

        Map map;
        std::vector<std::string> encodedColors;

        try {
            std::ifstream file(path.c_str());
            json data;
            file >> data;

            map.name = data.at("name").get<std::string>();
            map.author = data.at("author").get<std::string>();
            encodedColors = data.at("map").get<std::vector<std::string>>();
        }
        catch (const std::exception& e) {
            flog::error("Could not load {0}: {1}", path, e.what());
            return;
        }

        if (encodedColors.empty()) {
            flog::error("Could not load {0}: the color map has no entries", path);
            return;
        }

        map.colors.reserve(encodedColors.size() * 3);
        for (const std::string& encoded : encodedColors) {
            const auto color = color_utils::parseHex<3>(encoded);
            if (!color) {
                flog::error(
                    "Could not load {0}: malformed color '{1}', expected \"#RRGGBB\"",
                    path, encoded);
                return;
            }
            for (uint8_t channel : *color) {
                map.colors.push_back(static_cast<float>(channel));
            }
        }

        const std::string name = map.name;
        maps.insert_or_assign(name, std::move(map));
    }
}
