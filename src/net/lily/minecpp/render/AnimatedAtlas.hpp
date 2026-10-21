#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glad/glad.hpp>

#include "BlockAtlas.hpp"

class AnimatedAtlas final {
public:
    explicit AnimatedAtlas(const BlockAtlas& atlas);
    void update(GLuint texture, int64_t tick);

private:
    struct Frame {
        int index = 0;
        int time = 1;
    };

    struct Sprite {
        int tileX = 0;
        int tileY = 0;
        int frameTime = 1;
        int frameSize = 16;
        bool interpolate = false;
        int lastFrame = -1;
        std::vector<unsigned char> pixels;
        std::vector<Frame> sequence;
    };

    std::vector<Sprite> sprites;
    int64_t lastUpdateTick = -1;
    static Sprite load(const BlockAtlas& atlas, const std::string& name);
};
