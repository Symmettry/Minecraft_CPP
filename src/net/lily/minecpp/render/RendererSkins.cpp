#include "Renderer.hpp"

#include <chrono>
#include <cstring>
#include <mutex>

#include <curl/curl.h>

#include "lib/stb_image.hpp"
#include "net/lily/minecpp/client/entity/EntityOtherPlayerMP.hpp"
#include "net/lily/minecpp/entity/player/EntityPlayer.hpp"

namespace {
size_t appendDownload(const char* data, const size_t size, const size_t count, void* output) {
    auto& bytes = *static_cast<std::vector<unsigned char>*>(output);
    const size_t length = size * count;
    bytes.insert(bytes.end(), data, data + length);
    return length;
}

void setOpaque(std::vector<unsigned char>& pixels, const int x0, const int y0, const int x1, const int y1) {
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) pixels[(y * 64 + x) * 4 + 3] = 255;
}

void clearOpaqueOverlay(std::vector<unsigned char>& pixels, const int x0, const int y0, const int x1, const int y1) {
    bool transparent = false;
    for (int y = y0; y < y1 && !transparent; ++y)
        for (int x = x0; x < x1; ++x)
            if (pixels[(y * 64 + x) * 4 + 3] < 128) { transparent = true; break; }
    if (transparent) return;
    for (int y = y0; y < y1; ++y)
        for (int x = x0; x < x1; ++x) pixels[(y * 64 + x) * 4 + 3] = 0;
}
}

std::optional<Renderer::SkinImage> Renderer::downloadSkin(const std::string& url) {
    if (!url.starts_with("https://") && !url.starts_with("http://")) return std::nullopt;
    static std::once_flag curlInitialization;
    std::call_once(curlInitialization, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    CURL* const curl = curl_easy_init();
    if (curl == nullptr) return std::nullopt;
    std::vector<unsigned char> encoded;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MineCPP/1.8.9");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, appendDownload);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &encoded);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (result != CURLE_OK || status < 200 || status >= 300 || encoded.empty()) return std::nullopt;

    int width = 0, height = 0, channels = 0;
    unsigned char* const decoded = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()),
        &width, &height, &channels, 4);
    if (decoded == nullptr || width != 64 || (height != 32 && height != 64)) {
        stbi_image_free(decoded);
        return std::nullopt;
    }
    SkinImage image;
    image.width = 64;
    image.height = 64;
    image.pixels.assign(64 * 64 * 4, 0);
    std::memcpy(image.pixels.data(), decoded, static_cast<size_t>(64 * height * 4));
    stbi_image_free(decoded);
    setOpaque(image.pixels, 0, 0, 32, 16);
    setOpaque(image.pixels, 0, 16, 64, 32);
    setOpaque(image.pixels, 16, 48, 48, 64);
    clearOpaqueOverlay(image.pixels, 32, 0, 64, 32);
    clearOpaqueOverlay(image.pixels, 0, 32, 16, 48);
    clearOpaqueOverlay(image.pixels, 16, 32, 40, 48);
    clearOpaqueOverlay(image.pixels, 40, 32, 56, 48);
    clearOpaqueOverlay(image.pixels, 0, 48, 16, 64);
    clearOpaqueOverlay(image.pixels, 48, 48, 64, 64);
    return image;
}

std::pair<GLuint, bool> Renderer::playerSkin(const EntityPlayer* player) const {
    bool slim = player->slimSkin;
    if (const auto* remote = dynamic_cast<const EntityOtherPlayerMP*>(player); player->skinUrl.empty()) {
        slim = remote != nullptr && remote->usesSlimSkin();
    }
    const GLuint fallback = slim ? alexSkinTexture : playerSkinTexture;
    if (player->skinUrl.empty()) return {fallback, slim};

    auto [found, inserted] = skinTextures.try_emplace(player->skinUrl);
    SkinTexture& cached = found->second;
    if (inserted) {
        const std::string url = player->skinUrl;
        cached.download = std::async(std::launch::async, [url] { return downloadSkin(url); });
    }
    if (cached.texture != 0) return {cached.texture, slim};
    if (cached.failed || !cached.download.valid()
        || cached.download.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return {fallback, slim};
    const std::optional<SkinImage> image = cached.download.get();
    if (!image.has_value()) {
        cached.failed = true;
        return {fallback, slim};
    }
    glGenTextures(1, &cached.texture);
    glBindTexture(GL_TEXTURE_2D, cached.texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image->width, image->height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, image->pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    return {cached.texture, slim};
}
