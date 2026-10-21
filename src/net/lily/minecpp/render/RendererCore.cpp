#include "Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <tracy/Tracy.hpp>

#include "RendererDetail.hpp"
#include "lib/stb_image.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/client/entity/EntityOtherPlayerMP.hpp"
#include "net/lily/minecpp/entity/item/EntityItem.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"
#include "net/lily/minecpp/util/FrustumPlanes.hpp"

using RendererDetail::clipThirdPersonDistance;

void Renderer::processInput() const {
    static bool f9pressed = false;
    if (glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS) {
        if (!f9pressed) {
            f9pressed = true;
            freezeFrustum = !freezeFrustum;
        }
    } else f9pressed = false;
}

bool Renderer::shouldClose() const {
    return glfwWindowShouldClose(window);;
}

void Renderer::updateProjection(const int fbWidth, const int fbHeight, const float fov) const {
    const int safeWidth = std::max(fbWidth, 1);
    const int safeHeight = std::max(fbHeight, 1);
    const float aspect = static_cast<float>(safeWidth) / static_cast<float>(safeHeight);
    const int renderDistance = mc->settings != nullptr ? mc->settings->getRenderDistance() : 8;
    const float farPlane = static_cast<float>(renderDistance * CHUNK_SIZE) * 1.41421356f;
    projection = glm::perspective(glm::radians(fov), aspect, 0.1f, std::max(farPlane, 32.0f));

    width = safeWidth;
    height = safeHeight;
    appliedProjectionFov = fov;

    updateFrustums();
}
void Renderer::updateVisibleChunks() const {
    if (freezeFrustum) return;

    for (const auto& [position, chunk] : mc->world->snapshotChunks()) {
        chunk->testCull(frustumPlanes, cameraPos);
    }
}

void Renderer::updateProjection(const int fbWidth, const int fbHeight) const {
    updateProjection(fbWidth, fbHeight, mc->settings != nullptr ? mc->settings->getFOV() : 70.0f);
}

void Renderer::updateProjection(const float fov) const {
    updateProjection(width, height, fov);
}

void Renderer::updateFrustums() const {
    projView = projection * view;
    extractFrustumPlanes(frustumPlanes, projView);
}

unsigned int Renderer::loadTexture(const char* path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << path << "\n";
        return 0;
    }

    GLint internalFormat, dataFormat;
    if (nrChannels == 1) internalFormat = dataFormat = GL_RED;
    else if (nrChannels == 3) internalFormat = dataFormat = GL_RGB;
    else if (nrChannels == 4) internalFormat = dataFormat = GL_RGBA;
    else {
        stbi_image_free(data);
        std::cerr << "Unsupported channel count: " << nrChannels << "\n";
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return textureID;
}

void Renderer::calculateView(const float yawDegrees, const float pitchDegrees) const {
    const float ry = yawDegrees + 90.0f, rx = -pitchDegrees;
    const float yaw   = glm::radians(std::fmod(ry, 360));
    const float pitch = glm::radians(std::clamp<float>(rx, -89.99f, 89.99f));

    glm::vec3 front;
    front.x = std::cos(yaw) * std::cos(pitch);
    front.y = std::sin(pitch);
    front.z = std::sin(yaw) * std::cos(pitch);
    front = glm::normalize(front);

    constexpr glm::vec3 up(0.0f, 1.0f, 0.0f);
    view = glm::lookAt(glm::vec3(0.0f), front, up);

    updateFrustums();

    blockShader->setMat4("viewProjection", glm::value_ptr(projView));
}

