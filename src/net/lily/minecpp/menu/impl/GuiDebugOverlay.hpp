#pragma once

#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <fstream>
#include <malloc.h>
#include <sys/sysinfo.h>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/util/DebugNames.hpp"

class GuiDebugOverlay {
public:
    explicit GuiDebugOverlay(const Minecraft* mc) : mc(mc) {}

    void render(const bool showProfiler, const bool showFrameGraph) const {
        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const float guiWidth = static_cast<float>((mc->width() + factor - 1) / factor);
        const float guiHeight = static_cast<float>((mc->height() + factor - 1) / factor);
        const FontRenderer* font = mc->fontRenderer;
        font->shader->use();
        font->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));

        std::vector<std::string> left{
            "Minecraft 1.8.9 (1.8.9/vanilla)",
            fpsDebugLine(),
            chunkDebugLine(),
            entityDebugLine(),
            "P: " + std::to_string(mc->activeParticleCount.load(std::memory_order_relaxed))
                + ". T: All: " + std::to_string(mc->world->snapshotEntities().size()),
            "MultiplayerChunkCache: " + std::to_string(mc->renderer->totalChunks) + ", "
                + std::to_string(mc->renderer->totalChunks),
            "",
        };

        const bool reduced = mc->settings->getBool("reducedDebugInfo") || mc->player->reducedDebug;
        if (!reduced) appendPosition(left);

        std::vector<std::string> right = systemInformation();
        appendTarget(right, reduced);

        drawLines(left, 2.0f * scale, 2.0f * scale, false);
        drawLines(right, guiWidth * scale - 2.0f * scale, 2.0f * scale, true);
        if (showFrameGraph) drawFrameGraph(guiWidth, guiHeight, scale);
        if (showProfiler) drawProfiler(guiWidth, guiHeight, scale);
    }

    void renderDirections() const {
        const float yaw = glm::radians(mc->player->rotation.yaw);
        const float pitch = glm::radians(mc->player->rotation.pitch);
        const glm::vec3 forward{-std::sin(yaw) * std::cos(pitch),
            -std::sin(pitch), std::cos(yaw) * std::cos(pitch)};
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3{0.0f, 1.0f, 0.0f}));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        const float centerX = static_cast<float>(mc->width()) * 0.5f;
        const float centerY = static_cast<float>(mc->height()) * 0.5f;
        const float length = 20.0f * static_cast<float>(scaleFactor());
        const float thickness = std::max(1.0f, static_cast<float>(scaleFactor()));
        std::vector<ColorVertex> vertices;
        vertices.reserve(18);
        const auto appendAxis = [&](const glm::vec3 axis, const glm::vec3 color) {
            const glm::vec2 delta{glm::dot(axis, right) * length, -glm::dot(axis, up) * length};
            if (glm::dot(delta, delta) < 0.0001f) return;
            const glm::vec2 normal = glm::normalize(glm::vec2{-delta.y, delta.x}) * thickness * 0.5f;
            const glm::vec2 start{centerX, centerY};
            const glm::vec2 end = start + delta;
            const auto vertex = [&](const glm::vec2 point) {
                return ColorVertex{point.x / mc->width() * 2.0f - 1.0f,
                    1.0f - point.y / mc->height() * 2.0f, color.r, color.g, color.b, 1.0f};
            };
            vertices.insert(vertices.end(), {vertex(start - normal), vertex(end - normal), vertex(end + normal),
                vertex(end + normal), vertex(start + normal), vertex(start - normal)});
        };
        appendAxis({1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f});
        appendAxis({0.0f, 0.66f, 0.0f}, {0.0f, 1.0f, 0.0f});
        appendAxis({0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f});
        const GLboolean cullingEnabled = glIsEnabled(GL_CULL_FACE);
        glDisable(GL_CULL_FACE);
        drawColored(vertices);
        if (cullingEnabled) glEnable(GL_CULL_FACE);
    }

