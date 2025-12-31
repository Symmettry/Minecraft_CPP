#ifndef MINECRAFTCLIENT_MINECRAFT_HPP
#define MINECRAFTCLIENT_MINECRAFT_HPP

#include "world/World.hpp"
#include "entity/player/EntityPlayer.hpp"
#include "render/Camera.hpp"
#include "render/Renderer.hpp"
#include "util/Timer.hpp"
#include <chrono>
#include <thread>

#include "menu/Gui.hpp"
#include "net/NetClient.hpp"
#include "render/FontRenderer.hpp"
#include "util/GameSettings.hpp"
#include "util/Inputs.hpp"

constexpr int _width = 800, _height = 800;

struct Minecraft {
private:
    mutable Gui* currentScreen = nullptr;

public:
    uint8_t maxPlayers = 1;

    Timer* timer = new Timer();
    World* world = nullptr;
    std::shared_ptr<EntityPlayer> player = std::make_shared<EntityPlayer>(this, 0.5, 1, 0.5);

    Camera* camera = new Camera(0, 0, 0, player);
    Renderer* renderer = new Renderer(this, camera, _width, _height);
    GameSettings* settings;
    Input* input;
    FontRenderer* fontRenderer;
    NetClient* netClient = nullptr;

    mutable bool running = true;

    static const Minecraft* getMinecraft();

    Shader* textShader;

    explicit Minecraft(const std::string& serverIp = "", uint16_t serverPort = 25565) {

        renderer->init();

        input = new Input(renderer->window);
        settings = new GameSettings(renderer->window, this);

        textShader = new Shader("assets/shaders/text.vert", "assets/shaders/text.frag");
        fontRenderer = new FontRenderer("assets/minecraft/textures/font/ascii.png", textShader);

        // world->entities.push_back(player);

        // // Generate some test world
        // for (int x=-16;x<16;x++) {
        //     for (int z=-16;z<16;z++) {
        //         Material mat;
        //         if (x == 0 || z == 0) mat = Material::Stone;
        //         else if (x > 0 && z > 0) mat = Material::Grass;
        //         else if (x < 0 && z > 0) mat = Material::Dirt;
        //         else if (x < 0) mat = Material::Obsidian;
        //         else mat = Material::Ice;
        //
        //         world->setBlockAt(x, 0, z, mat);
        //         if ((x + z) % 2 == 0) world->setBlockAt(x, 10, z, mat);
        //     }
        // }
        //
        // for (int i=0;i<10;i++) {
        //     world->setBlockAt(3 + i, 1, 3, Material::Ice);
        //     world->setBlockAt(3 + i, 1, 4, Material::Ice);
        // }
        //
        // world->setBlockAt(-196, 69, 254, Material::Dirt);
        // for (auto &val: world->chunks | std::views::values) {
        //     val.generateMesh(renderer->blockAtlas);
        //     val.uploadMesh();
        // }

        if (!serverIp.empty()) {
            netClient = new NetClient(serverIp, serverPort, player->username);
            netClient->connect(this);
        }
    }

    ~Minecraft() {
        running = false;
        delete settings;
        delete input;
        delete netClient;
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

    void init() const {
        running = true;
        std::thread tickThread(&Minecraft::runTickLoop, this);
        runRenderLoop();
        running = false;
        tickThread.join();
    }

    void runTickLoop() const {
        using clock = std::chrono::high_resolution_clock;
        timer->lastTick = clock::now();

        while (running && !renderer->shouldClose()) {
            const auto tickDuration = std::chrono::duration<double>(TICK_SPEED * timer->timer);
            const auto now = clock::now();
            if (auto elapsed = now - timer->lastTick; elapsed >= tickDuration) {
                timer->lastTick = now;
                runTick();
            } else {
                // std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    void runRenderLoop() const {
        using clock = std::chrono::high_resolution_clock;

        int frames = 0;
        auto lastFpsTime = clock::now();

        while (running && !renderer->shouldClose()) {
            auto now = clock::now();
            timer->calculatePartialTicks(now);
            render();
            frames++;

            if (now - lastFpsTime >= std::chrono::seconds(1)) {
                renderer->fps = frames;
                frames = 0;
                lastFpsTime = now;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void render() const;

    static std::vector<MeshUploadJob> meshUploadQueue;
    static std::mutex meshQueueMutex;

    void runTick() const;
};

#endif
