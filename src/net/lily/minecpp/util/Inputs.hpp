#define FLOAT static_cast<float>

#ifndef MINECRAFTCLIENT_INPUTS_H
#define MINECRAFTCLIENT_INPUTS_H

#include <atomic>
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
        lastMouseX = centerX;
        lastMouseY = centerY;
    }

    void updateMouse() {
        if (!mouseGrabbed.load(std::memory_order_acquire)) {
            clearMouseDelta();
            return;
        }

        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (!mouseGrabbed.load(std::memory_order_acquire)) {
            clearMouseDelta();
            return;
        }

        mouseDeltaX.store(FLOAT(mouseX - lastMouseX), std::memory_order_release);
        mouseDeltaY.store(FLOAT(mouseY - lastMouseY), std::memory_order_release);
        lastMouseX = mouseX;
        lastMouseY = mouseY;
    }

    void setMouseGrabbed(const bool grabbed) {
        const bool previous = mouseGrabbed.exchange(grabbed, std::memory_order_acq_rel);
        if (previous == grabbed) return;
        clearMouseDelta();

        int width = 1;
        int height = 1;
        glfwGetWindowSize(window, &width, &height);
        centerX = width / 2;
        centerY = height / 2;
        glfwSetInputMode(window, GLFW_CURSOR, grabbed ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        glfwSetCursorPos(window, centerX, centerY);
        lastMouseX = centerX;
        lastMouseY = centerY;
    }

    void recenterMouse() {
        int width = 1;
        int height = 1;
        glfwGetWindowSize(window, &width, &height);
        centerX = width / 2;
        centerY = height / 2;
        clearMouseDelta();
        glfwSetCursorPos(window, centerX, centerY);
        lastMouseX = centerX;
        lastMouseY = centerY;
    }

    [[nodiscard]] float getMouseDeltaX() const {
        return mouseGrabbed.load(std::memory_order_acquire) && glfwGetWindowAttrib(window, GLFW_FOCUSED)
            ? mouseDeltaX.load(std::memory_order_acquire) : 0.0f;
    }
    [[nodiscard]] float getMouseDeltaY() const {
        return mouseGrabbed.load(std::memory_order_acquire) && glfwGetWindowAttrib(window, GLFW_FOCUSED)
            ? mouseDeltaY.load(std::memory_order_acquire) : 0.0f;
    }

    void consumeMouseDelta() {
        clearMouseDelta();
    }

private:
    void clearMouseDelta() {
        mouseDeltaX.store(0.0f, std::memory_order_release);
        mouseDeltaY.store(0.0f, std::memory_order_release);
    }

    GLFWwindow* window;
    std::atomic_bool mouseGrabbed{true};
    std::atomic<float> mouseDeltaX{0.0f};
    std::atomic<float> mouseDeltaY{0.0f};
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
};

#endif
