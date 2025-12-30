#ifndef MINECRAFTCLIENT_GAMESETTINGS_H
#define MINECRAFTCLIENT_GAMESETTINGS_H
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "../world/World.hpp"

struct KeybindSetting {
    int key;
};
struct NumberSetting {
    float value;
};

struct Minecraft;

struct GameSettings {
    explicit GameSettings(GLFWwindow* window, const Minecraft* mc) : window(window), mc(mc) {};

    const KeybindSetting moveForward {GLFW_KEY_W};
    const KeybindSetting moveBackward{GLFW_KEY_S};
    const KeybindSetting moveLeft    {GLFW_KEY_A};
    const KeybindSetting moveRight   {GLFW_KEY_D};
    const KeybindSetting jump        {GLFW_KEY_SPACE};
    const KeybindSetting sneak       {GLFW_KEY_LEFT_SHIFT};
    const KeybindSetting sprint      {GLFW_KEY_R};

    const NumberSetting  mouseSensitivity{0.3};
    EnumDifficulty::Value difficulty = EnumDifficulty::NORMAL;

    std::string language = "en_es";
    int renderDistance = 16;
    bool chatVisibility = true, chatColors = true;
    bool modelParts[7] = {true};

    [[nodiscard]] bool isKeyDown(const KeybindSetting setting) const {
        return glfwGetKey(window, setting.key) == GLFW_PRESS;
    }

    void sendSettingsToServer() const;

private:
    GLFWwindow* window;
    const Minecraft *mc;

};

#endif //MINECRAFTCLIENT_GAMESETTINGS_H