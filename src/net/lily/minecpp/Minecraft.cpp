#include "menu/impl/GuiMainMenu.hpp"
#include "menu/Gui.hpp"

#include "Minecraft.hpp"

#include "menu/impl/GuiIngame.hpp"
#include "menu/impl/GuiIngameMenu.hpp"
#include "menu/impl/GuiGameOver.hpp"
#include "util/CrashHandler.hpp"
#include <tracy/Tracy.hpp>

static Minecraft* MineInst;
static GuiIngame* ingameGui;

ThreadPool Minecraft::threadPool(std::max(1U, std::thread::hardware_concurrency()), "General worker");
ThreadPool Minecraft::chunkLightingThread(
    std::clamp(std::thread::hardware_concurrency() / 6, 1U, 4U), "Chunk lighting");

Minecraft* Minecraft::getMinecraft() {
    return MineInst;
}

void Minecraft::render() const {
    ZoneScopedN("Frame");
    using clock = std::chrono::high_resolution_clock;
    const auto frameStart = clock::now();
    if (guiRefreshPending.exchange(false, std::memory_order_acq_rel)) displayGuiScreen(nullptr);

    std::vector<MeshUploadJob> uploads;
    std::vector<MeshDeleteJob> deletions;
    {
        ZoneScopedN("Mesh queue");
        std::lock_guard lock(meshQueueMutex);
        constexpr size_t uploadsPerFrame = 4;
        const size_t uploadCount = std::min(uploadsPerFrame, meshUploadQueue.size());
        renderer->chunkUpdates = uploadCount;
        uploads.reserve(uploadCount);
        for (size_t index = 0; index < uploadCount; ++index) {
            uploads.push_back(std::move(meshUploadQueue.front()));
            meshUploadQueue.pop_front();
        }
        deletions.swap(meshDeleteQueue);
    }

    for (const auto&[vao, vbo] : deletions) {
        if (vao != 0) glDeleteVertexArrays(1, &vao);
        if (vbo != 0) glDeleteBuffers(1, &vbo);
    }

    for (const auto&[chunk] : uploads) {
        if (chunk == nullptr) continue;
        chunk->uploadMesh();
    }

    if (currentScreen) {
        ZoneScopedN("GUI render");
        const auto start = clock::now();
        currentScreen->render();
        frameMetrics.guiMs = std::chrono::duration<double, std::milli>(clock::now() - start).count();
    }

    {
        ZoneScopedN("Swap buffers");
        const auto start = clock::now();
        glfwSwapBuffers(renderer->window);
        frameMetrics.swapMs = std::chrono::duration<double, std::milli>(clock::now() - start).count();
    }
    {
        ZoneScopedN("Poll events");
        const auto start = clock::now();
        glfwPollEvents();
        frameMetrics.eventsMs = std::chrono::duration<double, std::milli>(clock::now() - start).count();
    }
    frameMetrics.frameMs = std::chrono::duration<double, std::milli>(clock::now() - frameStart).count();
    frameMetrics.history[frameMetrics.historyIndex] = static_cast<float>(frameMetrics.frameMs);
    frameMetrics.historyIndex = (frameMetrics.historyIndex + 1) % frameMetrics.history.size();
    frameMetrics.historyCount = std::min(frameMetrics.historyCount + 1, frameMetrics.history.size());
    FrameMark;
}

std::deque<MeshUploadJob> Minecraft::meshUploadQueue{};
std::vector<MeshDeleteJob> Minecraft::meshDeleteQueue{};
std::mutex Minecraft::meshQueueMutex{};

void Minecraft::runTick() const {
    ZoneScopedN("Game tick");
    std::deque<std::function<void()>> tasks;
    {
        std::lock_guard lock(scheduledTaskMutex);
        tasks.swap(scheduledTasks);
    }
    for (auto& task : tasks) task();
    if (currentScreen) currentScreen->tick();
    if (world != nullptr) {
        renderer->updateEquippedItem();
        renderer->tickParticles(world);
    }
}

void Minecraft::displayGuiScreen(Gui* gui) const {
    if (!gui) {
        if (world == nullptr) {
            gui = new GuiMainMenu();
        } else if (player->health <= 0.0f) {
            gui = new GuiGameOver();
        } else {
            if (!ingameGui) ingameGui = new GuiIngame();
            gui = ingameGui;
        }
    }

    if (currentScreen != nullptr && currentScreen != gui) {
        currentScreen->onClose();
    }

    currentScreen = gui;
    if (currentScreen) currentScreen->onOpen();

    const bool capturesMouse = currentScreen != nullptr && currentScreen->capturesMouse();
    settings->inputEnabled = capturesMouse;
    input->setMouseGrabbed(capturesMouse);
}

void Minecraft::renderIngameHud() const {
    if (ingameGui != nullptr) ingameGui->renderHud();
}

void Minecraft::openPauseMenu() const {
    if (world == nullptr) return;
    displayGuiScreen(new GuiIngameMenu());
}

bool Minecraft::connectToServer(const std::string& serverIp, const uint16_t serverPort) {
    if (netClient != nullptr) {
        if (!netClient->hasFailed()) return false;
        netClient->disconnect();
        clearScheduledTasks();
        delete netClient;
        netClient = nullptr;
    }

    netClient = new NetClient(serverIp, serverPort, player->username);
    if (netClient->connect(this)) return true;

    netClient->disconnect();
    clearScheduledTasks();
    delete netClient;
    netClient = nullptr;
    return false;
}

bool Minecraft::handleKey(const int key, const int scancode, const int action, const int mods) const {
    return currentScreen != nullptr && currentScreen->handleKey(key, scancode, action, mods);
}

bool Minecraft::handleChar(const unsigned int codepoint) const {
    return currentScreen != nullptr && currentScreen->handleChar(codepoint);
}

bool Minecraft::handleMouseButton(const int button, const int action, const int mods) const {
    return currentScreen != nullptr && currentScreen->handleMouseButton(button, action, mods);
}

bool Minecraft::handleCursorPos(const double x, const double y) const {
    return currentScreen != nullptr && currentScreen->handleCursorPos(x, y);
}

bool Minecraft::handleScroll(const double xOffset, const double yOffset) const {
    return currentScreen != nullptr && currentScreen->handleScroll(xOffset, yOffset);
}

int main() {
    installCrashHandlers();
    MineInst = new Minecraft();
    Gui::init();

    GLFWwindow* window = MineInst->renderer->window;
    glfwSetKeyCallback(window, [](GLFWwindow*, const int key, const int scancode, const int action, const int mods) {
        MineInst->handleKey(key, scancode, action, mods);
    });
    glfwSetCharCallback(window, [](GLFWwindow*, const unsigned int codepoint) {
        MineInst->handleChar(codepoint);
    });
    glfwSetMouseButtonCallback(window, [](GLFWwindow*, const int button, const int action, const int mods) {
        MineInst->handleMouseButton(button, action, mods);
    });
    glfwSetCursorPosCallback(window, [](GLFWwindow*, const double x, const double y) {
        MineInst->handleCursorPos(x, y);
    });
    glfwSetScrollCallback(window, [](GLFWwindow*, const double xOffset, const double yOffset) {
        MineInst->handleScroll(xOffset, yOffset);
    });

    MineInst->displayGuiScreen(nullptr);
    MineInst->init();
    return 0;
}
