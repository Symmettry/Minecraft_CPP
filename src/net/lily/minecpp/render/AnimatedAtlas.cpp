#include "AnimatedAtlas.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include "lib/stb_image.hpp"
#include "Mipmap.hpp"
#include "net/lily/minecpp/util/json/JsonValue.hpp"
#include "net/lily/minecpp/world/chunk/Block.hpp"

namespace {
int number(const JsonValue& value, const int fallback) {
    if (value.type != JsonValue::Type::Number) return fallback;
    try { return std::stoi(value.text); } catch (...) { return fallback; }
}
}

AnimatedAtlas::AnimatedAtlas(const BlockAtlas& atlas) {
    for (const std::string name : {"water_still", "water_flow", "lava_still", "lava_flow",
             "fire_layer_0", "fire_layer_1", "portal", "prismarine_rough", "sea_lantern"}) {
        Sprite sprite = load(atlas, name);
        if (!sprite.pixels.empty() && !sprite.sequence.empty()) sprites.push_back(std::move(sprite));
    }
}

AnimatedAtlas::Sprite AnimatedAtlas::load(const BlockAtlas& atlas, const std::string& name) {
    Sprite sprite;
    const char* atlasName = BlockUtil::getAtlasTexturePtr(atlas, name);
    const auto position = atlas.blockAtlasPos.find(atlasName);
    if (position == atlas.blockAtlasPos.end()) return sprite;
    sprite.tileX = position->second.first;
    sprite.tileY = position->second.second;

    const std::string base = "assets/minecraft/textures/blocks/" + name + ".png";
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* loaded = stbi_load(base.c_str(), &width, &height, &channels, 4);
    if (loaded == nullptr || width <= 0 || height < width) return sprite;
    sprite.frameSize = width;
    sprite.pixels.assign(loaded, loaded + width * height * 4);
    stbi_image_free(loaded);

    std::ifstream metadata(base + ".mcmeta");
    if (metadata) {
        const std::string source{std::istreambuf_iterator<char>(metadata), std::istreambuf_iterator<char>()};
        try {
            const JsonValue root = JsonValue::parse(source);
            if (const auto animation = root.object.find("animation"); animation != root.object.end()) {
                const auto& object = animation->second.object;
                if (const auto value = object.find("frametime"); value != object.end())
                    sprite.frameTime = std::max(1, number(value->second, 1));
                if (const auto value = object.find("interpolate"); value != object.end())
                    sprite.interpolate = value->second.boolean;
                if (const auto frames = object.find("frames"); frames != object.end()) {
                    for (const JsonValue& frame : frames->second.array) {
                        if (frame.type == JsonValue::Type::Number) {
                            sprite.sequence.push_back({number(frame, 0), sprite.frameTime});
                        } else if (frame.type == JsonValue::Type::Object) {
                            const auto index = frame.object.find("index");
                            const auto time = frame.object.find("time");
                            sprite.sequence.push_back({index == frame.object.end() ? 0 : number(index->second, 0),
                                time == frame.object.end() ? sprite.frameTime : std::max(1, number(time->second, 1))});
                        }
                    }
                }
            }
        } catch (...) { }
    }
    const int frameCount = height / width;
    if (sprite.sequence.empty()) {
        for (int frame = 0; frame < frameCount; ++frame) sprite.sequence.push_back({frame, sprite.frameTime});
    }
    return sprite;
}

void AnimatedAtlas::update(const GLuint texture, const int64_t tick) {
    if (tick == lastUpdateTick) return;
    lastUpdateTick = tick;

    bool changed = false;
    std::vector<unsigned char> scaled(16 * 16 * 4);
    for (Sprite& sprite : sprites) {
        int cycle = 0;
        for (const Frame& frame : sprite.sequence) cycle += frame.time;
        if (cycle <= 0) continue;
        int cursor = static_cast<int>(tick % cycle);
        int selected = 0;
        for (size_t index = 0; index < sprite.sequence.size(); ++index) {
            if (cursor < sprite.sequence[index].time) { selected = static_cast<int>(index); break; }
            cursor -= sprite.sequence[index].time;
        }
        if (selected == sprite.lastFrame && !sprite.interpolate) continue;
        sprite.lastFrame = selected;
        const Frame& current = sprite.sequence[selected];
        const Frame& next = sprite.sequence[(selected + 1) % sprite.sequence.size()];
        const float blend = sprite.interpolate ? static_cast<float>(cursor) / current.time : 0.0f;
        const int count = static_cast<int>(sprite.pixels.size()) / (sprite.frameSize * sprite.frameSize * 4);
        const int first = std::clamp(current.index, 0, count - 1);
        const int second = std::clamp(next.index, 0, count - 1);
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                const int sourceX = x * sprite.frameSize / 16;
                const int sourceY = y * sprite.frameSize / 16;
                const int destination = (y * 16 + x) * 4;
                const int a = ((first * sprite.frameSize + sourceY) * sprite.frameSize + sourceX) * 4;
                const int b = ((second * sprite.frameSize + sourceY) * sprite.frameSize + sourceX) * 4;
                for (int channel = 0; channel < 4; ++channel) {
                    scaled[destination + channel] = static_cast<unsigned char>(
                        sprite.pixels[a + channel] * (1.0f - blend) + sprite.pixels[b + channel] * blend);
                }
            }
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, sprite.tileX * 16, sprite.tileY * 16,
            16, 16, GL_RGBA, GL_UNSIGNED_BYTE, scaled.data());
        std::vector<unsigned char> mipmap = scaled;
        int size = 16;
        for (int level = 1; level <= 4; ++level) {
            mipmap = generateMipmapLevel(mipmap, size, size, true);
            size /= 2;
            glTexSubImage2D(GL_TEXTURE_2D, level, sprite.tileX * size, sprite.tileY * size,
                size, size, GL_RGBA, GL_UNSIGNED_BYTE, mipmap.data());
        }
        changed = true;
    }
    (void)changed;
}