void Renderer::render(const World* world) const {
    ZoneScopedN("World render");
    processInput();
    frameEntities = world->snapshotEntities();
    totalEntities = frameEntities.size();
    renderedEntities = 0;
    hiddenEntities = 0;

    const float partialTicks = static_cast<float>(mc->timer->partialTicks);
    const float fovModifier = std::lerp(mc->player->prevFovModifier,
        mc->player->fovModifier, partialTicks);
    const float frameFov = mc->settings->getFOV() * fovModifier;
    if (frameFov != appliedProjectionFov) updateProjection(width, height, frameFov);
    if (const std::shared_ptr<Entity>& viewEntity = mc->getRenderViewEntity()) {
        viewEntity->getMouseOver(partialTicks);
    } else {
        mc->objectMouseOver.clear();
        mc->pointedEntity = nullptr;
    }
    const float celestialAngle = world->celestialAngle(partialTicks);
    const float skyFactor = std::clamp(std::cos(celestialAngle * static_cast<float>(M_PI) * 2.0f) * 2.0f + 0.5f,
        0.0f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glClearColor(0.02f + 0.48f * skyFactor, 0.02f + 0.68f * skyFactor,
        0.05f + 0.95f * skyFactor, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    blockShader->use();
    const double pt = partialTicks;

    const glm::vec3 eye(camera->getX(pt), camera->getY(pt), camera->getZ(pt));
    const float playerYaw = camera->getRotY(pt);
    const float playerPitch = camera->getRotX(pt);
    float viewYaw = playerYaw;
    float viewPitch = playerPitch;
    cameraPos = eye;

    switch (mc->settings->thirdPersonView) {
        case 1:
        case 2: {
            const float yaw = glm::radians(playerYaw + 90.0f);
            const float pitch = glm::radians(-playerPitch);
            const glm::vec3 look = glm::normalize(glm::vec3(
                std::cos(yaw) * std::cos(pitch),
                std::sin(pitch),
                std::sin(yaw) * std::cos(pitch)));
            const bool frontFacing = mc->settings->thirdPersonView == 2;
            const glm::vec3 cameraDirection = frontFacing ? look : -look;
            constexpr float thirdPersonDistance = 4.0f;
            const float clippedDistance = clipThirdPersonDistance(
                world, eye, cameraDirection, thirdPersonDistance);
            cameraPos += cameraDirection * clippedDistance;
            if (frontFacing) {
                viewYaw += 180.0f;
                viewPitch = -viewPitch;
            }
            break;
        }
        default:
            break;
    }

    const glm::vec2 cameraRotation{viewYaw, viewPitch};
    calculateView(viewYaw, viewPitch);
    if (mc->player->hurtTime > 0 && mc->player->maxHurtTime > 0) {
        float hurt = (static_cast<float>(mc->player->hurtTime) - partialTicks)
            / static_cast<float>(mc->player->maxHurtTime);
        hurt = std::sin(hurt * hurt * hurt * hurt * static_cast<float>(M_PI));
        const float attackedYaw = glm::radians(mc->player->attackedAtYaw);
        view = glm::rotate(view, -attackedYaw, {0.0f, 1.0f, 0.0f});
        view = glm::rotate(view, glm::radians(-hurt * 14.0f), {0.0f, 0.0f, 1.0f});
        view = glm::rotate(view, attackedYaw, {0.0f, 1.0f, 0.0f});
        updateFrustums();
        blockShader->setMat4("viewProjection", glm::value_ptr(projView));
    }

    if (mc->player->dimension == 0) renderSky(world, partialTicks);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
    animatedAtlas->update(blockAtlasTexture, world->totalTime.load(std::memory_order_relaxed));
    const int mipmapLevels = std::clamp(mc->settings->getInt("mipmapLevels"), 0, 4);
    if (mipmapLevels != appliedMipmapLevels) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, mipmapLevels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
            mipmapLevels == 0 ? GL_NEAREST : GL_NEAREST_MIPMAP_LINEAR);
        appliedMipmapLevels = mipmapLevels;
    }
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, grassColorTexture);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, foliageColorTexture);
    glActiveTexture(GL_TEXTURE0);

    blockShader->use();
    blockShader->setInt("skylightSubtracted",
        mc->player->dimension == 0 ? world->skylightSubtracted(partialTicks) : 0);
    setupFog(world, partialTicks);

    bool chunksChanged = false;
    {
        ZoneScopedN("Chunk snapshot");
        chunksChanged = world->refreshChunkSnapshot(cachedChunkRevision, cachedChunks);
    }
    const int renderDistance = mc->settings->getRenderDistance();
    const int cameraChunkX = static_cast<int>(std::floor(mc->player->position.x / CHUNK_SIZE));
    const int cameraChunkZ = static_cast<int>(std::floor(mc->player->position.z / CHUNK_SIZE));
    const auto withinRenderDistance = [&](const ChunkCoord& position) {
        return std::abs(position.x - cameraChunkX) <= renderDistance
            && std::abs(position.z - cameraChunkZ) <= renderDistance;
    };
    const glm::vec3 cullMovement = cameraPos - lastCullCameraPos;
    const glm::vec2 cullRotation = cameraRotation - lastCullCameraRotation;
    constexpr float cullPositionThreshold = 0.25f;
    constexpr float cullRotationThreshold = 0.25f;
    constexpr float cullFovThreshold = 0.25f;
    const bool cameraMoved = glm::dot(cullMovement, cullMovement)
        >= cullPositionThreshold * cullPositionThreshold;
    const bool viewChanged = std::abs(cullRotation.x) >= cullRotationThreshold
        || std::abs(cullRotation.y) >= cullRotationThreshold;
    const bool fovChanged = std::abs(frameFov - lastCullFov) >= cullFovThreshold;
    const bool renderDistanceChanged = renderDistance != lastCullRenderDistance;
    const glm::ivec2 cameraChunk(cameraChunkX, cameraChunkZ);
    const bool cameraChunkChanged = cameraChunk != lastCullCameraChunk;
    if (!freezeFrustum && (chunksChanged || cameraMoved || viewChanged || fovChanged
        || renderDistanceChanged || cameraChunkChanged)) {
        ZoneScopedN("Frustum culling");
        for (const auto& [pos, chunk] : cachedChunks) {
            if (!chunk->loaded.load(std::memory_order_acquire)) continue;
            if (!withinRenderDistance(pos)) {
                chunk->culled = true;
                continue;
            }
            chunk->testCull(frustumPlanes, cameraPos);
        }
        lastCullCameraPos = cameraPos;
        lastCullCameraRotation = cameraRotation;
        lastCullFov = frameFov;
        lastCullRenderDistance = renderDistance;
        lastCullCameraChunk = cameraChunk;
    }

    {
        ZoneScopedN("Opaque chunks");
        renderedChunks = 0;
        totalChunks = static_cast<int>(cachedChunks.size());
        for (const auto &[pos, chunk]: cachedChunks) {
            if (!withinRenderDistance(pos) || !chunk->loaded.load(std::memory_order_acquire)
                || chunk->culled) continue;

            const glm::vec3 worldPos(pos.x * CHUNK_SIZE, 0, pos.z * CHUNK_SIZE);
            const glm::vec3 relativePos = worldPos - cameraPos;

            chunk->draw(chunkOffsetLoc, relativePos);
            ++renderedChunks;
        }
    }

    renderPlayer(partialTicks);
    renderSkullBlocks(partialTicks);
    renderBlockDamage(world);
    renderSelectionBox();

    {
        ZoneScopedN("Translucent chunks");
        blockShader->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_TRUE);
        const glm::vec3 sortDelta = cameraPos - lastTranslucentSortPos;
        if (chunksChanged || translucentChunks.size() != cachedChunks.size()
            || glm::dot(sortDelta, sortDelta) >= 1.0f) {
            translucentChunks.clear();
            translucentChunks.reserve(cachedChunks.size());
            for (const auto& entry : cachedChunks) translucentChunks.push_back(&entry);
            std::ranges::sort(translucentChunks, [&](const auto* left, const auto* right) {
                return left->second->euclDistSqr(cameraPos) > right->second->euclDistSqr(cameraPos);
            });
            lastTranslucentSortPos = cameraPos;
        }
        for (const auto* entry : translucentChunks) {
            const auto& [pos, chunk] = *entry;
            if (!withinRenderDistance(pos) || !chunk->loaded.load(std::memory_order_acquire)
                || chunk->culled) continue;
            const glm::vec3 worldPos(pos.x * CHUNK_SIZE, 0, pos.z * CHUNK_SIZE);
            chunk->drawTranslucent(chunkOffsetLoc, worldPos - cameraPos);
        }
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        glBindVertexArray(0);
    }

    renderParticles(partialTicks);
    renderHand(partialTicks);
}
