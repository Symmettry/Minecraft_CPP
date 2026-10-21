#pragma once

#include <sstream>
#include <string>
#include <unordered_map>

#include "net/lily/minecpp/util/Zstd.hpp"

class ItemAtlas {
public:
    static constexpr int tilesPerRow = 16;

    static ItemAtlas load(const std::string& dataPath) {
        ItemAtlas atlas;
        std::stringstream input(Zstd::readZstdFile(dataPath));
        std::string line;
        while (std::getline(input, line)) {
            const size_t separator = line.find(',');
            if (separator == std::string::npos) continue;
            atlas.tiles.emplace(line.substr(0, separator),
                static_cast<uint16_t>(std::stoi(line.substr(separator + 1))));
        }
        return atlas;
    }

    [[nodiscard]] uint16_t tile(const std::string& name) const {
        if (const auto found = tiles.find(name); found != tiles.end()) return found->second;
        if (const auto fallback = tiles.find("barrier"); fallback != tiles.end()) return fallback->second;
        return 0;
    }

private:
    std::unordered_map<std::string, uint16_t> tiles;
};
