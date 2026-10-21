#ifndef MINECRAFTCLIENT_MINECRAFT_HPP
#define MINECRAFTCLIENT_MINECRAFT_HPP

#include "client/entity/EntityPlayerSP.hpp"
#include "render/Camera.hpp"
#include "render/Renderer.hpp"
#include "util/Timer.hpp"
#include <atomic>
#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>

#include "menu/Gui.hpp"
#include "net/NetClient.hpp"
#include "render/FontRenderer.hpp"
#include "util/GameSettings.hpp"
#include "util/Inputs.hpp"
#include "util/MovingObjectPosition.hpp"
#include "util/ThreadPool.hpp"

constexpr int _width = 800, _height = 800;

class World;

struct Minecraft {
private:
    mutable Gui* currentScreen = nullptr;
    mutable std::atomic_bool guiRefreshPending = false;

public:
    struct FrameMetrics {
        double frameMs = 0.0;
        double guiMs = 0.0;
        double worldMs = 0.0;
        double swapMs = 0.0;
        double eventsMs = 0.0;
        std::array<float, 240> history{};
        size_t historyIndex = 0;
        size_t historyCount = 0;
    };

    mutable FrameMetrics frameMetrics;
    mutable std::atomic_size_t activeParticleCount{0};
    uint8_t maxPlayers = 1;

    Timer* timer = new Timer();
    World* world = nullptr;
    std::shared_ptr<EntityPlayerSP> player = std::make_shared<EntityPlayerSP>(this, 0.5, 1, 0.5);

    Camera* camera = new Camera(0, 0, 0, player);
    Renderer* renderer = new Renderer(this, camera, _width, _height);
    GameSettings* settings = nullptr;
    Input* input = nullptr;
    FontRenderer* fontRenderer = nullptr;
    NetClient* netClient{};

    mutable MovingObjectPosition objectMouseOver{};
    mutable Entity* pointedEntity = nullptr;

    mutable bool running = true;
    const std::thread::id mainThreadId = std::this_thread::get_id();
    mutable std::mutex scheduledTaskMutex;
    mutable std::deque<std::function<void()>> scheduledTasks;

    static Minecraft* getMinecraft();
    static ThreadPool threadPool;
    static ThreadPool chunkLightingThread;

    Shader* textShader;

    explicit Minecraft(const std::string& serverIp = "", const uint16_t serverPort = 25565) {

        renderer->init();

        input = new Input(renderer->window);
        settings = new GameSettings(renderer->window, this);
        renderer->updateProjection(renderer->width, renderer->height, settings->getFOV());

        textShader = new Shader("assets/shaders/text.vert", "assets/shaders/text.frag");
        fontRenderer = new FontRenderer("assets/minecraft/textures/font/ascii.png", textShader);

        BlockUtil::loadBlockTextures(renderer->blockAtlas.second, "assets/blocktextures.dat.zst");

        if (!serverIp.empty()) {
            netClient = new NetClient(serverIp, serverPort, player->username);
            netClient->connect(this);
        }
    }

    ~Minecraft() {
        running = false;
        if (netClient != nullptr) netClient->disconnect();
        clearScheduledTasks();
        delete netClient;
        delete settings;
        delete input;
    }

    void setRenderViewEntity(const std::shared_ptr<Entity>& entity) const {
        camera->setRenderViewEntity(entity);
    }
    std::shared_ptr<Entity>& getRenderViewEntity() const {
        return camera->getRenderViewEntity();
    }

    int width() const {
        return renderer->width;
    }
    int height() const {
        return renderer->height;
    }

    void displayGuiScreen(Gui* gui) const;
    [[nodiscard]] Gui* getCurrentScreen() const { return currentScreen; }
    void renderIngameHud() const;
    void requestGuiRefresh() const { guiRefreshPending.store(true, std::memory_order_release); }
    [[nodiscard]] bool isCallingFromMainThread() const {
        return std::this_thread::get_id() == mainThreadId;
    }
    void addScheduledTask(std::function<void()> task) const {
        std::lock_guard lock(scheduledTaskMutex);
        scheduledTasks.push_back(std::move(task));
    }
    void clearScheduledTasks() const {
        std::lock_guard lock(scheduledTaskMutex);
        scheduledTasks.clear();
    }
    void openPauseMenu() const;
    [[nodiscard]] bool isSingleplayer() const { return world != nullptr && netClient == nullptr; }
    [[nodiscard]] bool isGamePaused() const { return currentScreen != nullptr && currentScreen->pausesGame(); }
    bool connectToServer(const std::string& serverIp, uint16_t serverPort = 25565);

    bool handleKey(int key, int scancode, int action, int mods) const;
    bool handleChar(unsigned int codepoint) const;
    bool handleMouseButton(int button, int action, int mods) const;
    bool handleCursorPos(double x, double y) const;
    bool handleScroll(double xOffset, double yOffset) const;

    void init() const {
        running = true;
        runRenderLoop();
        running = false;
    }

    void runRenderLoop() const {
        using clock = std::chrono::high_resolution_clock;

        int frames = 0;
        auto lastFpsTime = clock::now();
        timer->lastTick = lastFpsTime;

        while (running && !renderer->shouldClose()) {
            const auto now = clock::now();
            const auto tickDuration = std::chrono::duration<double>(TICK_SPEED * timer->timer);

            if (isGamePaused()) {
                // don't accumulate a backlog which would be replayed after leaving a singleplayer pause screen
                timer->lastTick = now;
            } else {
                int elapsedTicks = 0;
                while (now - timer->lastTick >= tickDuration && elapsedTicks < 10) {
                    timer->lastTick += std::chrono::duration_cast<Time::duration>(tickDuration);
                    runTick();
                    ++elapsedTicks;
                }

                if (elapsedTicks == 10 && now - timer->lastTick >= tickDuration) {
                    timer->lastTick = now;
                }

                const std::chrono::duration<double> elapsed = now - timer->lastTick;
                timer->partialTicks = std::clamp(elapsed.count() / tickDuration.count(), 0.0, 1.0);
            }

            render();
            frames++;

            if (now - lastFpsTime >= std::chrono::seconds(1)) {
                renderer->fps = frames;
                frames = 0;
                lastFpsTime = now;
            }
        }
    }

    void render() const;

    static std::deque<MeshUploadJob> meshUploadQueue;
    static std::vector<MeshDeleteJob> meshDeleteQueue;
    static std::mutex meshQueueMutex;

    struct ChunkDispatcherStats {
        size_t pendingCompiles;
        size_t pendingUploads;
        size_t availableBuilders;
    };

    [[nodiscard]] ChunkDispatcherStats chunkDispatcherStats() const {
        const size_t active = chunkLightingThread.activeJobCount();
        const size_t workers = chunkLightingThread.threadCount();
        size_t pendingUploads;
        {
            std::lock_guard lock(meshQueueMutex);
            pendingUploads = meshUploadQueue.size();
        }
        return {chunkLightingThread.pendingJobCount() + active, pendingUploads,
            workers > active ? workers - active : 0};
    }

    void runTick() const;
};

#endif
