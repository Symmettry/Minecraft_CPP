//
// Created by lily on 12/28/25.
//

#ifndef MINECRAFTCLIENT_WORLDSETTINGS_HPP
#define MINECRAFTCLIENT_WORLDSETTINGS_HPP

#include "World.hpp"
#include "WorldType.hpp"

struct WorldSettings {
    long long seed;
    GameType::Value gameType;
    bool mapFeaturesEnabled;
    bool hardcoreEnabled;
    const WorldType* terrainType;
    bool commandsAllowed = false;
    bool bonusChestEnabled = false;
    std::string worldName;

    WorldSettings(const long long seedIn, const GameType::Value gameType, const bool enableMapFeatures, const bool hardcoreMode, const WorldType* worldType)
        : seed(seedIn), gameType(gameType), mapFeaturesEnabled(enableMapFeatures), hardcoreEnabled(hardcoreMode), terrainType(worldType), worldName("") {}

    WorldSettings* setWorldName(const std::string& newWorldName) {
        worldName = newWorldName;
        return this;
    }

};

#endif //MINECRAFTCLIENT_WORLDSETTINGS_HPP