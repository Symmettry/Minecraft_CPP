#pragma once

#include <cstdint>
#include <cctype>
#include <sstream>
#include <string>
#include <unordered_map>

#include "Zstd.hpp"

class ItemRegistry {
public:
    enum class UseAction : uint8_t { NONE, EAT, DRINK, BLOCK, BOW };
    static const std::string& identifier(const int16_t id, const int16_t damage) {
        static const std::unordered_map<uint32_t, std::string> identifiers = load();
        const uint32_t exact = key(id, damage);
        if (const auto found = identifiers.find(exact); found != identifiers.end()) return found->second;
        if (const auto found = identifiers.find(key(id, 0)); found != identifiers.end()) return found->second;
        static const std::string fallback = "barrier";
        return fallback;
    }

    static std::string displayName(const int16_t id, const int16_t damage) {
        std::string name = identifier(id, damage);
        bool capitalize = true;
        for (char& character : name) {
            if (character == '_') {
                character = ' ';
                capitalize = true;
            } else if (capitalize) {
                character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
                capitalize = false;
            }
        }
        return name;
    }

    static constexpr bool isFood(const int16_t id) {
        switch (id) {
            case 260: case 282: case 297: case 319: case 320: case 322:
            case 349: case 350: case 357: case 360: case 363:
            case 364: case 365: case 366: case 367: case 391: case 392:
            case 393: case 394: case 396: case 400: case 411: case 412:
            case 413: case 423: case 424:
                return true;
            default: return false;
        }
    }

    static constexpr bool hasUseAction(const int16_t id) {
        return useAction(id) != UseAction::NONE;
    }

    static constexpr UseAction useAction(const int16_t id) {
        if (isFood(id)) return UseAction::EAT;
        switch (id) {
            case 261: return UseAction::BOW;
            case 267: case 268: case 272: case 276: case 283: case 442:
                return UseAction::BLOCK;
            case 335: case 373: return UseAction::DRINK;
            default: return UseAction::NONE;
        }
    }

private:
    static uint32_t key(const int16_t id, const int16_t damage) {
        return static_cast<uint32_t>(static_cast<uint16_t>(id)) << 16
            | static_cast<uint16_t>(damage);
    }

    static std::unordered_map<uint32_t, std::string> load() {
        std::unordered_map<uint32_t, std::string> result;
        std::stringstream input(Zstd::readZstdFile("assets/itemdata.dat.zst"));
        std::string line;
        std::getline(input, line);
        while (std::getline(input, line)) {
            std::stringstream row(line);
            std::string id, damage, identifier;
            if (!std::getline(row, id, ',') || !std::getline(row, damage, ',')
                || !std::getline(row, identifier, ',')) continue;
            result.emplace(key(static_cast<int16_t>(std::stoi(id)),
                static_cast<int16_t>(std::stoi(damage))), identifier);
        }
        return result;
    }
};