private:
    struct ColorVertex { float x, y, r, g, b, a; };
    const Minecraft* mc;
    static inline std::unique_ptr<Shader> colorShader;
    static inline GLuint colorVao = 0;
    static inline GLuint colorVbo = 0;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    static std::string fixed(const double value, const int precision) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(precision) << value;
        return stream.str();
    }

    [[nodiscard]] std::string entityDebugLine() const {
        const size_t ignored = mc->renderer->totalEntities - std::min(mc->renderer->totalEntities,
            mc->renderer->renderedEntities + mc->renderer->hiddenEntities);
        return "E: " + std::to_string(mc->renderer->renderedEntities) + "/"
            + std::to_string(mc->renderer->totalEntities) + ", B: "
            + std::to_string(mc->renderer->hiddenEntities) + ", I: " + std::to_string(ignored);
    }

    [[nodiscard]] std::string fpsDebugLine() const {
        const int maximumFps = mc->settings->getInt("maxFps");
        std::string line = std::to_string(mc->renderer->fps) + " fps ("
            + std::to_string(mc->renderer->chunkUpdates) + " chunk update"
            + (mc->renderer->chunkUpdates == 1 ? "" : "s") + ") T: "
            + (maximumFps >= 260 ? "inf" : std::to_string(maximumFps));
        switch (mc->settings->getInt("renderClouds")) {
            case 1: line += " fast-clouds"; break;
            case 2: line += " fancy-clouds"; break;
            default: break;
        }
        return line;
    }

    [[nodiscard]] std::string chunkDebugLine() const {
        const Minecraft::ChunkDispatcherStats stats = mc->chunkDispatcherStats();
        std::ostringstream line;
        line << "C: " << mc->renderer->renderedChunks << '/' << mc->renderer->totalChunks
             << " (s) D: " << mc->settings->getRenderDistance() << ", pC: "
             << std::setfill('0') << std::setw(3) << stats.pendingCompiles << std::setfill(' ')
             << ", pU: " << stats.pendingUploads << ", aB: " << stats.availableBuilders;
        return line.str();
    }

    static std::string cpuName() {
        static const std::string name = [] {
            std::ifstream cpuInfo("/proc/cpuinfo");
            std::string line;
            while (std::getline(cpuInfo, line)) {
                constexpr std::string_view prefix = "model name";
                if (!line.starts_with(prefix)) continue;
                const size_t separator = line.find(':');
                if (separator != std::string::npos) return line.substr(separator + 2);
            }
            return std::string("unknown");
        }();
        return name;
    }

    [[nodiscard]] std::vector<std::string> systemInformation() const {
        const GLubyte* vendor = glGetString(GL_VENDOR);
        const GLubyte* renderer = glGetString(GL_RENDERER);
        const GLubyte* version = glGetString(GL_VERSION);
        const auto heap = mallinfo2();
        struct sysinfo system{};
        sysinfo(&system);
        const uint64_t maximum = std::max<uint64_t>(system.totalram * system.mem_unit, 1);
        const uint64_t used = heap.uordblks;
        const uint64_t allocated = static_cast<uint64_t>(heap.arena) + heap.hblkhd;
        const auto memoryLine = [](const std::string& label, const uint64_t bytes,
                                   const uint64_t max, const bool showMaximum) {
            std::ostringstream text;
            text << label << ": " << std::setw(2) << bytes * 100 / max << "% "
                 << std::setw(3) << bytes / 1024 / 1024;
            if (showMaximum) text << "/" << std::setw(3) << max / 1024 / 1024;
            text << "MB";
            return text.str();
        };
        return {
            "C++26 " + std::string(sizeof(void*) == 8 ? "64bit" : "32bit"),
            memoryLine("Mem", used, maximum, true),
            memoryLine("Allocated", allocated, maximum, false),
            "",
            "CPU: " + std::to_string(std::max(1U, std::thread::hardware_concurrency())) + "x " + cpuName(),
            "",
            "Display: " + std::to_string(mc->width()) + "x" + std::to_string(mc->height()) + " ("
                + (vendor == nullptr ? "Unknown" : reinterpret_cast<const char*>(vendor)) + ")",
            renderer == nullptr ? "Unknown GPU" : reinterpret_cast<const char*>(renderer),
            version == nullptr ? "Unknown OpenGL" : reinterpret_cast<const char*>(version),
        };
    }

    void appendPosition(std::vector<std::string>& lines) const {
        const auto& position = mc->player->position;
        const auto wrapDegrees = [](const float angle) {
            float wrapped = std::fmod(angle, 360.0f);
            if (wrapped >= 180.0f) wrapped -= 360.0f;
            if (wrapped < -180.0f) wrapped += 360.0f;
            return wrapped;
        };
        const float yaw = wrapDegrees(mc->player->rotation.yaw);
        const float pitch = std::clamp(wrapDegrees(mc->player->rotation.pitch), -90.0f, 90.0f);
        const int blockX = static_cast<int>(std::floor(position.x));
        const int blockY = static_cast<int>(std::floor(position.y));
        const int blockZ = static_cast<int>(std::floor(position.z));
        const int chunkX = static_cast<int>(std::floor(static_cast<double>(blockX) / CHUNK_SIZE));
        const int chunkZ = static_cast<int>(std::floor(static_cast<double>(blockZ) / CHUNK_SIZE));
        const int yawIndex = static_cast<int>(std::floor(yaw * 4.0f / 360.0f + 0.5f)) & 3;
        static constexpr const char* directions[]{"south (Towards positive Z)", "west (Towards negative X)",
            "north (Towards negative Z)", "east (Towards positive X)"};
        lines.push_back("XYZ: " + fixed(position.x, 3) + " / " + fixed(position.y, 5) + " / " + fixed(position.z, 3));
        lines.push_back("Block: " + std::to_string(blockX) + " " + std::to_string(blockY) + " " + std::to_string(blockZ));
        lines.push_back("Chunk: " + std::to_string(World::toLocalX(blockX)) + " "
            + std::to_string(blockY & 15) + " " + std::to_string(World::toLocalZ(blockZ)) + " in "
            + std::to_string(chunkX) + " " + std::to_string(blockY >> 4) + " " + std::to_string(chunkZ));
        lines.push_back(std::string("Facing: ") + directions[yawIndex] + " (" + fixed(yaw, 1)
            + " / " + fixed(pitch, 1) + ")");
        lines.push_back("Biome: " + DebugNames::biomeName(mc->world->getBiomeAt(blockX, blockZ)));
        if (const auto chunk = mc->world->getLoadedChunk(chunkX, chunkZ);
            chunk != nullptr && blockY >= 0 && blockY < WORLD_HEIGHT) {
            const uint32_t light = chunk->getLightData(World::toLocalX(blockX), blockY, World::toLocalZ(blockZ));
            lines.push_back("Light: " + std::to_string(std::max((light >> 8U) & 15U, (light >> 12U) & 15U))
                + " (" + std::to_string((light >> 12U) & 15U) + " sky, "
                + std::to_string((light >> 8U) & 15U) + " block)");
        }
        if (mc->objectMouseOver.valid && mc->objectMouseOver.typeOfHit == MovingObjectPosition::BLOCK) {
            const glm::ivec3 pos = mc->objectMouseOver.blockPos;
            lines.push_back("Looking at: " + std::to_string(pos.x) + " " + std::to_string(pos.y)
                + " " + std::to_string(pos.z));
        }
    }

    void appendTarget(std::vector<std::string>& lines, const bool reduced) const {
        if (reduced || !mc->objectMouseOver.valid
            || mc->objectMouseOver.typeOfHit != MovingObjectPosition::BLOCK) return;
        const glm::ivec3 pos = mc->objectMouseOver.blockPos;
        const Block block = mc->world->getBlockAt(pos);
        lines.emplace_back("");
        lines.push_back(DebugNames::blockRegistryName(blockId(block)));
        switch (blockId(block)) {
            case blockId(BLOCK_GRASS): {
                const Block above = mc->world->getBlockAt(pos + glm::ivec3(0, 1, 0));
                const bool snowy = blockId(above) == blockId(BLOCK_SNOW)
                    || blockId(above) == blockId(BLOCK_SNOW_LAYER);
                lines.push_back(std::string("snowy: ") + (snowy ? '\1' : '\2')
                    + (snowy ? "true" : "false"));
                break;
            }
            case blockId(BLOCK_SNOW_LAYER):
                lines.push_back("layers: " + std::to_string((blockMeta(block) & 7) + 1));
                break;
            default:
                lines.push_back("metadata: " + std::to_string(blockMeta(block)));
                break;
        }
    }

    void drawLines(const std::vector<std::string>& lines, const float anchorX,
                   const float startY, const bool rightAligned) const {
        const float scale = static_cast<float>(scaleFactor());
        float y = startY;
        for (const std::string& line : lines) {
            if (!line.empty()) {
                const size_t colorMarker = line.find_first_of("\1\2");
                const std::string visibleLine = colorMarker == std::string::npos
                    ? line : line.substr(0, colorMarker) + line.substr(colorMarker + 1);
                const float width = mc->fontRenderer->getStringWidth(visibleLine) * scale;
                const float x = rightAligned ? anchorX - width : anchorX;
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                Gui::drawRect(x - scale, y - scale, width + 2.0f * scale, 9.0f * scale,
                    {80.0f / 255.0f, 80.0f / 255.0f, 80.0f / 255.0f, 144.0f / 255.0f});
                if (colorMarker == std::string::npos) {
                    mc->fontRenderer->renderText(line, x, y, scale, 0xFFE0E0E0, false);
                } else {
                    const std::string prefix = line.substr(0, colorMarker);
                    mc->fontRenderer->renderText(prefix, x, y, scale, 0xFFE0E0E0, false);
                    const unsigned int valueColor = line[colorMarker] == '\1' ? 0xFF55FF55 : 0xFFFF5555;
                    mc->fontRenderer->renderText(line.substr(colorMarker + 1),
                        x + mc->fontRenderer->getStringWidth(prefix) * scale,
                        y, scale, valueColor, false);
                }
            }
            y += 9.0f * scale;
        }
    }

    static void initColorRenderer() {
        if (colorVao != 0) return;
        colorShader = std::make_unique<Shader>("assets/shaders/debug_color.vert", "assets/shaders/debug_color.frag");
        glGenVertexArrays(1, &colorVao);
        glGenBuffers(1, &colorVbo);
        glBindVertexArray(colorVao);
        glBindBuffer(GL_ARRAY_BUFFER, colorVbo);
        glBufferData(GL_ARRAY_BUFFER, 4096 * sizeof(ColorVertex), nullptr, GL_STREAM_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(ColorVertex), reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glBindVertexArray(0);
    }

    void drawColored(const std::vector<ColorVertex>& vertices) const {
        if (vertices.empty()) return;
        initColorRenderer();
        colorShader->use();
        glBindVertexArray(colorVao);
        glBindBuffer(GL_ARRAY_BUFFER, colorVbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(ColorVertex)),
            vertices.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
        glBindVertexArray(0);
    }

    void drawFrameGraph(const float guiWidth, const float guiHeight, const float scale) const {
        const float left = 2.0f * scale;
        const float bottom = guiHeight * scale - 2.0f * scale;
        const float graphHeight = 60.0f * scale;
        const float barWidth = std::max(1.0f, (guiWidth * scale - 4.0f * scale) / 240.0f);
        Gui::drawRect(left, bottom - graphHeight, guiWidth * scale - 4.0f * scale, graphHeight,
            {0.0f, 0.0f, 0.0f, 0.55f});
        for (size_t i = 0; i < mc->frameMetrics.historyCount; ++i) {
            const size_t index = (mc->frameMetrics.historyIndex + mc->frameMetrics.history.size()
                - mc->frameMetrics.historyCount + i) % mc->frameMetrics.history.size();
            const float milliseconds = mc->frameMetrics.history[index];
            const float height = std::min(graphHeight, milliseconds / 50.0f * graphHeight);
            const float red = std::clamp(milliseconds / 50.0f, 0.0f, 1.0f);
            Gui::drawRect(left + i * barWidth, bottom - height, barWidth, height,
                {red, 1.0f - red, 0.0f, 1.0f});
        }
        const float line60 = graphHeight * (16.6667f / 50.0f);
        Gui::drawRect(left, bottom - line60, guiWidth * scale - 4.0f * scale, 1.0f,
            {1.0f, 1.0f, 1.0f, 0.8f});
    }

    void drawProfiler(const float guiWidth, const float guiHeight, const float scale) const {
        const auto& metrics = mc->frameMetrics;
        const double world = std::max(metrics.worldMs, 0.0);
        const double hud = std::max(metrics.guiMs - world, 0.0);
        const double swap = std::max(metrics.swapMs, 0.0);
        const double events = std::max(metrics.eventsMs, 0.0);
        const double other = std::max(metrics.frameMs - world - hud - swap - events, 0.0);
        const std::array values{world, hud, swap, events, other};
        constexpr std::array<const char*, 5> names{"gameRenderer", "gui", "swap", "events", "unspecified"};
        constexpr std::array<glm::vec3, 5> colors{{{0.2f,0.7f,1.0f},{0.3f,1.0f,0.3f},{1.0f,0.7f,0.2f},{0.8f,0.3f,1.0f},{0.8f,0.8f,0.8f}}};
        const double total = std::max(metrics.frameMs, 0.0001);
        const float centerX = guiWidth * scale - 105.0f * scale;
        const float centerY = guiHeight * scale - 105.0f * scale;
        const float radius = 75.0f * scale;
        std::vector<ColorVertex> vertices;
        double angle = -M_PI * 0.5;
        for (size_t slice = 0; slice < values.size(); ++slice) {
            const double end = angle + values[slice] / total * M_PI * 2.0;
            const int segments = std::max(1, static_cast<int>(std::ceil((end - angle) * 24.0 / (M_PI * 2.0))));
            for (int segment = 0; segment < segments; ++segment) {
                const double a0 = angle + (end - angle) * segment / segments;
                const double a1 = angle + (end - angle) * (segment + 1) / segments;
                const auto ndc = [&](const float x, const float y, const glm::vec3 color) {
                    return ColorVertex{x / mc->width() * 2.0f - 1.0f, 1.0f - y / mc->height() * 2.0f,
                        color.r, color.g, color.b, 0.9f};
                };
                vertices.push_back(ndc(centerX, centerY, colors[slice]));
                vertices.push_back(ndc(centerX + std::cos(a0) * radius, centerY + std::sin(a0) * radius, colors[slice]));
                vertices.push_back(ndc(centerX + std::cos(a1) * radius, centerY + std::sin(a1) * radius, colors[slice]));
            }
            angle = end;
        }
        drawColored(vertices);
        float labelY = centerY - radius;
        for (size_t i = 0; i < names.size(); ++i) {
            const std::string label = std::string(names[i]) + " " + fixed(values[i] / total * 100.0, 1) + "%";
            mc->fontRenderer->renderText(label, centerX - radius, labelY, scale, 0xFFFFFFFF, false);
            labelY += 10.0f * scale;
        }
    }
};
