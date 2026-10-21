#include "GameSettings.hpp"
#include "../Minecraft.hpp"
#include "net/lily/minecpp/net/packets/play/client/C15PacketClientSettings.hpp"
#include <fstream>

void GameSettings::setDefaults() {
    options = {
        {"invertYMouse", "false"}, {"mouseSensitivity", "0.5"}, {"fov", "0.5"},
        {"gamma", "0.0"}, {"renderDistance", "8"}, {"viewBobbing", "true"},
        {"anaglyph3d", "false"}, {"maxFps", "120"}, {"renderClouds", "2"},
        {"fancyGraphics", "true"}, {"ambientOcclusion", "2"}, {"guiScale", "0"},
        {"particles", "0"}, {"chatVisibility", "0"}, {"chatColors", "true"},
        {"chatLinks", "true"}, {"chatOpacity", "1.0"}, {"chatLinksPrompt", "true"},
        {"snooperEnabled", "true"}, {"fullscreen", "false"}, {"enableVsync", "true"},
        {"useVbo", "false"}, {"touchscreen", "false"}, {"chatScale", "1.0"},
        {"chatWidth", "1.0"}, {"chatHeightFocused", "1.0"}, {"chatHeightUnfocused", "0.443"},
        {"mipmapLevels", "4"}, {"forceUnicodeFont", "false"}, {"blockAlternatives", "true"},
        {"reducedDebugInfo", "false"}, {"entityShadows", "true"}, {"realmsNotifications", "true"},
        {"resourcePacks", "[]"}, {"incompatibleResourcePacks", "[]"}, {"lang", "en_US"}
    };
    for (const std::string category : {"master", "music", "record", "weather", "block", "hostile", "neutral", "player", "ambient"}) {
        options["soundCategory_" + category] = "1.0";
    }
    for (const std::string key : {"streamBytesPerPixel", "streamMicVolume", "streamSystemVolume", "streamKbps", "streamFps"}) options[key] = "0.5";
    options["streamCompression"] = "0";
    options["streamSendMetadata"] = "true";
    options["streamChatEnabled"] = "0";
    options["streamChatUserFilter"] = "0";
    options["streamMicToggleBehavior"] = "0";
}

void GameSettings::loadOptions() {
    std::ifstream stream("options.txt");
    std::string line;
    while (std::getline(stream, line)) {
        const size_t separator = line.find(':');
        if (separator != std::string::npos) options[line.substr(0, separator)] = line.substr(separator + 1);
    }
    guiScale = getInt("guiScale");
    language = getOption("lang");
    renderDistance = getInt("renderDistance");
    fov = 30.0f + getFloat("fov") * 80.0f;
    mouseSensitivity.value = getFloat("mouseSensitivity");
    const auto loadKey = [this](const std::string& name, const int fallback) {
        const auto found = this->options.find(name);
        if (found == this->options.end()) return fallback;
        try { return std::stoi(found->second); } catch (...) { return fallback; }
    };
    moveForward.key = loadKey("key_key.forward", GLFW_KEY_W);
    moveBackward.key = loadKey("key_key.back", GLFW_KEY_S);
    moveLeft.key = loadKey("key_key.left", GLFW_KEY_A);
    moveRight.key = loadKey("key_key.right", GLFW_KEY_D);
    jump.key = loadKey("key_key.jump", GLFW_KEY_SPACE);
    sneak.key = loadKey("key_key.sneak", GLFW_KEY_LEFT_SHIFT);
    sprint.key = loadKey("key_key.sprint", GLFW_KEY_R);
}

void GameSettings::saveOptions() const {
    std::ofstream stream("options.txt", std::ios::trunc);
    auto saved = options;
    saved["guiScale"] = std::to_string(guiScale);
    saved["lang"] = language;
    saved["renderDistance"] = std::to_string(renderDistance);
    saved["fov"] = std::to_string((fov - 30.0f) / 80.0f);
    saved["key_key.forward"] = std::to_string(moveForward.key);
    saved["key_key.back"] = std::to_string(moveBackward.key);
    saved["key_key.left"] = std::to_string(moveLeft.key);
    saved["key_key.right"] = std::to_string(moveRight.key);
    saved["key_key.jump"] = std::to_string(jump.key);
    saved["key_key.sneak"] = std::to_string(sneak.key);
    saved["key_key.sprint"] = std::to_string(sprint.key);
    for (const auto& [key, value] : saved) stream << key << ':' << value << '\n';
}

std::string GameSettings::getOption(const std::string& key) const {
    if (const auto found = options.find(key); found != options.end()) return found->second;
    return {};
}

bool GameSettings::getBool(const std::string& key) const { return getOption(key) == "true"; }
float GameSettings::getFloat(const std::string& key) const { try { return std::stof(getOption(key)); } catch (...) { return 0.0f; } }
int GameSettings::getInt(const std::string& key) const { try { return std::stoi(getOption(key)); } catch (...) { return 0; } }
void GameSettings::setOption(const std::string& key, const std::string& value) { options[key] = value; saveOptions(); }
void GameSettings::setBool(const std::string& key, const bool value) { setOption(key, value ? "true" : "false"); }
void GameSettings::setFloat(const std::string& key, const float value) {
    if (key == "mouseSensitivity") mouseSensitivity.value = value;
    setOption(key, std::to_string(value));
}

void GameSettings::sendSettingsToServer() const {
    if (!mc->player) return;

    int i = 0;
    for (int p=0;p<7;p++) {
        if (!modelParts[p]) continue;
        i |= EnumPlayerModelParts::getMask(p);
    }

    mc->netClient->sendPacket(C15PacketClientSettings{language, renderDistance,
        EnumChatVisibility::fromId(getInt("chatVisibility")), getBool("chatColors"), i});
}

void GameSettings::setFOV(const float _fov) {
    fov = _fov;
    mc->renderer->updateProjection(_fov);
}

void GameSettings::setRenderDistance(const int amount) const {
    const int clamped = std::clamp(amount, 2, 32);
    if (renderDistance == clamped) return;
    renderDistance = clamped;
    computeSqrRenderSize();
    mc->renderer->updateProjection(mc->renderer->width, mc->renderer->height, fov);
    if (mc->netClient != nullptr) sendSettingsToServer();
}
