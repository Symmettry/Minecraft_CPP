#ifndef MINECRAFTCLIENT_GAMESETTINGS_H
#define MINECRAFTCLIENT_GAMESETTINGS_H
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <string>
#include <unordered_map>

#include "../world/World.hpp"

struct KeybindSetting {
    int key;
};
struct NumberSetting {
    float value;
};

struct Minecraft;

struct GameSettings {
    explicit GameSettings(GLFWwindow* window, const Minecraft* mc) : window(window), mc(mc) {
        setDefaults();
        loadOptions();
        computeSqrRenderSize();
    };

    KeybindSetting moveForward {GLFW_KEY_W};
    KeybindSetting moveBackward{GLFW_KEY_S};
    KeybindSetting moveLeft    {GLFW_KEY_A};
    KeybindSetting moveRight   {GLFW_KEY_D};
    KeybindSetting jump        {GLFW_KEY_SPACE};
    KeybindSetting sneak       {GLFW_KEY_LEFT_SHIFT};
    KeybindSetting sprint      {GLFW_KEY_R};

    NumberSetting mouseSensitivity{0.5f};
    EnumDifficulty::Value difficulty = EnumDifficulty::NORMAL;

    std::string language = "en_es";
    bool chatVisibility = true, chatColors = true;
    bool modelParts[7] = {true};
    bool chunkBoundaries = false;
    int thirdPersonView = 0;
    int guiScale = 0;
    bool inputEnabled = true;
    std::unordered_map<std::string, std::string> options;

    void loadOptions();
    void saveOptions() const;
    [[nodiscard]] bool getBool(const std::string& key) const;
    [[nodiscard]] float getFloat(const std::string& key) const;
    [[nodiscard]] int getInt(const std::string& key) const;
    [[nodiscard]] std::string getOption(const std::string& key) const;
    void setOption(const std::string& key, const std::string& value);
    void setBool(const std::string& key, bool value);
    void setFloat(const std::string& key, float value);

    void setFOV(float _fov);
    float getFOV() const { return fov; }

    void setRenderDistance(int amount) const;
    int getRenderDistance() const {
        return renderDistance;
    }
    int getRenderDistanceSqrSize() const {
        return renderDistanceSqrSize;
    }

    [[nodiscard]] bool isKeyDown(const KeybindSetting setting) const {
        return inputEnabled && glfwGetKey(window, setting.key) == GLFW_PRESS;
    }

    void sendSettingsToServer() const;

private:
    GLFWwindow* window;
    const Minecraft *mc;

    mutable int renderDistance = 64, renderDistanceSqrSize{};
    float fov = 90.0f;

    void computeSqrRenderSize() const {
        renderDistanceSqrSize = renderDistance * renderDistance * CHUNK_SIZE * CHUNK_SIZE;
    }

    void setDefaults();

};

#endif //mINECRAFTCLIENT_GAMESETTINGS_H
