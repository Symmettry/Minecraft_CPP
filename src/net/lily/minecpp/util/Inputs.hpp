#define FLOAT static_cast<float>

#ifndef MINECRAFTCLIENT_INPUTS_H
#define MINECRAFTCLIENT_INPUTS_H

#include <GLFW/glfw3.h>

static int centerX = 0, centerY = 0;

struct Input {
    explicit Input(GLFWwindow* window)
        : window(window)
    {
        glfwGetWindowSize(window, &centerX, &centerY);
        centerX /= 2; centerY /= 2;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        glfwSetCursorPos(window, centerX, centerY);

        glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, const int width, const int height) {
            centerX = width/2;
            centerY = height/2;
        });
    }

    void updateMouse() {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        mouseDeltaX = FLOAT(mouseX - centerX);
        mouseDeltaY = FLOAT(mouseY - centerY);

        glfwSetCursorPos(window, centerX, centerY);
    }

    [[nodiscard]] float getMouseDeltaX() const {
        return glfwGetWindowAttrib(window, GLFW_FOCUSED) ? mouseDeltaX : 0;
    }
    [[nodiscard]] float getMouseDeltaY() const {
        return glfwGetWindowAttrib(window, GLFW_FOCUSED) ? mouseDeltaY : 0;
    }

private:
    GLFWwindow* window;
    float mouseDeltaX{}, mouseDeltaY{};
};

#endif
