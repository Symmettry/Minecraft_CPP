#ifndef MINECRAFTCLIENT_MINECRAFT_HPP
#define MINECRAFTCLIENT_MINECRAFT_HPP

#include "world/World.hpp"
#include "entity/player/EntityPlayer.hpp"
#include "render/Camera.hpp"
#include "render/Renderer.hpp"
#include "util/Timer.hpp"
#include <chrono>
#include <thread>

#include "net/NetClient.hpp"
#include "render/FontRenderer.hpp"
#include "util/GameSettings.hpp"
#include "util/Inputs.hpp"
#include "world/block/BlockRegistry.hpp"

constexpr int width = 800, height = 800;

struct Minecraft {
    Timer* timer = new Timer();
    World* world = new World();
    std::shared_ptr<EntityPlayer> player = std::make_shared<EntityPlayer>(this, 0, 1, 0);

    Camera* camera = new Camera(0, 0, 0, player);
    Renderer* renderer = new Renderer(this, camera, width, height);
    GameSettings* settings;
    Input* input;
    FontRenderer* fontRenderer;
    NetClient* netClient = nullptr;

    bool running = true;

    explicit Minecraft(const std::string& serverIp = "", uint16_t serverPort = 25565) {

        BlockRegistry::initialize();

        renderer->init();
        input = new Input(renderer->window);
        settings = new GameSettings(renderer->window);

        Shader textShader("assets/shaders/text.vert", "assets/shaders/text.frag");
        fontRenderer = new FontRenderer("assets/minecraft/textures/font/ascii.png", &textShader);

        world->entities.push_back(player);

        for (int x=-16;x<16;x++) {
            for (int z=-16;z<16;z++) {
                world->setBlockAt(x, 0, z, Material::Grass);
                if ((x + z) % 2 == 0) {
                    world->setBlockAt(x, 10, z, Material::Dirt);
                }
            }
        }
        world->setBlockAt(5, 1, 5, Material::Grass);
        for (auto &val: world->chunks | std::views::values) {
            val.generateMesh(renderer->blockAtlas);
            val.uploadMesh();
        }

        if (!serverIp.empty()) {
            netClient = new NetClient(serverIp, serverPort, player->username);
            netClient->connect();
        }

        running = true;
        std::thread tickThread(&Minecraft::runTickLoop, this);
        runRenderLoop();
        running = false;
        tickThread.join();
    }

    ~Minecraft() {
        running = false;
        delete settings;
        delete input;
        delete netClient;
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
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

            // Calculate FPS every second
            if (now - lastFpsTime >= std::chrono::seconds(1)) {
                std::cout << "FPS: " << frames << std::endl;
                frames = 0;
                lastFpsTime = now;
            }
        }
    }


    void render() const {
        renderer->render(world);
    }

    void runTick() const {
        input->updateMouse();
        world->update();

        if (netClient) {
            netClient->tick();
        }
    }
};

#endif
