#pragma once

#include <array>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

struct PlayerListEntry {
    std::array<uint8_t, 16> uuid{};
    std::string name;
    std::string displayName;
    std::string skinUrl;
    bool slimSkin = false;
    int gameMode = 0;
    int ping = 0;
};

class PlayerList {
public:
    static inline std::mutex mutex;
    static inline std::unordered_map<std::string, PlayerListEntry> entries;

    static std::string key(const std::array<uint8_t, 16>& uuid) {
        return {reinterpret_cast<const char*>(uuid.data()), uuid.size()};
    }

    static std::vector<PlayerListEntry> snapshot() {
        std::lock_guard lock(mutex);
        std::vector<PlayerListEntry> result;
        result.reserve(entries.size());
        for (const auto& [key, entry] : entries) result.push_back(entry);
        return result;
    }
};
