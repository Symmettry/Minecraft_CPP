#ifndef MINECRAFTCLIENT_MINECRAFT_HPP
#define MINECRAFTCLIENT_MINECRAFT_HPP

#include "world/World.hpp"
#include "entity/player/EntityPlayer.hpp"
#include "render/Camera.hpp"
#include "render/Renderer.hpp"
#include "util/Timer.hpp"
#include <chrono>
#include <thread>

#include "render/FontRenderer.hpp"
#include "util/GameSettings.hpp"
#include "util/Inputs.hpp"

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

    bool running = true;

    Minecraft() {
        renderer->init();
        input = new Input(renderer->window);
        settings = new GameSettings(renderer->window);

        Shader textShader("assets/shaders/text.vert", "assets/shaders/text.frag");
        fontRenderer = new FontRenderer("assets/minecraft/textures/font/ascii.png", &textShader);

        world->entities.push_back(player);

        for (int x=-10;x<10;x++) {
            for (int z=-10;z<10;z++) {
                world->setBlockAt(x, 0, z, BlockType::Grass);
                if ((x + z) % 2 == 0) {
                    world->setBlockAt(x, 10, z, BlockType::Dirt);
                }
            }
        }
        for (auto &val: world->chunks | std::views::values) {
            val.generateMesh();
            val.uploadMesh();
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
        constexpr double targetFPS = 60.0;
        const auto frameDuration = std::chrono::duration<double>(1.0 / targetFPS);

        auto lastFrameTime = clock::now();

        while (running && !renderer->shouldClose()) {
            if (const auto now = clock::now(); now - lastFrameTime >= frameDuration) {
                lastFrameTime = now;
                timer->calculatePartialTicks(now);
                render();
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
    }

    void render() const {
        renderer->render(world);
    }

    void runTick() const {
        input->updateMouse();
        world->update();
    }
};

#endif