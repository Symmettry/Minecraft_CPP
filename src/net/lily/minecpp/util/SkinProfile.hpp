#pragma once

#include <string>

struct SkinProfile {
    std::string url;
    bool slim = false;

    static SkinProfile fromTextureProperty(const std::string& encoded);
};
