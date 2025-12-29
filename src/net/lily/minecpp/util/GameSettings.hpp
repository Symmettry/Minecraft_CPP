#ifndef MINECRAFTCLIENT_GAMESETTINGS_H
#define MINECRAFTCLIENT_GAMESETTINGS_H
#include <GLFW/glfw3.h>

struct KeybindSetting {
    int key;
};
struct NumberSetting {
    float value;
};

struct GameSettings {
    explicit GameSettings(GLFWwindow* window) : window(window) {};

    const KeybindSetting moveForward {GLFW_KEY_W};
    const KeybindSetting moveBackward{GLFW_KEY_S};
    const KeybindSetting moveLeft    {GLFW_KEY_A};
    const KeybindSetting moveRight   {GLFW_KEY_D};
    const KeybindSetting jump        {GLFW_KEY_SPACE};
    const KeybindSetting sneak       {GLFW_KEY_LEFT_SHIFT};
    const KeybindSetting sprint      {GLFW_KEY_R};

    const NumberSetting  mouseSensitivity{0.3};
    EnumDifficulty::Value difficulty = EnumDifficulty::NORMAL;

    [[nodiscard]] bool isKeyDown(const KeybindSetting setting) const {
        return glfwGetKey(window, setting.key) == GLFW_PRESS;
    }

private:
    GLFWwindow* window;

};

#endif //MINECRAFTCLIENT_GAMESETTINGS_H