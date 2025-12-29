//
// Created by lily on 12/28/25.
//

#ifndef MINECRAFTCLIENT_WORLDTYPE_HPP
#define MINECRAFTCLIENT_WORLDTYPE_HPP
#include <string>
#include <algorithm>
#include <cctype>

struct WorldType {
private:
    static std::string toLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c){ return std::tolower(c); });
        return result;
    }
public:

    static WorldType* worldTypes[16];

    // these have to all be on their own line.. for some reason
    static WorldType* DEFAULT;
    static WorldType* FLAT;
    static WorldType* LARGE_BIOMES;
    static WorldType* AMPLIFIED;
    static WorldType* CUSTOMIZED;
    static WorldType* DEBUG_WORLD;
    static WorldType* DEFAULT_1_1;

    const int worldTypeId, generatorVersion;
    const std::string& worldType;

    bool isWorldTypeVersioned = false, canBeCreated = true, hasNotificationData = false;
    WorldType* setVersioned() {
        isWorldTypeVersioned = true;
        return this;
    }
    WorldType* setCanBeCreated(const bool canBe) {
        canBeCreated = canBe;
        return this;
    }
    WorldType* setNotificationData() {
        hasNotificationData = true;
        return this;
    }

    static WorldType* parseWorldType(const std::string & string) {
        for (const auto type : worldTypes) {
            if (type && toLower(type->worldType) == toLower(string)) return type;
        }
        return nullptr;
    }

    WorldType(const int id, const std::string& name, const int version) : worldTypeId(id), generatorVersion(version), worldType(name) {
        worldTypes[id] = this;
    }
    WorldType(const int id, const std::string& name) : WorldType(id, name, 0) {};

};

#endif //MINECRAFTCLIENT_WORLDTYPE_HPP