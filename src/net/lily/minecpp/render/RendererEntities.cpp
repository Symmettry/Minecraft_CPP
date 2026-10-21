#include "Renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <random>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "RendererDetail.hpp"
#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/client/entity/EntityOtherPlayerMP.hpp"
#include "net/lily/minecpp/entity/item/EntityItem.hpp"
#include "net/lily/minecpp/entity/EntityMob.hpp"
#include "net/lily/minecpp/util/ItemRegistry.hpp"

using RendererDetail::interpolateRotation;

namespace {
GLuint skullTexture(const Renderer& renderer, const int damage) {
    switch (std::clamp(damage, 0, 4)) {
        case 1: return renderer.witherSkullTexture;
        case 2: return renderer.zombieSkullTexture;
        case 3: return renderer.playerSkinTexture;
        case 4: return renderer.creeperSkullTexture;
        default: return renderer.skeletonSkullTexture;
    }
}

GLuint skullVao(const Renderer& renderer, const int damage) {
    switch (std::clamp(damage, 0, 4)) {
        case 2: case 3: return renderer.playerVAO;
        default: return renderer.skullVAO;
    }
}

glm::vec3 blockItemTint(const Block block) {
    switch (blockId(block)) {
        case blockId(BLOCK_GRASS):
            return RendererDetail::sampleClimateColor(0.5f, 1.0f, false);
        case blockId(BLOCK_LEAVES):
            switch (blockMeta(block) & 3) {
                case 1: return glm::vec3(0x61, 0x99, 0x61) / 255.0f;
                case 2: return glm::vec3(0x80, 0xA7, 0x55) / 255.0f;
                default: return glm::vec3(0x48, 0xB5, 0x18) / 255.0f;
            }
        case blockId(BLOCK_LEAVES2):
            return glm::vec3(0x48, 0xB5, 0x18) / 255.0f;
        default:
            return glm::vec3(1.0f);
    }
}
}

void Renderer::updateEquippedItem() {
    previousEquippedProgress = equippedProgress;
    const NetworkItemStack& selected = mc->player->inventory[36 + mc->player->currentHotbarSlot];
    const bool changed = static_cast<bool>(itemToRender) != static_cast<bool>(selected)
        || (itemToRender && (itemToRender.itemId != selected.itemId
            || itemToRender.damage != selected.damage || itemToRender.count != selected.count));
    const float target = changed ? 0.0f : 1.0f;
    equippedProgress += std::clamp(target - equippedProgress, -0.4f, 0.4f);
    if (equippedProgress < 0.1f) {
        itemToRender = selected;
        equippedItemSlot = mc->player->currentHotbarSlot;
    }
}

void Renderer::renderHand(const float partialTicks) const {
    if (mc->player->isSpectator() || mc->settings->thirdPersonView != 0) return;

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    // entityRenderer::renderHand uses getFOVModifier(partialTicks, false): the first-person item projection remains at 70 degrees independently of the configurable world FOV
    const glm::mat4 handProjection = glm::perspective(glm::radians(70.0f), aspect, 0.05f, 10.0f);
    glm::mat4 model(1.0f);
    if (mc->settings->getBool("viewBobbing")) {
        const float walkedDelta = mc->player->distanceWalkedModified - mc->player->prevDistanceWalkedModified;
        const float walked = -(mc->player->distanceWalkedModified + walkedDelta * partialTicks);
        const float yaw = std::lerp(mc->player->prevCameraYaw, mc->player->cameraYaw, partialTicks);
        const float pitch = std::lerp(mc->player->prevCameraPitch, mc->player->cameraPitch, partialTicks);
        model = glm::translate(model, {std::sin(walked * static_cast<float>(M_PI)) * yaw * 0.5f,
            -std::abs(std::cos(walked * static_cast<float>(M_PI)) * yaw), 0.0f});
        model = glm::rotate(model, glm::radians(std::sin(walked * static_cast<float>(M_PI)) * yaw * 3.0f),
            {0.0f, 0.0f, 1.0f});
        model = glm::rotate(model, glm::radians(std::abs(std::cos(walked * static_cast<float>(M_PI) - 0.2f) * yaw) * 5.0f),
            {1.0f, 0.0f, 0.0f});
        model = glm::rotate(model, glm::radians(pitch), {1.0f, 0.0f, 0.0f});
    }
    const float armPitch = std::lerp(mc->player->prevRenderArmPitch,
        mc->player->renderArmPitch, partialTicks);
    const float armYaw = std::lerp(mc->player->prevRenderArmYaw,
        mc->player->renderArmYaw, partialTicks);
    const float viewPitch = std::lerp(mc->player->lastRot.pitch, mc->player->rotation.pitch, partialTicks);
    const float viewYaw = std::lerp(mc->player->lastRot.yaw,
        mc->player->rotation.yaw, partialTicks);
    model = glm::rotate(model, glm::radians((viewPitch - armPitch) * 0.1f),
        {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians((viewYaw - armYaw) * 0.1f),
        {0.0f, 1.0f, 0.0f});
    float swingProgress = mc->player->swingProgress - mc->player->prevSwingProgress;
    if (swingProgress < 0.0f) swingProgress += 1.0f;
    swingProgress = mc->player->prevSwingProgress + swingProgress * partialTicks;
    const float equipOffset = 1.0f - std::lerp(previousEquippedProgress,
        equippedProgress, partialTicks);
    if (const NetworkItemStack& heldStack = itemToRender) {
        const bool usingItem = mc->player->usingItem
            && heldStack.itemId == mc->player->itemInUseId
            && heldStack.damage == mc->player->itemInUseDamage;
        const ItemRegistry::UseAction useAction = usingItem
            ? ItemRegistry::useAction(heldStack.itemId) : ItemRegistry::UseAction::NONE;
        if (usingItem) swingProgress = 0.0f;
        const float swingRoot = std::sqrt(swingProgress);
        if (useAction == ItemRegistry::UseAction::EAT
            || useAction == ItemRegistry::UseAction::DRINK) {
            const float remaining = std::max(0.0f,
                32.0f - static_cast<float>(mc->player->itemUseTicks) - partialTicks + 1.0f);
            const float ratio = remaining / 32.0f;
            const float bob = ratio >= 0.8f ? 0.0f
                : std::abs(std::cos(remaining * 0.25f * static_cast<float>(M_PI)) * 0.1f);
            const float progress = 1.0f - std::pow(ratio, 27.0f);
            model = glm::translate(model, {0.0f, bob, 0.0f});
            model = glm::translate(model, {progress * 0.6f, progress * -0.5f, 0.0f});
            model = glm::rotate(model, glm::radians(progress * 90.0f), {0.0f, 1.0f, 0.0f});
            model = glm::rotate(model, glm::radians(progress * 10.0f), {1.0f, 0.0f, 0.0f});
            model = glm::rotate(model, glm::radians(progress * 30.0f), {0.0f, 0.0f, 1.0f});
        }
        // itemRenderer.doItemUsedTransformations and transformFirstPersonItem
        model = glm::translate(model, {
            -0.4f * std::sin(swingRoot * static_cast<float>(M_PI)),
            0.2f * std::sin(swingRoot * static_cast<float>(M_PI) * 2.0f),
            -0.2f * std::sin(swingProgress * static_cast<float>(M_PI)),
        });
        model = glm::translate(model, {0.56f, -0.52f, -0.71999997f});
        model = glm::translate(model, {0.0f, equipOffset * -0.6f, 0.0f});
        model = glm::rotate(model, glm::radians(45.0f), {0.0f, 1.0f, 0.0f});
        model = glm::rotate(model, glm::radians(
            std::sin(swingProgress * swingProgress * static_cast<float>(M_PI)) * -20.0f),
            {0.0f, 1.0f, 0.0f});
        model = glm::rotate(model, glm::radians(
            std::sin(swingRoot * static_cast<float>(M_PI)) * -20.0f),
            {0.0f, 0.0f, 1.0f});
        model = glm::rotate(model, glm::radians(
            std::sin(swingRoot * static_cast<float>(M_PI)) * -80.0f),
            {1.0f, 0.0f, 0.0f});
        model = glm::scale(model, glm::vec3(0.4f));
        if (useAction == ItemRegistry::UseAction::BLOCK) {
            model = glm::translate(model, {-0.5f, 0.2f, 0.0f});
            model = glm::rotate(model, glm::radians(30.0f), {0.0f, 1.0f, 0.0f});
            model = glm::rotate(model, glm::radians(-80.0f), {1.0f, 0.0f, 0.0f});
            model = glm::rotate(model, glm::radians(60.0f), {0.0f, 1.0f, 0.0f});
        } else if (useAction == ItemRegistry::UseAction::BOW) {
            const float draw = std::min((static_cast<float>(mc->player->itemUseTicks) + partialTicks)
                / 20.0f, 1.0f);
            model = glm::rotate(model, glm::radians(-18.0f), {0.0f, 0.0f, 1.0f});
            model = glm::rotate(model, glm::radians(-12.0f), {0.0f, 1.0f, 0.0f});
            model = glm::rotate(model, glm::radians(-8.0f), {1.0f, 0.0f, 0.0f});
            model = glm::translate(model, {-0.9f, 0.2f, draw * 0.1f});
            model = glm::scale(model, {1.0f, 1.0f, 1.0f + draw * 0.2f});
        }

        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glActiveTexture(GL_TEXTURE0);

        if (heldStack.itemId == 397) {
            glm::mat4 skullModel = glm::rotate(model, glm::radians(180.0f), {0.0f, 1.0f, 0.0f});
            skullModel = glm::scale(skullModel, glm::vec3(1.7f));
            if (heldStack.damage == 2 || heldStack.damage == 3)
                skullModel = glm::translate(skullModel, {0.0f, -0.25f, 0.0f});
            handShader->use();
            handShader->setMat4("projection", glm::value_ptr(handProjection));
            handShader->setMat4("model", glm::value_ptr(skullModel));
            handShader->setVec3("colorMultiplier", {1.0f, 1.0f, 1.0f});
            handShader->setVec4("colorOverlay", {0.0f, 0.0f, 0.0f, 0.0f});
            glBindTexture(GL_TEXTURE_2D, skullTexture(*this, heldStack.damage));
            glBindVertexArray(skullVao(*this, heldStack.damage));
            glDisable(GL_CULL_FACE);
            glDrawArrays(GL_TRIANGLES, 0, 36);
            glBindVertexArray(0);
            glEnable(GL_CULL_FACE);
            glDisable(GL_BLEND);
            return;
        }

        const Block heldBlock = BlockUtil::itemBlockState(heldStack.itemId, heldStack.damage);
        const bool blockModel = heldStack.itemId < 256
            && heldStack.itemId != 101 && heldStack.itemId != 102 && heldStack.itemId != 160
            && BlockUtil::getCrossPlantItemTexture(heldBlock) == nullptr;
        if (blockModel) {
            static constexpr int faceIndexToBlockFace[6]{1, 0, 3, 2, 4, 5};
            float tileOrigins[12];
            for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
                const char* texture = BlockUtil::getBlockTexture(
                    heldBlock, faceIndexToBlockFace[faceIndex]);
                const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(texture);
                tileOrigins[faceIndex * 2] = static_cast<float>(tileX) / atlasTilesPerRow;
                tileOrigins[faceIndex * 2 + 1] = static_cast<float>(tileY) / atlasTilesPerRow;
            }
            itemShader->use();
            itemShader->setMat4("projection", glm::value_ptr(handProjection));
            itemShader->setMat4("view", glm::value_ptr(glm::mat4(1.0f)));
            itemShader->setMat4("model", glm::value_ptr(model));
            itemShader->setVec2Array("tileOrigins", tileOrigins, 6);
            itemShader->setVec3("colorMultiplier", {1.0f, 1.0f, 1.0f});
            glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
            glBindVertexArray(damageVAO);
            glDisable(GL_CULL_FACE);
            {
                const auto [first, count] = blockItemRange(heldBlock);
                glDrawArrays(GL_TRIANGLES, first, count);
            }
        } else {
            const ItemExtrusionMesh* mesh = nullptr;
            GLuint texture = itemAtlasTexture;
            if (heldStack.itemId < 256) {
                const char* itemTexture = BlockUtil::getCrossPlantItemTexture(heldBlock);
                std::string textureName;
                if (heldStack.itemId == 101) textureName = "iron_bars";
                else if (heldStack.itemId == 102) textureName = "glass";
                else if (heldStack.itemId == 160) textureName = "glass_" + std::string(
                    BlockUtil::dyeColorNames[heldStack.damage & 15]);
                else textureName = itemTexture == nullptr ? BlockUtil::getBlockTexture(heldBlock, 4)
                                                          : itemTexture;
                const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(
                    BlockUtil::getAtlasTexturePtr(blockAtlas.second, textureName));
                mesh = &getOrBuildBlockAtlasExtrusionMesh(tileX, tileY);
                texture = blockAtlasTexture;
            } else {
                mesh = &getOrBuildItemExtrusionMesh(itemAtlas.tile(
                    ItemRegistry::identifier(heldStack.itemId, heldStack.damage)));
            }
            // renderItem.preTransform scales non-3D models by 2.0 before renderItem applies its universal 0.5 scale, so their net model scale is the FIRST_PERSON transform's 1.7
            glm::mat4 itemModel = glm::translate(model, {0.0f, 4.0f / 16.0f, 2.0f / 16.0f});
            itemModel = glm::rotate(itemModel, glm::radians(-135.0f), {0.0f, 1.0f, 0.0f});
            itemModel = glm::rotate(itemModel, glm::radians(25.0f), {0.0f, 0.0f, 1.0f});
            itemModel = glm::scale(itemModel, glm::vec3(1.7f));
            itemSpriteShader->use();
            itemSpriteShader->setMat4("projection", glm::value_ptr(handProjection));
            itemSpriteShader->setMat4("view", glm::value_ptr(glm::mat4(1.0f)));
            itemSpriteShader->setMat4("model", glm::value_ptr(itemModel));
            itemSpriteShader->setVec3("colorMultiplier", {1.0f, 1.0f, 1.0f});
            itemSpriteShader->setFloat("opacity", 1.0f);
            itemSpriteShader->setVec2("uvMinimum", 0.0f, 0.0f);
            itemSpriteShader->setVec2("uvMaximum", 1.0f, 1.0f);
            glBindTexture(GL_TEXTURE_2D, texture);
            glBindVertexArray(mesh->vao);
            glDisable(GL_CULL_FACE);
            glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
        }
        glBindVertexArray(0);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        return;
    }
    const float swingRoot = std::sqrt(swingProgress);
    model = glm::translate(model, {
        -0.3f * std::sin(swingRoot * static_cast<float>(M_PI)),
        0.4f * std::sin(swingRoot * static_cast<float>(M_PI) * 2.0f),
        -0.4f * std::sin(swingProgress * static_cast<float>(M_PI)),
    });
    model = glm::translate(model, {0.64000005f, -0.6f, -0.71999997f});
    model = glm::translate(model, {0.0f, equipOffset * -0.6f, 0.0f});
    model = glm::rotate(model, glm::radians(45.0f), {0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, glm::radians(std::sin(swingRoot * static_cast<float>(M_PI)) * 70.0f),
        {0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, glm::radians(std::sin(swingProgress * swingProgress
        * static_cast<float>(M_PI)) * -20.0f), {0.0f, 0.0f, 1.0f});
    model = glm::translate(model, {-1.0f, 3.6f, 3.5f});
    model = glm::rotate(model, glm::radians(120.0f), {0.0f, 0.0f, 1.0f});
    model = glm::rotate(model, glm::radians(200.0f), {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians(-135.0f), {0.0f, 1.0f, 0.0f});
    model = glm::translate(model, {5.6f - 5.0f / 16.0f, 2.0f / 16.0f, 0.0f});

    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    handShader->use();
    handShader->setVec3("colorMultiplier", {1.0f, 1.0f, 1.0f});
    handShader->setVec4("colorOverlay", {0.0f, 0.0f, 0.0f, 0.0f});
    handShader->setMat4("projection", glm::value_ptr(handProjection));
    handShader->setMat4("model", glm::value_ptr(model));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, playerSkin(mc->player.get()).first);
    glBindVertexArray(handVAO);
    glDrawArrays(GL_TRIANGLES, 0, 72);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

void Renderer::renderSkullBlocks(const float partialTicks) const {
    handShader->use();
    handShader->setMat4("projection", glm::value_ptr(projView));
    handShader->setVec3("colorMultiplier", {1.0f, 1.0f, 1.0f});
    handShader->setVec4("colorOverlay", {0.0f, 0.0f, 0.0f, 0.0f});
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_CULL_FACE);
    std::shared_lock blockEntityLock(mc->world->blockEntityMutex);
    for (const auto& [coordinate, chunk] : cachedChunks) {
        if (chunk == nullptr || chunk->culled || !chunk->loaded.load(std::memory_order_acquire)) continue;
        // uploadedSkullBlocks is only written by uploadMesh on this thread, so no lock is needed
        for (const glm::ivec3& local : chunk->uploadedSkullBlocks) {
            const Block block = chunk->getBlock(local.x, local.y, local.z);
            if (blockId(block) != blockId(BLOCK_SKULL)) continue; // the list also holds chests and tables
            const int metadata = blockMeta(block) & 7;
            const glm::ivec3 worldPosition{coordinate.x * CHUNK_SIZE + local.x,
                local.y, coordinate.z * CHUNK_SIZE + local.z};
            int skullType = 0;
            uint8_t skullRotation = 0;
            if (const auto found = mc->world->blockEntities.find(World::positionKey(worldPosition));
                found != mc->world->blockEntities.end()) {
                if (const auto* skull = std::get_if<SkullBlockEntity>(&found->second.payload)) {
                    skullType = skull->type;
                    skullRotation = skull->rotation;
                }
            }
            const bool wallMounted = metadata >= 2 && metadata <= 5;
            glm::vec3 position{coordinate.x * CHUNK_SIZE + local.x + 0.5f,
                local.y + (wallMounted ? 0.5f : 0.25f),
                coordinate.z * CHUNK_SIZE + local.z + 0.5f};
            float yaw = static_cast<float>(skullRotation) * -22.5f;
            switch (metadata) {
                case 2: position.z += 0.24f; break;
                case 3: position.z -= 0.24f; yaw = -180.0f; break;
                case 4: position.x += 0.24f; yaw = -270.0f; break;
                case 5: position.x -= 0.24f; yaw = -90.0f; break;
                default:
                    break;
            }
            glm::mat4 model = glm::translate(glm::mat4(1.0f), position - cameraPos);
            model = glm::rotate(model, glm::radians(yaw), {0.0f, 1.0f, 0.0f});
            if (skullType == 2 || skullType == 3)
                model = glm::translate(model, {0.0f, -0.25f, 0.0f});
            handShader->setMat4("model", glm::value_ptr(model));
            glBindTexture(GL_TEXTURE_2D, skullTexture(*this, skullType));
            glBindVertexArray(skullVao(*this, skullType));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    renderBlockEntityModels(partialTicks);
}

void Renderer::setChestViewers(const glm::ivec3& position, const int viewers) const {
    std::lock_guard lock(chestStateMutex);
    chestStates[World::positionKey(position)].viewers = viewers;
}

// the lid opens at 0.1 per tick while anybody has the chest open (TileEntityChest.update)
void Renderer::tickBlockEntities() const {
    std::lock_guard lock(chestStateMutex);
    for (auto it = chestStates.begin(); it != chestStates.end();) {
        ChestState& state = it->second;
        state.previousAngle = state.angle;
        state.angle = std::clamp(state.angle + (state.viewers > 0 ? 0.1f : -0.1f), 0.0f, 1.0f);
        if (state.viewers == 0 && state.angle == 0.0f && state.previousAngle == 0.0f) it = chestStates.erase(it);
        else ++it;
    }
}

void Renderer::renderBlockEntityModels(const float partialTicks) const {
    struct Instance {
        uint16_t model;
        glm::mat4 chain;     // vanilla's per-block transform, applied to vanilla model coordinates
        float brightness;
        float lid = 0.0f;    // chest lid angle
        float spread = 0.0f, flip = 0.0f, ticks = 0.0f; // enchanting table book
    };
    std::vector<Instance> instances;
    const double now = glfwGetTime();
    const World* world = mc->world;
    constexpr float maxDistanceSquared = 64.0f * 64.0f;

    for (const auto& [coordinate, chunk] : cachedChunks) {
        if (chunk == nullptr || chunk->culled || !chunk->loaded.load(std::memory_order_acquire)) continue;
        for (const glm::ivec3& local : chunk->uploadedSkullBlocks) {
            const Block block = chunk->getBlock(local.x, local.y, local.z);
            const uint16_t id = blockId(block);
            if (id != blockId(BLOCK_CHEST) && id != blockId(BLOCK_TRAPPED_CHEST) && id != blockId(BLOCK_ENDER_CHEST)
                && id != blockId(BLOCK_ENCHANTING_TABLE)) continue;
            const glm::ivec3 position{coordinate.x * CHUNK_SIZE + local.x, local.y, coordinate.z * CHUNK_SIZE + local.z};
            const glm::vec3 relative = glm::vec3(position) - cameraPos;
            if (glm::dot(relative, relative) > maxDistanceSquared) continue;
            // frustum test of the block's center
            const glm::vec4 clip = projView * glm::vec4(relative + glm::vec3(0.5f), 1.0f);
            if (clip.w <= -1.5f || std::abs(clip.x) > clip.w + 2.0f || std::abs(clip.y) > clip.w + 2.0f) continue;

            float brightness = 1.0f;
            {
                const uint32_t light = chunk->getLightData(local.x, std::clamp(local.y, 0, WORLD_HEIGHT - 1), local.z);
                const float level = static_cast<float>(std::max(light & 15U,
                    std::max(((light >> 4U) & 15U) - static_cast<uint32_t>(world->skylightSubtracted(partialTicks)), 0U))) / 15.0f;
                brightness = level / ((1.0f - level) * 3.0f + 1.0f);
            }

            if (id == blockId(BLOCK_ENCHANTING_TABLE)) {
                // TileEntityEnchantmentTableRenderer: a book hovering over the table that opens and turns toward
                // the player standing next to it
                BookState& state = bookStates[World::positionKey(position)];
                const double elapsed = state.lastTime < 0.0 ? 0.0 : std::min(now - state.lastTime, 0.25);
                state.lastTime = now;
                const float ticksPassed = static_cast<float>(elapsed * 20.0);
                const glm::vec3 center = glm::vec3(position) + glm::vec3(0.5f);
                const glm::vec3 toPlayer = glm::vec3(mc->player->position) - center;
                const bool near = glm::dot(toPlayer, toPlayer) < 9.0f;
                state.spread = std::clamp(state.spread + (near ? 0.1f : -0.1f) * ticksPassed, 0.0f, 1.0f);
                const float target = near ? std::atan2(toPlayer.z, toPlayer.x) : state.rotation + 0.02f * ticksPassed;
                float delta = target - state.rotation;
                while (delta >= 3.14159265f) delta -= 6.2831853f;
                while (delta < -3.14159265f) delta += 6.2831853f;
                state.rotation += delta * std::min(1.0f, 0.4f * ticksPassed);
                state.flip += 0.02f * ticksPassed * (state.spread > 0.0f ? 1.0f : 0.0f);
                const float ticks = static_cast<float>(now * 20.0);
                glm::mat4 chain = glm::translate(glm::mat4(1.0f), relative + glm::vec3(0.5f, 0.75f, 0.5f));
                chain = glm::translate(chain, {0.0f, 0.1f + std::sin(ticks * 0.1f) * 0.01f, 0.0f});
                chain = glm::rotate(chain, -state.rotation, {0.0f, 1.0f, 0.0f});
                chain = glm::rotate(chain, glm::radians(80.0f), {0.0f, 0.0f, 1.0f});
                instances.push_back({305, chain, brightness, 0.0f, state.spread, state.flip, ticks});
                continue;
            }

            // chests: TileEntityChestRenderer. Two chests side by side render once, as a large chest, from the
            // one with a neighbor on its positive side.
            const auto same = [&](const int dx, const int dz) {
                return blockId(world->getBlockAt(position.x + dx, position.y, position.z + dz)) == id;
            };
            if (same(-1, 0) || same(0, -1)) continue;
            const bool xPositive = same(1, 0), zPositive = same(0, 1);
            const bool large = xPositive || zPositive;
            const int facing = blockMeta(block);
            float angle = 0.0f;
            if (facing == 2) angle = 180.0f;
            else if (facing == 4) angle = 90.0f;
            else if (facing == 5) angle = -90.0f;
            glm::mat4 chain = glm::translate(glm::mat4(1.0f), relative + glm::vec3(0.0f, 1.0f, 1.0f));
            chain = glm::scale(chain, {1.0f, -1.0f, -1.0f});
            chain = glm::translate(chain, {0.5f, 0.5f, 0.5f});
            if (facing == 2 && xPositive) chain = glm::translate(chain, {1.0f, 0.0f, 0.0f});
            if (facing == 5 && zPositive) chain = glm::translate(chain, {0.0f, 0.0f, -1.0f});
            chain = glm::rotate(chain, glm::radians(angle), {0.0f, 1.0f, 0.0f});
            chain = glm::translate(chain, {-0.5f, -0.5f, -0.5f});
            float lid = 0.0f;
            {
                std::lock_guard lock(chestStateMutex);
                if (const auto found = chestStates.find(World::positionKey(position)); found != chestStates.end())
                    lid = std::lerp(found->second.previousAngle, found->second.angle, partialTicks);
            }
            lid = 1.0f - lid;
            lid = 1.0f - lid * lid * lid;
            const uint16_t model = id == blockId(BLOCK_ENDER_CHEST) ? 304
                : id == blockId(BLOCK_TRAPPED_CHEST) ? (large ? 303 : 302) : (large ? 301 : 300);
            instances.push_back({model, chain, brightness, lid * 1.5707964f});
        }
    }
    if (instances.empty()) return;
    // one bind per model: group the instances before drawing
    std::stable_sort(instances.begin(), instances.end(),
        [](const Instance& a, const Instance& b) { return a.model < b.model; });

    handShader->use();
    handShader->setMat4("projection", glm::value_ptr(projView));
    handShader->setVec4("colorOverlay", {0.0f, 0.0f, 0.0f, 0.0f});
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    uint16_t bound = 0xFFFF;
    std::vector<glm::mat4> partMatrices;
    for (const Instance& instance : instances) {
        const auto found = mobMeshes.find(instance.model);
        if (found == mobMeshes.end() || found->second.texture == 0) continue;
        const MobMesh& mesh = found->second;
        if (bound != instance.model) {
            glBindTexture(GL_TEXTURE_2D, mesh.texture);
            glBindVertexArray(mesh.vao);
            bound = instance.model;
        }
        handShader->setVec3("colorMultiplier", {instance.brightness, instance.brightness, instance.brightness});
        // the models are stored y-flipped relative to vanilla's model space; undo that first
        const glm::mat4 base = glm::scale(glm::translate(instance.chain, {0.0f, 1.5f, 0.0f}), {1.0f, -1.0f, 1.0f});
        partMatrices.assign(mesh.parts.size(), base);
        for (size_t index = 0; index < mesh.parts.size(); ++index) {
            const MobPart& part = mesh.parts[index];
            glm::vec3 rotation = part.rotation;
            glm::vec3 pivot = part.pivot;
            if (part.animation == 40) {
                rotation.x = instance.lid; // vanilla -lid in its frame; this mesh's frame flips the sign
            } else if (part.animation >= 41 && part.animation <= 46) {
                // ModelBook.setRotationAngles
                const float f = (std::sin(instance.ticks * 0.02f) * 0.1f + 1.25f) * instance.spread;
                const float flipRight = std::clamp((instance.flip + 0.25f - std::floor(instance.flip + 0.25f)) * 1.6f - 0.3f, 0.0f, 1.0f);
                const float flipLeft = std::clamp((instance.flip + 0.75f - std::floor(instance.flip + 0.75f)) * 1.6f - 0.3f, 0.0f, 1.0f);
                switch (part.animation) {
                    case 41: rotation.y = 3.14159265f + f; break;
                    case 42: rotation.y = -f; break;
                    case 43: rotation.y = f; pivot.x = std::sin(f) / 16.0f; break;
                    case 44: rotation.y = -f; pivot.x = std::sin(f) / 16.0f; break;
                    case 45: rotation.y = f - f * 2.0f * flipRight; pivot.x = std::sin(f) / 16.0f; break;
                    default: rotation.y = f - f * 2.0f * flipLeft; pivot.x = std::sin(f) / 16.0f; break;
                }
            }
            glm::mat4 model = glm::translate(base, pivot);
            model = glm::rotate(model, rotation.z, {0.0f, 0.0f, 1.0f});
            model = glm::rotate(model, rotation.y, {0.0f, 1.0f, 0.0f});
            model = glm::rotate(model, rotation.x, {1.0f, 0.0f, 0.0f});
            handShader->setMat4("model", glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, part.firstVertex, 36);
        }
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
}

void Renderer::renderPlayer(const float partialTicks) const {
    const std::vector<std::shared_ptr<Entity>>& entities = frameEntities;
    entityShadowVertices.clear();
    if (mc->settings->thirdPersonView != 0 && !mc->player->isSpectator()) {
        queueEntityShadow(mc->player.get(), partialTicks, 0.5f);
    }
    for (const std::shared_ptr<Entity>& entity : entities) {
        if (entity.get() == mc->player.get()) continue;
        switch (entity->getType()) {
            case PLAYER: queueEntityShadow(entity.get(), partialTicks, 0.5f); break;
            case ITEM: queueEntityShadow(entity.get(), partialTicks, 0.15f); break;
            case MOB: {
                const auto mob = static_cast<const EntityMob*>(entity.get());
                if (!mob->isInvisible()) queueEntityShadow(entity.get(), partialTicks, mob->shadowSize());
                break;
            }
            default: break;
        }
    }
    renderEntityShadows();

    if (mc->settings->thirdPersonView != 0 && !mc->player->isSpectator()) {
        renderPlayerEntity(mc->player.get(), partialTicks);
    }
    for (const std::shared_ptr<Entity>& entity : entities) {
        if (entity.get() == mc->player.get()) continue;
        switch (entity->getType()) {
            case PLAYER:
                renderPlayerEntity(static_cast<const EntityPlayer*>(entity.get()), partialTicks);
                renderEntityName(entity.get(),
                    static_cast<const EntityOtherPlayerMP*>(entity.get())->username, partialTicks);
                ++renderedEntities;
                break;
            case ITEM:
                renderItemEntity(static_cast<const EntityItem*>(entity.get()), partialTicks);
                ++renderedEntities;
                break;
            case MOB: {
                const auto mob = static_cast<const EntityMob*>(entity.get());
                if (!mob->isInvisible()) renderMobEntity(mob, partialTicks);
                if (mob->hasVisibleCustomName())
                    renderEntityName(mob, mob->customName(), partialTicks);
                ++renderedEntities;
                break;
            }
            default:
                ++hiddenEntities;
                break;
        }
    }
}

// riders are drawn from their vehicle's interpolated position, so they move smoothly with it instead of lagging a tick behind or waiting for their own position update
glm::vec3 Renderer::interpolatedPosition(const Entity* entity, const float partialTicks) const {
    if (entity->vehicleId >= 0) {
        if (const std::shared_ptr<Entity> vehicle = mc->world->getEntityById(entity->vehicleId)) {
            return {std::lerp(vehicle->lastPos.x, vehicle->position.x, partialTicks),
                    std::lerp(vehicle->lastPos.y, vehicle->position.y, partialTicks)
                        + entity->ridingYOffset(*vehicle),
                    std::lerp(vehicle->lastPos.z, vehicle->position.z, partialTicks)};
        }
    }
    return {std::lerp(entity->lastPos.x, entity->position.x, partialTicks),
            std::lerp(entity->lastPos.y, entity->position.y, partialTicks),
            std::lerp(entity->lastPos.z, entity->position.z, partialTicks)};
}

void Renderer::renderMobEntity(const EntityMob* mob, const float partialTicks) const {
    const auto found = mobMeshes.find(mob->mobType);
    if (found == mobMeshes.end() || found->second.texture == 0) return;
    const MobMesh& mesh = found->second;
    const glm::vec3 position = interpolatedPosition(mob, partialTicks);
    float bodyYaw = interpolateRotation(mob->lastRot.yaw, mob->rotation.yaw, partialTicks);
    const float headYaw = interpolateRotation(mob->previousHeadYaw, mob->headYaw, partialTicks);
    if (mob->mobType != 30 && mob->mobType != 210) {
        // entityLivingBase.func_110146_f: the body turns so the head stays within 75 degrees of it; otherwise a head whose yaw differs by 180 degrees from the body would point into it
        float difference = std::fmod(headYaw - bodyYaw, 360.0f);
        if (difference >= 180.0f) difference -= 360.0f;
        if (difference < -180.0f) difference += 360.0f;
        bodyYaw = headYaw - std::clamp(difference, -75.0f, 75.0f);
    }
    const float pitch = interpolateRotation(mob->lastRot.pitch, mob->rotation.pitch, partialTicks);
    const float limbSwing = mob->limbSwing
        - mob->limbSwingAmount * (1.0f - partialTicks);
    const float movement = std::lerp(mob->previousLimbSwingAmount,
        mob->limbSwingAmount, partialTicks);
    const float walk = std::cos(limbSwing * 0.6662f) * 1.4f * movement;
    const float rabbitJump = std::abs(std::sin(limbSwing * 0.6662f)) * movement;

    glm::mat4 base = glm::translate(glm::mat4(1.0f), position - cameraPos);
    base = glm::rotate(base, glm::radians(180.0f - bodyYaw), {0.0f, 1.0f, 0.0f});
    if (mob->isChild()) {
        base = glm::translate(base, {0.0f, 0.0f, 0.0f});
        base = glm::scale(base, glm::vec3(0.5f));
    }
    if (mob->isSmallArmorStand()) base = glm::scale(base, glm::vec3(0.5f));
    if (mob->mobType == 55 && mob->metadata.bytes[16].has_value()) {
        base = glm::scale(base, glm::vec3(static_cast<float>(std::max<int>(mob->metadata.bytes[16].value(), 1))));
    }

    float brightness = 1.0f;
    const int x = static_cast<int>(std::floor(mob->position.x));
    const int y = std::clamp(static_cast<int>(std::floor(mob->position.y + mob->height * 0.5f)),
        0, WORLD_HEIGHT - 1);
    const int z = static_cast<int>(std::floor(mob->position.z));
    if (const std::shared_ptr<Chunk> chunk = mc->world->getLoadedChunkAt(x, z)) {
        // light is only read; meshing holds this mutex for a long time, so never wait on it
        std::unique_lock lightLock(chunk->meshMutex, std::try_to_lock);
        const uint32_t light = chunk->getLightData(World::toLocalX(x), y, World::toLocalZ(z));
        const int blockLight = static_cast<int>(light & 15U);
        const int skyLight = std::max(static_cast<int>((light >> 4U) & 15U)
            - mc->world->skylightSubtracted(partialTicks), 0);
        const float normalized = static_cast<float>(std::max(blockLight, skyLight)) / 15.0f;
        brightness = normalized / ((1.0f - normalized) * 3.0f + 1.0f);
    }

    handShader->use();
    handShader->setMat4("projection", glm::value_ptr(projView));
    handShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
    handShader->setVec4("colorOverlay", mob->hurtTime > 0
        ? glm::vec4{1.0f, 0.0f, 0.0f, 0.3f}
        : glm::vec4{0.0f, 0.0f, 0.0f, 0.0f});
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    // entityHorse: type is metadata 19, the coat and markings are the packed int in 20
    const int horseType = mob->metadata.bytes[19].value_or(0);
    const bool longEars = horseType == 1 || horseType == 2;
    const auto drawMesh = [&](const MobMesh& drawn, const glm::vec3& tint, const GLuint textureOverride = 0) {
        handShader->setVec3("colorMultiplier", tint * brightness);
        glBindTexture(GL_TEXTURE_2D, textureOverride != 0 ? textureOverride : drawn.texture);
        glBindVertexArray(drawn.vao);
        std::vector<glm::mat4> partMatrices(drawn.parts.size(), base);
        for (size_t partIndex = 0; partIndex < drawn.parts.size(); ++partIndex) {
            const auto& [firstVertex, pivot, _rotation, animation, parent] = drawn.parts[partIndex];
            if (mob->mobType == 100) {
                if (longEars ? (partIndex == 19 || partIndex == 20) : (partIndex == 23 || partIndex == 24)) continue;
            }
            const uint8_t standFlags = mob->armorStandFlags();
            if ((animation == 11 || animation == 12) && (standFlags & 0x04U) == 0) continue;
            if (animation == 16 && (standFlags & 0x08U) != 0) continue;
            glm::vec3 rotation = _rotation;
            const auto armorStandRotation = [&](const size_t index, const glm::vec3 fallback,
                                                const bool reflectX = true,
                                                const bool reflectZ = false) {
                const glm::vec3 degrees = mob->metadata.rotations[index].value_or(fallback);
                // the mesh is vanilla's model space rotated 180 degrees about Z (X and Y flipped), which negates X and Y rotations and leaves Z alone
                return glm::radians(glm::vec3{
                    reflectX ? -degrees.x : degrees.x, -degrees.y,
                    reflectZ ? -degrees.z : degrees.z});
            };
            switch (animation) {
                case 1: // head
                    rotation.x += glm::radians(-pitch);
                    rotation.y += glm::radians(bodyYaw - headYaw);
                    break;
                case 2: case 4: rotation.x += walk; break;
                case 3: case 5: rotation.x -= walk; break;
                case 6: rotation.x = rabbitJump * -0.87266463f; break;
                case 7: rotation.x = 0.36651915f - rabbitJump * 0.87266463f; break;
                case 8: rotation.x = rabbitJump * 0.6981317f + 0.19198622f; break;
                case 9: rotation += armorStandRotation(11, {}); break;
                case 10: case 15: rotation += armorStandRotation(12, {}); break;
                case 11: rotation += armorStandRotation(13, {-10.0f,0.0f,-10.0f}); break;
                case 12: rotation += armorStandRotation(14, {-15.0f,0.0f,10.0f}); break;
                case 13: rotation += armorStandRotation(15, {-1.0f,0.0f,-1.0f}); break;
                case 14: rotation += armorStandRotation(16, {1.0f,0.0f,1.0f}); break;
                case 16: rotation.y -= glm::radians(bodyYaw); break;
                case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27: {
                    // modelSpider legs 1-8; the mesh is vanilla flipped in Y, which negates X and Z rotations
                    const int leg = animation - 20;
                    constexpr float quarterTurn = 0.78539816f, spread = 0.3926991f;
                    static constexpr float zScale[4]{1.0f, 0.74f, 0.74f, 1.0f};
                    static constexpr float yScale[4]{2.0f, 1.0f, -1.0f, -2.0f};
                    const float sign = (leg & 1) == 0 ? 1.0f : -1.0f; // legs 1,3,5,7 mirror legs 2,4,6,8
                    const int pair = leg / 2;
                    constexpr float pi = 3.14159265f;
                    constexpr float phase[4]{0.0f, pi, pi * 0.5f, pi * 1.5f};
                    const float yWalk = -(std::cos(limbSwing * 0.6662f * 2.0f + phase[pair]) * 0.4f) * movement;
                    const float zWalk = std::abs(std::sin(limbSwing * 0.6662f + phase[pair]) * 0.4f) * movement;
                    const float yaw = sign * (spread * yScale[pair] + yWalk);
                    // vanilla z = sign * (zWalk - quarterTurn * scale); negated for the flipped mesh
                    rotation.y = yaw;
                    rotation.z = sign * (quarterTurn * zScale[pair] - zWalk);
                    break;
                }
                case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37: {
                    // modelHorse walk cycle (no rearing or grazing); shins are relative to their legs
                    const float swing = std::cos(limbSwing * 0.6662f + 3.14159265f);
                    const float front = swing * 0.8f * movement;
                    const float back = swing * 0.5f * movement;
                    float angle = 0.0f; // vanilla absolute X angle (shins: relative to the leg)
                    switch (animation) {
                        case 30: angle = -back; break;
                        case 31: angle = -std::max(0.0f, back); break;
                        case 32: angle = back; break;
                        case 33: angle = -std::max(0.0f, -back); break;
                        case 34: angle = front; break;
                        case 35: angle = std::max(0.0f, back); break;
                        case 36: angle = -front; break;
                        default: angle = std::max(0.0f, -back); break;
                    }
                    rotation.x = -angle;
                    break;
                }
                default: break;
            }
            // modelRenderer applies Z, then Y, then X rotations
            glm::mat4 model = glm::translate(parent >= 0 ? partMatrices[static_cast<size_t>(parent)] : base, pivot);
            model = glm::rotate(model, rotation.z, {0.0f, 0.0f, 1.0f});
            model = glm::rotate(model, rotation.y, {0.0f, 1.0f, 0.0f});
            model = glm::rotate(model, rotation.x, {1.0f, 0.0f, 0.0f});
            partMatrices[partIndex] = model;
            handShader->setMat4("model", glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, firstVertex, 36);
        }
    };
    if (mob->mobType == 100) {
        const int variant = mob->metadata.ints[20].value_or(0);
        GLuint coat = horseCoatTextures[std::clamp(variant & 255, 0, 6)];
        switch (horseType) {
            case 1: coat = donkeyTexture; break;
            case 2: coat = muleTexture; break;
            case 3: coat = zombieHorseTexture; break;
            case 4: coat = skeletonHorseTexture; break;
            default: break;
        }
        drawMesh(mesh, {1.0f, 1.0f, 1.0f}, coat);
        const int marking = (variant & 0xFF00) >> 8;
        if (horseType == 0 && marking >= 1 && marking <= 4 && horseMarkingTextures[marking] != 0) {
            glDepthFunc(GL_LEQUAL); // the overlay is drawn over the same faces
            drawMesh(mesh, {1.0f, 1.0f, 1.0f}, horseMarkingTextures[marking]);
            glDepthFunc(GL_LESS);
        }
    } else {
        drawMesh(mesh, {1.0f, 1.0f, 1.0f});
    }
    if (mob->mobType == 91) {
        const bool sheared = mob->metadata.bytes[16].has_value()
            && (static_cast<uint8_t>(mob->metadata.bytes[16].value()) & 0x10U) != 0;
        if (const auto wool = mobMeshes.find(250); !sheared && wool != mobMeshes.end()
            && wool->second.texture != 0) {
            static constexpr glm::vec3 fleece[16]{
                {1.0f,1.0f,1.0f}, {0.85f,0.5f,0.2f}, {0.7f,0.3f,0.85f}, {0.4f,0.6f,0.85f},
                {0.9f,0.9f,0.2f}, {0.5f,0.8f,0.1f}, {0.95f,0.5f,0.65f}, {0.3f,0.3f,0.3f},
                {0.6f,0.6f,0.6f}, {0.3f,0.5f,0.6f}, {0.5f,0.25f,0.7f}, {0.2f,0.3f,0.7f},
                {0.4f,0.3f,0.2f}, {0.4f,0.5f,0.2f}, {0.6f,0.2f,0.2f}, {0.1f,0.1f,0.15f}
            };
            const int color = mob->metadata.bytes[16].has_value()
                ? static_cast<uint8_t>(mob->metadata.bytes[16].value()) & 15 : 0;
            drawMesh(wool->second, fleece[color]);
        }
    }
    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

void Renderer::queueEntityShadow(const Entity* entity, const float partialTicks,
                                 const float radius) const {
    const glm::vec3 position(
        std::lerp(entity->lastPos.x, entity->position.x, partialTicks),
        std::lerp(entity->lastPos.y, entity->position.y, partialTicks),
        std::lerp(entity->lastPos.z, entity->position.z, partialTicks));
    const glm::vec3 cameraDelta = position - cameraPos;
    const float cameraDistance = glm::length(cameraDelta);
    const float distanceAlpha = 1.0f - cameraDistance / 256.0f;
    if (distanceAlpha <= 0.0f) return;

    const int minimumX = static_cast<int>(std::floor(position.x - radius));
    const int maximumX = static_cast<int>(std::floor(position.x + radius));
    const int surfaceBlockY = static_cast<int>(std::floor(position.y + 0.05f));
    const int minimumZ = static_cast<int>(std::floor(position.z - radius));
    const int maximumZ = static_cast<int>(std::floor(position.z + radius));
    for (int x = minimumX; x <= maximumX; ++x) {
        for (int z = minimumZ; z <= maximumZ; ++z) {
                const Block block = mc->world->getBlockAt(x, surfaceBlockY - 1, z);
                if (const Block above = mc->world->getBlockAt(x, surfaceBlockY, z); !BlockUtil::isFullBlock(block) || BlockUtil::isFullBlock(above)) continue;
                const std::shared_ptr<Chunk> chunk = mc->world->getLoadedChunkAt(x, z);
                if (chunk == nullptr) continue;
                const uint32_t packedLight = chunk->getLightData(World::toLocalX(x),
                    std::clamp(surfaceBlockY, 0, WORLD_HEIGHT - 1), World::toLocalZ(z));
                const int lightLevel = std::max(static_cast<int>(packedLight & 15U),
                    static_cast<int>((packedLight >> 4U) & 15U)
                        - mc->world->skylightSubtracted(partialTicks));
                if (lightLevel <= 3) continue;
                const float normalizedLight = static_cast<float>(lightLevel) / 15.0f;
                const float lightBrightness = normalizedLight
                    / ((1.0f - normalizedLight) * 3.0f + 1.0f);
                const float opacity = std::clamp(
                    (distanceAlpha - (position.y - static_cast<float>(surfaceBlockY)) * 0.5f)
                        * 0.5f * lightBrightness,
                    0.0f, 1.0f);
                if (opacity <= 0.0f) continue;

                const float x0 = static_cast<float>(x) - cameraPos.x;
                const float x1 = static_cast<float>(x + 1) - cameraPos.x;
                const float surfaceY = static_cast<float>(surfaceBlockY) + 0.015625f - cameraPos.y;
                const float z0 = static_cast<float>(z) - cameraPos.z;
                const float z1 = static_cast<float>(z + 1) - cameraPos.z;
                const float u0 = (position.x - x) / (2.0f * radius) + 0.5f;
                const float u1 = (position.x - (x + 1)) / (2.0f * radius) + 0.5f;
                const float v0 = (position.z - z) / (2.0f * radius) + 0.5f;
                const float v1 = (position.z - (z + 1)) / (2.0f * radius) + 0.5f;
                const float vertices[]{
                    x0,surfaceY,z0,u0,v0,opacity, x0,surfaceY,z1,u0,v1,opacity,
                    x1,surfaceY,z1,u1,v1,opacity, x1,surfaceY,z1,u1,v1,opacity,
                    x1,surfaceY,z0,u1,v0,opacity, x0,surfaceY,z0,u0,v0,opacity,
                };
                entityShadowVertices.insert(entityShadowVertices.end(), std::begin(vertices), std::end(vertices));
        }
    }
}

void Renderer::renderEntityShadows() const {
    if (entityShadowVertices.empty()) return;
    entityShadowShader->use();
    entityShadowShader->setMat4("projection", glm::value_ptr(projection));
    entityShadowShader->setMat4("view", glm::value_ptr(view));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, entityShadowTexture);
    glBindVertexArray(entityShadowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, entityShadowVBO);
    glBufferData(GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(entityShadowVertices.size() * sizeof(float)),
        entityShadowVertices.data(), GL_STREAM_DRAW);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(entityShadowVertices.size() / 6));
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void Renderer::renderPlayerEntity(const EntityPlayer* player, const float partialTicks) const {

    const glm::vec3 playerPosition = interpolatedPosition(player, partialTicks);
    const float bodyYaw = interpolateRotation(player->prevRenderYawOffset,
        player->renderYawOffset, partialTicks);
    const float headYaw = interpolateRotation(player->prevRotationYawHead,
        player->rotationYawHead, partialTicks);
    const float pitch = interpolateRotation(player->lastRot.pitch, player->rotation.pitch, partialTicks);
    const float limbSwing = player->limbSwing - player->limbSwingAmount * (1.0f - partialTicks);
    const float limbSwingAmount = std::lerp(player->prevLimbSwingAmount,
        player->limbSwingAmount, partialTicks);
    const float walkCycle = std::cos(limbSwing * 0.6662f);
    // modelBiped.isRiding: arms tilt up by PI/5 and the legs point straight forward (signs flipped with the mesh)
    const bool seated = player->vehicleId >= 0;
    const float rightLegPitch = seated ? 1.2566371f : walkCycle * 1.4f * limbSwingAmount;
    const float leftLegPitch = seated ? 1.2566371f : -rightLegPitch;
    float rightArmPitch = -walkCycle * limbSwingAmount + (seated ? 0.6283185f : 0.0f);
    float leftArmPitch = walkCycle * limbSwingAmount + (seated ? 0.6283185f : 0.0f);
    const float age = static_cast<float>(mc->world->totalTime.load(std::memory_order_relaxed)) + partialTicks;
    const float idleArmPitch = std::sin(age * 0.067f) * 0.05f;
    const float idleArmRoll = std::cos(age * 0.09f) * 0.05f + 0.05f;
    float swing = player->swingProgress - player->prevSwingProgress;
    if (swing < 0.0f) swing += 1.0f;
    swing = player->prevSwingProgress + swing * partialTicks;
    const float vanillaBodySwing = std::sin(std::sqrt(swing) * static_cast<float>(M_PI) * 2.0f) * 0.2f;
    const float easedSwing = 1.0f - std::pow(1.0f - swing, 4.0f);
    const float swingPitch = std::sin(easedSwing * static_cast<float>(M_PI));
    const float headSwing = std::sin(swing * static_cast<float>(M_PI))
        * -(glm::radians(pitch) - 0.7f) * 0.75f;
    // modelBiped.heldItemRight == 1: rightArm.x = rightArm.x * 0.5 - PI / 10 (our X rotation is negated)
    const NetworkItemStack& held = player == mc->player.get()
        ? mc->player->inventory[36 + mc->player->currentHotbarSlot] : player->heldItem;
    if (held) rightArmPitch = rightArmPitch * 0.5f + static_cast<float>(M_PI) / 10.0f;
    rightArmPitch += swingPitch * 1.2f + headSwing;
    leftArmPitch -= vanillaBodySwing;
    const float bodySwingYaw = -vanillaBodySwing;
    const float rightArmYaw = -vanillaBodySwing * 3.0f;
    const float leftArmYaw = -vanillaBodySwing;
    const float rightArmRoll = std::sin(swing * static_cast<float>(M_PI)) * 0.4f;
    const float rightShoulderX = std::cos(vanillaBodySwing) * 0.3125f + 0.0625f;
    const float rightShoulderZ = -std::sin(vanillaBodySwing) * 0.3125f;
    const float leftShoulderX = -std::cos(vanillaBodySwing) * 0.3125f - 0.0625f;
    const float leftShoulderZ = std::sin(vanillaBodySwing) * 0.3125f;

    glm::mat4 base = glm::translate(glm::mat4(1.0f), playerPosition - cameraPos);
    base = glm::rotate(base, glm::radians(180.0f - bodyYaw), {0.0f, 1.0f, 0.0f});
    base = glm::scale(base, glm::vec3(0.9375f));
    const bool sneaking = player->isSneaking();
    if (sneaking) base = glm::translate(base, {0.0f, -0.2f, 0.0f});

    handShader->use();
    handShader->setMat4("projection", glm::value_ptr(projView));
    float playerBrightness = 1.0f;
    const int blockX = static_cast<int>(std::floor(player->position.x));
    const int blockY = std::clamp(static_cast<int>(std::floor(
        player->position.y + player->getEyeHeight())), 0, WORLD_HEIGHT - 1);
    const int blockZ = static_cast<int>(std::floor(player->position.z));
    if (const std::shared_ptr<Chunk> chunk = mc->world->getLoadedChunkAt(blockX, blockZ)) {
        // light is only read; meshing holds this mutex for a long time, so never wait on it
        std::unique_lock lightLock(chunk->meshMutex, std::try_to_lock);
        const uint32_t light = chunk->getLightData(World::toLocalX(blockX), blockY, World::toLocalZ(blockZ));
        const int blockLight = static_cast<int>(light & 15U);
        const int skyLight = std::max(static_cast<int>((light >> 4U) & 15U)
            - mc->world->skylightSubtracted(partialTicks), 0);
        const float normalizedLight = static_cast<float>(std::max(blockLight, skyLight)) / 15.0f;
        playerBrightness = normalizedLight / ((1.0f - normalizedLight) * 3.0f + 1.0f);
    }
    handShader->setVec3("colorMultiplier", {playerBrightness, playerBrightness, playerBrightness});
    handShader->setVec4("colorOverlay", player->hurtTime > 0
        ? glm::vec4{1.0f, 0.0f, 0.0f, 0.3f}
        : glm::vec4{0.0f, 0.0f, 0.0f, 0.0f});
    glActiveTexture(GL_TEXTURE0);
    const auto [skinTexture, slimSkin] = playerSkin(player);
    glBindTexture(GL_TEXTURE_2D, skinTexture);
    glBindVertexArray(slimSkin ? slimPlayerVAO : playerVAO);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const auto drawPart = [&](const int firstBox, const glm::vec3 pivot,
                              const float rotateX, const float rotateY = 0.0f,
                              const float rotateZ = 0.0f,
                              const glm::vec3 scale = glm::vec3(1.0f),
                              const glm::vec3 offset = glm::vec3(0.0f)) {
        glm::mat4 model = glm::translate(base, pivot);
        model = glm::rotate(model, rotateY, {0.0f, 1.0f, 0.0f});
        model = glm::rotate(model, rotateX, {1.0f, 0.0f, 0.0f});
        model = glm::rotate(model, rotateZ, {0.0f, 0.0f, 1.0f});
        model = glm::translate(model, offset);
        model = glm::scale(model, scale);
        handShader->setMat4("model", glm::value_ptr(model));
        glDrawArrays(GL_TRIANGLES, firstBox * 36, 72);
    };

    struct Pose { glm::vec3 pivot; float rotateX, rotateY, rotateZ; };
    const float legPivotY = sneaking ? 0.9375f : 0.75f;
    const float legPivotZ = sneaking ? 0.25f : 0.00625f;
    const Pose poses[6]{
        {{0.0f, sneaking ? 1.4375f : 1.5f, 0.0f}, glm::radians(-pitch), glm::radians(bodyYaw - headYaw), 0.0f},
        {{0.0f, 1.5f, 0.0f}, sneaking ? -0.5f : 0.0f, bodySwingYaw, 0.0f},
        {{rightShoulderX, 1.5f, rightShoulderZ},
            rightArmPitch + idleArmPitch + (sneaking ? -0.4f : 0.0f), rightArmYaw, idleArmRoll + rightArmRoll},
        {{leftShoulderX, 1.5f, leftShoulderZ},
            leftArmPitch - idleArmPitch + (sneaking ? -0.4f : 0.0f), leftArmYaw, -idleArmRoll},
        {{-0.125f, legPivotY, legPivotZ}, rightLegPitch, 0.0f, 0.0f},
        {{0.125f, legPivotY, legPivotZ}, leftLegPitch, 0.0f, 0.0f},
    };
    for (int index = 0; index < 6; ++index) {
        const auto&[pivot, rotateX, rotateY, rotateZ] = poses[index];
        drawPart(index * 2, pivot, rotateX, rotateY, rotateZ);
    }

    // armor layers: LayerArmorBase draws a ModelBiped per slot with the same pose as the skin parts
    const std::array<NetworkItemStack, 4> armorStacks = player == mc->player.get()
        ? std::array<NetworkItemStack, 4>{mc->player->inventory[8], mc->player->inventory[7],
              mc->player->inventory[6], mc->player->inventory[5]}
        : player->armor;
    static constexpr const char* materials[5]{"leather", "chainmail", "iron", "diamond", "gold"};
    glBindVertexArray(armorVAO);
    const auto drawArmorBox = [&](const int model, const int box, const int pose) {
        glm::mat4 matrix = glm::translate(base, poses[pose].pivot);
        matrix = glm::rotate(matrix, poses[pose].rotateY, {0.0f, 1.0f, 0.0f});
        matrix = glm::rotate(matrix, poses[pose].rotateX, {1.0f, 0.0f, 0.0f});
        matrix = glm::rotate(matrix, poses[pose].rotateZ, {0.0f, 0.0f, 1.0f});
        handShader->setMat4("model", glm::value_ptr(matrix));
        glDrawArrays(GL_TRIANGLES, (model * 7 + box) * 36, 36);
    };
    for (int slot = 0; slot < 4; ++slot) { // 0 boots, 1 leggings, 2 chestplate, 3 helmet
        const NetworkItemStack& stack = armorStacks[slot];
        if (!stack || stack.itemId < 298 || stack.itemId > 317) continue;
        if ((stack.itemId - 298) % 4 != (slot == 3 ? 0 : slot == 2 ? 1 : slot == 1 ? 2 : 3)) continue;
        const int material = (stack.itemId - 298) / 4;
        const bool leggings = slot == 1;
        const std::string base_name = std::string(materials[material]) + "_layer_" + (leggings ? "2" : "1");
        const int model = leggings ? 1 : 0;
        const auto drawLayer = [&](const GLuint texture, const glm::vec3& tint) {
            if (texture == 0) return;
            handShader->setVec3("colorMultiplier", tint * playerBrightness);
            glBindTexture(GL_TEXTURE_2D, texture);
            switch (slot) {
                case 3:
                    drawArmorBox(model, 0, 0);
                    drawArmorBox(model, 1, 0);
                    break;
                case 2:
                    drawArmorBox(model, 2, 1);
                    drawArmorBox(model, 3, 2);
                    drawArmorBox(model, 4, 3);
                    break;
                case 1:
                    drawArmorBox(model, 2, 1);
                    drawArmorBox(model, 5, 5);
                    drawArmorBox(model, 6, 4);
                    break;
                default:
                    drawArmorBox(model, 5, 5);
                    drawArmorBox(model, 6, 4);
                    break;
            }
        };
        // leather's dye color is NBT, which is not retained yet, so it uses the default brown
        const glm::vec3 tint = material == 0 ? glm::vec3(0xA0 / 255.0f, 0x65 / 255.0f, 0x40 / 255.0f)
                                             : glm::vec3(1.0f);
        drawLayer(armorTexture(base_name), tint);
        if (material == 0) drawLayer(armorTexture(base_name + "_overlay"), glm::vec3(1.0f));
    }
    glBindVertexArray(0);

    if (held) {
        // modelBiped.postRenderArm: the arm's pivot and rotations, same as drawPart(4, ...)
        glm::mat4 arm = glm::translate(base, {rightShoulderX, 1.5f, rightShoulderZ});
        arm = glm::rotate(arm, rightArmYaw, {0.0f, 1.0f, 0.0f});
        arm = glm::rotate(arm, rightArmPitch + idleArmPitch + (sneaking ? -0.4f : 0.0f),
            {1.0f, 0.0f, 0.0f});
        arm = glm::rotate(arm, idleArmRoll + rightArmRoll, {0.0f, 0.0f, 1.0f});
        // vanilla's rotation point is 5px from the body centre and 2px below the arm top; ours is 6px out at the arm top. ModelPlayer.postRenderArm moves slim arms a further 1px inward
        arm = glm::translate(arm, {(slimSkin ? -2.0f : -1.0f) / 16.0f, -2.0f / 16.0f, 0.0f});
        renderHeldItemThirdPerson(held, arm, sneaking, playerBrightness);
    }
    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);
}

GLuint Renderer::armorTexture(const std::string& name) const {
    if (const auto found = armorTextures.find(name); found != armorTextures.end()) return found->second;
    const std::string path = "assets/minecraft/textures/models/armor/" + name + ".png";
    return armorTextures.emplace(name, loadTexture(path.c_str())).first->second;
}

const Renderer::DisplayTransform& Renderer::thirdPersonTransform(const std::string& name,
                                                                 const bool blockModel) const {
    if (const auto found = thirdPersonTransformCache.find(name); found != thirdPersonTransformCache.end())
        return found->second;
    // block/block.json and builtin/generated defaults
    DisplayTransform transform = blockModel
        ? DisplayTransform{{10.0f, -45.0f, 170.0f}, {0.0f, 1.5f, -2.75f}, glm::vec3(0.375f)}
        : DisplayTransform{{-90.0f, 0.0f, 0.0f}, {0.0f, 1.0f, -3.0f}, glm::vec3(0.55f)};
    std::ifstream file("assets/minecraft/models/item/" + name + ".json");
    const std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (const size_t section = text.find("\"thirdperson\""); section != std::string::npos) {
        const auto readVector = [&](const char* key, glm::vec3& out) {
            const size_t keyPosition = text.find(key, section);
            if (keyPosition == std::string::npos) return;
            const size_t open = text.find('[', keyPosition);
            if (open == std::string::npos) return;
            glm::vec3 value;
            if (std::sscanf(text.c_str() + open, "[ %f , %f , %f", &value.x, &value.y, &value.z) == 3) out = value;
        };
        readVector("\"rotation\"", transform.rotation);
        readVector("\"translation\"", transform.translation);
        readVector("\"scale\"", transform.scale);
    }
    return thirdPersonTransformCache.emplace(name, transform).first->second;
}

void Renderer::renderHeldItemThirdPerson(const NetworkItemStack& stack, const glm::mat4& handMatrix,
                                         const bool sneaking, const float brightness) const {
    if (stack.itemId == 397) return; // skulls are not drawn in hand yet
    const Block heldBlock = BlockUtil::itemBlockState(stack.itemId, stack.damage);
    const bool blockModel = stack.itemId < 256
        && stack.itemId != 101 && stack.itemId != 102 && stack.itemId != 160
        && BlockUtil::getCrossPlantItemTexture(heldBlock) == nullptr;
    const DisplayTransform& display = thirdPersonTransform(
        ItemRegistry::identifier(stack.itemId, stack.damage), blockModel);

    // layerHeldItem: our local frame is vanilla's flipped in X and Y (scale(-1,-1,1))
    glm::mat4 model = glm::scale(handMatrix, {-1.0f, -1.0f, 1.0f});
    model = glm::translate(model, {-0.0625f, 0.4375f, 0.0625f});
    if (stack.itemId == 50 || stack.itemId == 75 || stack.itemId == 76) { // render type 2 (torches)
        model = glm::translate(model, {0.0f, 0.1875f, -0.3125f});
        model = glm::rotate(model, glm::radians(20.0f), {1.0f, 0.0f, 0.0f});
        model = glm::rotate(model, glm::radians(45.0f), {0.0f, 1.0f, 0.0f});
        model = glm::scale(model, {-0.375f, -0.375f, 0.375f});
    }
    if (sneaking) model = glm::translate(model, {0.0f, 0.203125f, 0.0f});
    // itemRenderer/RenderItem.preTransform scale(2), ItemCameraTransforms.applyTransform, renderItem scale(0.5)
    model = glm::scale(model, glm::vec3(2.0f));
    model = glm::translate(model, display.translation / 16.0f);
    model = glm::rotate(model, glm::radians(display.rotation.y), {0.0f, 1.0f, 0.0f});
    model = glm::rotate(model, glm::radians(display.rotation.x), {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians(display.rotation.z), {0.0f, 0.0f, 1.0f});
    model = glm::scale(model, display.scale);
    model = glm::scale(model, glm::vec3(0.5f));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glActiveTexture(GL_TEXTURE0);
    if (blockModel) {
        static constexpr int faceIndexToBlockFace[6]{1, 0, 3, 2, 4, 5};
        float tileOrigins[12];
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            const char* texture = BlockUtil::getBlockTexture(heldBlock, faceIndexToBlockFace[faceIndex]);
            const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(texture);
            tileOrigins[faceIndex * 2] = static_cast<float>(tileX) / atlasTilesPerRow;
            tileOrigins[faceIndex * 2 + 1] = static_cast<float>(tileY) / atlasTilesPerRow;
        }
        itemShader->use();
        itemShader->setMat4("projection", glm::value_ptr(projView));
        itemShader->setMat4("view", glm::value_ptr(glm::mat4(1.0f)));
        itemShader->setMat4("model", glm::value_ptr(model));
        itemShader->setVec2Array("tileOrigins", tileOrigins, 6);
        itemShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
        glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
        glBindVertexArray(damageVAO);
        {
            const auto [first, count] = blockItemRange(heldBlock);
            glDrawArrays(GL_TRIANGLES, first, count);
        }
    } else {
        const ItemExtrusionMesh* mesh = nullptr;
        GLuint texture = itemAtlasTexture;
        if (stack.itemId < 256) {
            const char* itemTexture = BlockUtil::getCrossPlantItemTexture(heldBlock);
            std::string textureName;
            if (stack.itemId == 101) textureName = "iron_bars";
            else if (stack.itemId == 102) textureName = "glass";
            else if (stack.itemId == 160) textureName = "glass_" + std::string(
                BlockUtil::dyeColorNames[stack.damage & 15]);
            else textureName = itemTexture == nullptr ? BlockUtil::getBlockTexture(heldBlock, 4) : itemTexture;
            const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(
                BlockUtil::getAtlasTexturePtr(blockAtlas.second, textureName));
            mesh = &getOrBuildBlockAtlasExtrusionMesh(tileX, tileY);
            texture = blockAtlasTexture;
        } else {
            mesh = &getOrBuildItemExtrusionMesh(itemAtlas.tile(
                ItemRegistry::identifier(stack.itemId, stack.damage)));
        }
        itemSpriteShader->use();
        itemSpriteShader->setMat4("projection", glm::value_ptr(projView));
        itemSpriteShader->setMat4("view", glm::value_ptr(glm::mat4(1.0f)));
        itemSpriteShader->setMat4("model", glm::value_ptr(model));
        itemSpriteShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
        itemSpriteShader->setFloat("opacity", 1.0f);
        itemSpriteShader->setVec2("uvMinimum", 0.0f, 0.0f);
        itemSpriteShader->setVec2("uvMaximum", 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(mesh->vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh->vertexCount);
    }
    glBindVertexArray(0);
}

void Renderer::renderEntityName(const Entity* entity, const std::string& name,
                                const float partialTicks) const {
    if (name.empty()) return;
    const glm::vec3 position(
        std::lerp(entity->lastPos.x, entity->position.x, partialTicks),
        std::lerp(entity->lastPos.y, entity->position.y, partialTicks),
        std::lerp(entity->lastPos.z, entity->position.z, partialTicks));
    if (glm::distance(position, cameraPos) > 64.0f) return;

    glm::mat4 billboard = glm::transpose(view);
    billboard[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
        position - cameraPos + glm::vec3(0.0f, entity->height + 0.5f, 0.0f));
    model *= billboard;
    model = glm::scale(model, glm::vec3(0.026666668f, -0.026666668f, 0.026666668f));
    const glm::mat4 textProjection = projection * view * model;
    const float textWidth = static_cast<float>(mc->fontRenderer->getStringWidth(name));

    glm::mat4 backgroundModel = glm::translate(model, {0.0f, 4.0f, 0.01f});
    backgroundModel = glm::scale(backgroundModel, {textWidth + 2.0f, 9.0f, 1.0f});
    selectionShader->use();
    selectionShader->setMat4("projection", glm::value_ptr(projection));
    selectionShader->setMat4("view", glm::value_ptr(view));
    selectionShader->setMat4("model", glm::value_ptr(backgroundModel));
    selectionShader->setVec4("color", {0.0f, 0.0f, 0.0f, 0.25f});
    glBindVertexArray(itemQuadVAO);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);

    mc->fontRenderer->shader->use();
    mc->fontRenderer->shader->setMat4("projection", glm::value_ptr(textProjection));
    const float x = -textWidth * 0.5f;
    glDepthMask(GL_FALSE);
    mc->fontRenderer->renderTextWithDepth(name, x, 0.0f, 1.0f,
        0x20FFFFFF, false);
    glDepthMask(GL_TRUE);
    mc->fontRenderer->renderTextWithDepth(name, x, 0.0f, 1.0f,
        0xFFFFFFFF, true);
    glEnable(GL_CULL_FACE);
}

const Renderer::ItemExtrusionMesh& Renderer::getOrBuildItemExtrusionMesh(const uint16_t tile) const {
    if (const auto found = itemExtrusionMeshCache.find(tile); found != itemExtrusionMeshCache.end())
        return found->second;

    const int iconSize = itemAtlasPixelWidth > 0 ? itemAtlasPixelWidth / ItemAtlas::tilesPerRow : 0;
    const int originPxX = (tile % ItemAtlas::tilesPerRow) * iconSize;
    const int originPxY = (tile / ItemAtlas::tilesPerRow) * iconSize;
    ItemExtrusionMesh mesh = buildExtrusionMesh(itemAtlasPixels.data(), itemAtlasPixelWidth, itemAtlasPixelHeight,
        originPxX, originPxY, iconSize);
    return itemExtrusionMeshCache.emplace(tile, mesh).first->second;
}

const Renderer::ItemExtrusionMesh& Renderer::getOrBuildBlockAtlasExtrusionMesh(const int tileX, const int tileY) const {
    const uint32_t key = static_cast<uint32_t>(tileX) << 16 | static_cast<uint32_t>(tileY);
    if (const auto found = blockAtlasExtrusionMeshCache.find(key); found != blockAtlasExtrusionMeshCache.end())
        return found->second;

    constexpr int atlasPixelSize = atlasTilesPerRow * atlasTileSize;
    ItemExtrusionMesh mesh = buildExtrusionMesh(blockAtlas.first.data(), atlasPixelSize, atlasPixelSize,
        tileX * atlasTileSize, tileY * atlasTileSize, atlasTileSize);
    return blockAtlasExtrusionMeshCache.emplace(key, mesh).first->second;
}

Renderer::ItemExtrusionMesh Renderer::buildExtrusionMesh(const unsigned char* pixels, const int pixelWidth,
        const int pixelHeight, const int originPxX, const int originPxY, const int iconSize) const {
    std::vector<float> vertices;
    if (pixelWidth > 0 && iconSize > 0) {
        constexpr float depth = 1.0f / 16.0f;
        constexpr float zFront = depth * 0.5f, zBack = -depth * 0.5f;

        const auto alphaAt = [&](const int lx, const int ly) -> int {
            if (lx < 0 || ly < 0 || lx >= iconSize || ly >= iconSize) return 0;
            const int px = originPxX + lx, py = originPxY + ly;
            return pixels[(static_cast<size_t>(py) * pixelWidth + px) * 4 + 3];
        };
        const auto addQuad = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                                  const glm::vec3& d, const glm::vec2& uv) {
            for (const glm::vec3* corner : {&a, &b, &c, &a, &c, &d}) {
                vertices.insert(vertices.end(), {corner->x, corner->y, corner->z, uv.x, uv.y});
            }
        };

        for (int ly = 0; ly < iconSize; ++ly) {
            for (int lx = 0; lx < iconSize; ++lx) {
                if (alphaAt(lx, ly) == 0) continue;
                const float x0 = -0.5f + static_cast<float>(lx) / iconSize;
                const float x1 = -0.5f + static_cast<float>(lx + 1) / iconSize;
                const float yTop = 0.5f - static_cast<float>(ly) / iconSize;
                const float yBottom = 0.5f - static_cast<float>(ly + 1) / iconSize;
                const float u0 = static_cast<float>(originPxX + lx) / pixelWidth;
                const float u1 = static_cast<float>(originPxX + lx + 1) / pixelWidth;
                const float v0 = static_cast<float>(originPxY + ly) / pixelHeight;
                const float v1 = static_cast<float>(originPxY + ly + 1) / pixelHeight;
                const glm::vec2 centerUv{(u0 + u1) * 0.5f, (v0 + v1) * 0.5f};

                // front and back faces, each corner textured with its own uv
                for (const auto& v : {std::pair{glm::vec3{x0, yBottom, zFront}, glm::vec2{u0, v1}},
                        std::pair{glm::vec3{x1, yBottom, zFront}, glm::vec2{u1, v1}},
                        std::pair{glm::vec3{x1, yTop, zFront}, glm::vec2{u1, v0}},
                        std::pair{glm::vec3{x0, yBottom, zFront}, glm::vec2{u0, v1}},
                        std::pair{glm::vec3{x1, yTop, zFront}, glm::vec2{u1, v0}},
                        std::pair{glm::vec3{x0, yTop, zFront}, glm::vec2{u0, v0}}}) {
                    vertices.insert(vertices.end(), {v.first.x, v.first.y, v.first.z, v.second.x, v.second.y});
                }
                for (const auto& v : {std::pair{glm::vec3{x1, yBottom, zBack}, glm::vec2{u1, v1}},
                        std::pair{glm::vec3{x0, yBottom, zBack}, glm::vec2{u0, v1}},
                        std::pair{glm::vec3{x0, yTop, zBack}, glm::vec2{u0, v0}},
                        std::pair{glm::vec3{x1, yBottom, zBack}, glm::vec2{u1, v1}},
                        std::pair{glm::vec3{x0, yTop, zBack}, glm::vec2{u0, v0}},
                        std::pair{glm::vec3{x1, yTop, zBack}, glm::vec2{u1, v0}}}) {
                    vertices.insert(vertices.end(), {v.first.x, v.first.y, v.first.z, v.second.x, v.second.y});
                }

                // side walls at alpha boundaries, flat-shaded via the pixel's own texel color
                if (alphaAt(lx - 1, ly) == 0) addQuad({x0, yBottom, zBack}, {x0, yBottom, zFront},
                    {x0, yTop, zFront}, {x0, yTop, zBack}, centerUv);
                if (alphaAt(lx + 1, ly) == 0) addQuad({x1, yBottom, zFront}, {x1, yBottom, zBack},
                    {x1, yTop, zBack}, {x1, yTop, zFront}, centerUv);
                if (alphaAt(lx, ly - 1) == 0) addQuad({x0, yTop, zBack}, {x1, yTop, zBack},
                    {x1, yTop, zFront}, {x0, yTop, zFront}, centerUv);
                if (alphaAt(lx, ly + 1) == 0) addQuad({x0, yBottom, zFront}, {x1, yBottom, zFront},
                    {x1, yBottom, zBack}, {x0, yBottom, zBack}, centerUv);
            }
        }
    }

    ItemExtrusionMesh mesh;
    mesh.vertexCount = static_cast<GLsizei>(vertices.size() / 5);
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)),
        vertices.empty() ? nullptr : vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    return mesh;
}

void Renderer::renderItemEntity(const EntityItem* item, const float partialTicks) const {
    glm::vec3 position(
        std::lerp(item->lastPos.x, item->position.x, partialTicks),
        std::lerp(item->lastPos.y, item->position.y, partialTicks),
        std::lerp(item->lastPos.z, item->position.z, partialTicks));
    if (item->collecting) {
        if (const std::shared_ptr<Entity> collector = item->collector.lock()) {
            const glm::dvec3 collectorPosition{
                std::lerp(collector->lastPos.x, collector->position.x, partialTicks),
                std::lerp(collector->lastPos.y, collector->position.y, partialTicks) + 0.5,
                std::lerp(collector->lastPos.z, collector->position.z, partialTicks)
            };
            float progress = (static_cast<float>(item->collectionAge) + partialTicks) / 3.0f;
            progress = std::clamp(progress, 0.0f, 1.0f);
            progress *= progress;
            position = glm::vec3(item->collectionStart
                + (collectorPosition - item->collectionStart) * static_cast<double>(progress));
        }
    }
    const float age = static_cast<float>(item->age) + partialTicks;
    const float animation = age / 10.0f + item->hoverStart;
    const float bob = std::sin(animation) * 0.1f + 0.1f;
    glm::mat4 baseModel = glm::translate(glm::mat4(1.0f),
        position - cameraPos + glm::vec3(0, bob + 0.125f, 0));
    baseModel = glm::rotate(baseModel, age / 20.0f + item->hoverStart,
        {0.0f, 1.0f, 0.0f});
    const int copies = item->count > 48 ? 5 : item->count > 32 ? 4
        : item->count > 16 ? 3 : item->count > 1 ? 2 : 1;
    const auto instanceModels = [&](const float scale, const bool layered) {
        static constexpr glm::vec3 offsets[5]{
            {0.0f, 0.0f, 0.0f}, {-0.109f, 0.071f, 0.094f},
            {0.126f, -0.057f, -0.081f}, {0.042f, 0.132f, -0.137f},
            {-0.138f, -0.103f, 0.028f}
        };
        std::array<glm::mat4, 5> models{};
        for (int copy = 0; copy < copies; ++copy) {
            const glm::vec3 offset = layered ? glm::vec3(0.0f, 0.0f, copy * 0.046875f) : offsets[copy];
            models[copy] = glm::scale(glm::translate(baseModel, offset), glm::vec3(scale));
        }
        return models;
    };
    GLuint glintVao = 0;
    GLint glintFirst = 0;
    GLsizei glintCount = 0;
    std::array<glm::mat4, 5> glintModels{};

    float brightness = 1.0f;
    if (const std::shared_ptr<Chunk> chunk = mc->world->getLoadedChunkAt(position.x, position.z)) {
        // light is only read; meshing holds this mutex for a long time, so never wait on it
        std::unique_lock lightLock(chunk->meshMutex, std::try_to_lock);
        const int y = std::clamp(static_cast<int>(std::floor(position.y)), 0, WORLD_HEIGHT - 1);
        const uint32_t light = chunk->getLightData(World::toLocalX(position.x), y, World::toLocalZ(position.z));
        const int blockLight = static_cast<int>(light & 15U);
        const int skyLight = std::max(static_cast<int>((light >> 4U) & 15U)
            - mc->world->skylightSubtracted(partialTicks), 0);
        const float level = static_cast<float>(std::max(blockLight, skyLight)) / 15.0f;
        brightness = level / ((1.0f - level) * 3.0f + 1.0f);
    }
    if (item->itemId < 0) return; // item data hasn't arrived yet (entity metadata packet pending)
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    constexpr int skullItemId = 397;
    constexpr int anvilItemId = 145;
    constexpr int ironBarsItemId = 101;
    constexpr int glassPaneItemId = 102;
    constexpr int stainedGlassPaneItemId = 160;
    if (item->itemId == skullItemId) {
        struct SkullVariant { GLuint texture; GLuint vao; float yOffset; };
        const SkullVariant variants[5]{
            {skeletonSkullTexture, skullVAO, 0.0f},
            {witherSkullTexture, skullVAO, 0.0f},
            {zombieSkullTexture, playerVAO, -0.25f},
            {playerSkinTexture, playerVAO, -0.25f},
            {creeperSkullTexture, skullVAO, 0.0f},
        };
        const auto&[texture, vao, yOffset] = variants[std::clamp(static_cast<int>(item->damage), 0, 4)];
        handShader->use();
        handShader->setMat4("projection", glm::value_ptr(projView));
        handShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
        handShader->setVec4("colorOverlay", {0.0f, 0.0f, 0.0f, 0.0f});
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindVertexArray(vao);
        std::mt19937 random(187);
        std::uniform_real_distribution<float> offset(-0.15f, 0.15f);
        for (int copy = 0; copy < copies; ++copy) {
            glm::mat4 model = baseModel;
            if (copy > 0) model = glm::translate(model,
                glm::vec3(offset(random), offset(random), offset(random)));
            model = glm::scale(model, glm::vec3(0.5f));
            model = glm::translate(model, {0.0f, yOffset, 0.0f});
            handShader->setMat4("model", glm::value_ptr(model));
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }
    } else if (item->itemId == anvilItemId) {
        const int damaged = std::clamp(static_cast<int>(item->damage), 0, 2);
        const std::string topTexture = "anvil_top_damaged_" + std::to_string(damaged);
        const auto [baseX, baseY] = blockAtlas.second.blockAtlasPos.at(BlockUtil::getAtlasTexturePtr(blockAtlas.second, "anvil_base"));
        const auto [topX, topY] = blockAtlas.second.blockAtlasPos.at(BlockUtil::getAtlasTexturePtr(blockAtlas.second, topTexture));
        const glm::vec2 baseOrigin{static_cast<float>(baseX) / atlasTilesPerRow,
            static_cast<float>(baseY) / atlasTilesPerRow};
        const glm::vec2 topOrigin{static_cast<float>(topX) / atlasTilesPerRow,
            static_cast<float>(topY) / atlasTilesPerRow};
        float allBaseOrigins[12];
        float topPlateOrigins[12];
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            allBaseOrigins[faceIndex * 2] = baseOrigin.x;
            allBaseOrigins[faceIndex * 2 + 1] = baseOrigin.y;
            const bool isUpFace = faceIndex == 4;
            const glm::vec2& origin = isUpFace ? topOrigin : baseOrigin;
            topPlateOrigins[faceIndex * 2] = origin.x;
            topPlateOrigins[faceIndex * 2 + 1] = origin.y;
        }
        itemShader->use();
        itemShader->setMat4("projection", glm::value_ptr(projection));
        itemShader->setMat4("view", glm::value_ptr(view));
        itemShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
        glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
        glBindVertexArray(anvilVAO);
        const auto models = instanceModels(0.25f, false);
        itemShader->setMat4Array("instanceModels", glm::value_ptr(models[0]), copies);
        itemShader->setBool("useInstances", true);
        itemShader->setVec2Array("tileOrigins", allBaseOrigins, 6);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 108, copies);
        itemShader->setVec2Array("tileOrigins", topPlateOrigins, 6);
        glDrawArraysInstanced(GL_TRIANGLES, 108, 36, copies);
        glintVao = anvilVAO; glintCount = 144; glintModels = models;
    } else if (item->itemId == ironBarsItemId || item->itemId == glassPaneItemId
            || item->itemId == stainedGlassPaneItemId) {
        const std::string texture = item->itemId == ironBarsItemId ? "iron_bars"
            : item->itemId == glassPaneItemId ? "glass"
            : "glass_" + std::string(BlockUtil::dyeColorNames[item->damage & 15]);
        const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(BlockUtil::getAtlasTexturePtr(blockAtlas.second, texture));
        const ItemExtrusionMesh& mesh = getOrBuildBlockAtlasExtrusionMesh(tileX, tileY);
        itemSpriteShader->use();
        itemSpriteShader->setMat4("projection", glm::value_ptr(projection));
        itemSpriteShader->setMat4("view", glm::value_ptr(view));
        itemSpriteShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
        itemSpriteShader->setFloat("opacity", 1.0f);
        itemSpriteShader->setVec2("uvMinimum", 0.0f, 0.0f);
        itemSpriteShader->setVec2("uvMaximum", 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
        glBindVertexArray(mesh.vao);
        glDisable(GL_CULL_FACE);
        const auto models = instanceModels(0.5f, true);
        itemSpriteShader->setMat4Array("instanceModels", glm::value_ptr(models[0]), copies);
        itemSpriteShader->setBool("useInstances", true);
        glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, copies);
        glintVao = mesh.vao; glintCount = mesh.vertexCount; glintModels = models;
        glEnable(GL_CULL_FACE);
    } else if (item->itemId < 256 && BlockUtil::getCrossPlantItemTexture(
            BlockUtil::itemBlockState(item->itemId, item->damage))) {
        const Block block = BlockUtil::itemBlockState(item->itemId, item->damage);
        const char* texture = BlockUtil::getCrossPlantItemTexture(block);
        const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(BlockUtil::getAtlasTexturePtr(blockAtlas.second, texture));
        const ItemExtrusionMesh& mesh = getOrBuildBlockAtlasExtrusionMesh(tileX, tileY);
        itemSpriteShader->use();
        itemSpriteShader->setMat4("projection", glm::value_ptr(projection));
        itemSpriteShader->setMat4("view", glm::value_ptr(view));
        const glm::vec3 tint = blockId(block) == blockId(BLOCK_TALLGRASS)
            && (blockMeta(block) == 1 || blockMeta(block) == 2)
            ? RendererDetail::sampleClimateColor(0.5f, 1.0f, false) : glm::vec3(1.0f);
        itemSpriteShader->setVec3("colorMultiplier", tint * brightness);
        itemSpriteShader->setFloat("opacity", 1.0f);
        itemSpriteShader->setVec2("uvMinimum", 0.0f, 0.0f);
        itemSpriteShader->setVec2("uvMaximum", 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
        glBindVertexArray(mesh.vao);
        glDisable(GL_CULL_FACE);
        const auto models = instanceModels(0.5f, true);
        itemSpriteShader->setMat4Array("instanceModels", glm::value_ptr(models[0]), copies);
        itemSpriteShader->setBool("useInstances", true);
        glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, copies);
        glintVao = mesh.vao; glintCount = mesh.vertexCount; glintModels = models;
        glEnable(GL_CULL_FACE);
    } else if (item->itemId < 256) {
        const Block block = BlockUtil::itemBlockState(item->itemId, item->damage);
        static constexpr int faceIndexToBlockFace[6] = {1, 0, 3, 2, 4, 5};
        float tileOrigins[12];
        for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
            const char* texture = BlockUtil::getBlockTexture(block, faceIndexToBlockFace[faceIndex]);
            const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(texture);
            tileOrigins[faceIndex * 2] = static_cast<float>(tileX) / atlasTilesPerRow;
            tileOrigins[faceIndex * 2 + 1] = static_cast<float>(tileY) / atlasTilesPerRow;
        }
        const bool isSlab = item->itemId == 44 || item->itemId == 126 || item->itemId == 182;
        itemShader->use();
        itemShader->setMat4("projection", glm::value_ptr(projection));
        itemShader->setMat4("view", glm::value_ptr(view));
        itemShader->setVec2Array("tileOrigins", tileOrigins, 6);
        glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
        glBindVertexArray(damageVAO);
        const glm::vec3 blockTint = blockItemTint(block);
        const auto models = instanceModels(0.25f, false);
        itemShader->setMat4Array("instanceModels", glm::value_ptr(models[0]), copies);
        itemShader->setBool("useInstances", true);
        if (blockId(block) == blockId(BLOCK_GRASS)) {
            itemShader->setVec3("colorMultiplier", blockTint * brightness);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, copies);
            itemShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
            glDrawArraysInstanced(GL_TRIANGLES, 6, 30, copies);
        } else {
            itemShader->setVec3("colorMultiplier", blockTint * brightness);
            const auto [first, count] = blockItemRange(block);
            glDrawArraysInstanced(GL_TRIANGLES, first, count, copies);
            glintFirst = first;
            glintCount = count;
        }
        if (blockId(block) == blockId(BLOCK_GRASS)) glintCount = 36;
        glintVao = damageVAO; glintModels = models;
    } else {
        const std::string& identifier = ItemRegistry::identifier(item->itemId, item->damage);
        const uint16_t tile = itemAtlas.tile(identifier);
        const ItemExtrusionMesh& mesh = getOrBuildItemExtrusionMesh(tile);
        itemSpriteShader->use();
        itemSpriteShader->setMat4("projection", glm::value_ptr(projection));
        itemSpriteShader->setMat4("view", glm::value_ptr(view));
        itemSpriteShader->setVec3("colorMultiplier", {brightness, brightness, brightness});
        itemSpriteShader->setFloat("opacity", 1.0f);
        itemSpriteShader->setVec2("uvMinimum", 0.0f, 0.0f);
        itemSpriteShader->setVec2("uvMaximum", 1.0f, 1.0f);
        glBindTexture(GL_TEXTURE_2D, itemAtlasTexture);
        glBindVertexArray(mesh.vao);
        glDisable(GL_CULL_FACE);
        const auto models = instanceModels(0.5f, true);
        itemSpriteShader->setMat4Array("instanceModels", glm::value_ptr(models[0]), copies);
        itemSpriteShader->setBool("useInstances", true);
        glDrawArraysInstanced(GL_TRIANGLES, 0, mesh.vertexCount, copies);
        glintVao = mesh.vao; glintCount = mesh.vertexCount; glintModels = models;
        glEnable(GL_CULL_FACE);
    }
    if (item->enchanted && glintVao != 0 && glintCount > 0)
        renderItemGlint(glintVao, glintFirst, glintCount, glintModels.data(), copies);
    glDisable(GL_BLEND);
    itemShader->use();
    itemShader->setBool("useInstances", false);
    itemSpriteShader->use();
    itemSpriteShader->setBool("useInstances", false);
    glBindVertexArray(0);
}

void Renderer::renderItemGlint(const GLuint vao, const GLint first, const GLsizei count,
                               const glm::mat4* models, const int instances) const {
    itemGlintShader->use();
    itemGlintShader->setMat4("projection", glm::value_ptr(projection));
    itemGlintShader->setMat4("view", glm::value_ptr(view));
    itemGlintShader->setMat4Array("instanceModels", glm::value_ptr(models[0]), instances);
    itemGlintShader->setFloat("glintOffset", static_cast<float>(glfwGetTime() * 0.02));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, itemGlintTexture);
    glBlendFunc(GL_SRC_COLOR, GL_ONE);
    glDepthFunc(GL_EQUAL);
    glBindVertexArray(vao);
    glDrawArraysInstanced(GL_TRIANGLES, first, count, instances);
    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderGuiBlockItem(const Block block, const float x, const float y,
                                  const float size) const {
    static constexpr int faceIndexToBlockFace[6]{1, 0, 3, 2, 4, 5};
    float tileOrigins[12];
    for (int faceIndex = 0; faceIndex < 6; ++faceIndex) {
        const char* texture = BlockUtil::getBlockTexture(block, faceIndexToBlockFace[faceIndex]);
        const auto [tileX, tileY] = blockAtlas.second.blockAtlasPos.at(texture);
        tileOrigins[faceIndex * 2] = static_cast<float>(tileX) / atlasTilesPerRow;
        tileOrigins[faceIndex * 2 + 1] = static_cast<float>(tileY) / atlasTilesPerRow;
    }

    const glm::mat4 guiProjection = glm::ortho(0.0f, static_cast<float>(width),
        static_cast<float>(height), 0.0f, -100.0f, 100.0f);
    // renderItem.setupGuiTransform followed by RenderItem.renderItem's 0.5 scale. The local cube is already centered at the origin, unlike vanilla's baked 0..1 vertices, so it does not need vanilla's -0.5 translation
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
        {x + size * 0.5f, y + size * 0.5f, 0.0f});
    model = glm::scale(model, {size * 0.625f, size * 0.625f, -size * 0.625f});
    model = glm::rotate(model, glm::radians(210.0f), {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians(-135.0f), {0.0f, 1.0f, 0.0f});
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    itemShader->use();
    itemShader->setMat4("projection", glm::value_ptr(guiProjection));
    itemShader->setMat4("view", glm::value_ptr(glm::mat4(1.0f)));
    itemShader->setMat4("model", glm::value_ptr(model));
    itemShader->setVec2Array("tileOrigins", tileOrigins, 6);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, blockAtlasTexture);
    glBindVertexArray(damageVAO);
    const glm::vec3 tint = blockItemTint(block);
    if (blockId(block) == blockId(BLOCK_GRASS)) {
        itemShader->setVec3("colorMultiplier", tint);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        itemShader->setVec3("colorMultiplier", {1.0f, 1.0f, 1.0f});
        glDrawArrays(GL_TRIANGLES, 6, 30);
    } else {
        itemShader->setVec3("colorMultiplier", tint);
        const auto [first, count] = blockItemRange(block);
        glDrawArrays(GL_TRIANGLES, first, count);
    }
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderGuiSkullItem(const int damage, const float x, const float y,
                                  const float size) const {
    const glm::mat4 guiProjection = glm::ortho(0.0f, static_cast<float>(width),
        static_cast<float>(height), 0.0f, -100.0f, 100.0f);
    glm::mat4 model = glm::translate(glm::mat4(1.0f),
        {x + size * 0.5f, y + size * 0.52f, 0.0f});
    model = glm::scale(model, {-size * 0.8f, -size * 0.8f, size * 0.8f});
    model = glm::rotate(model, glm::radians(210.0f), {1.0f, 0.0f, 0.0f});
    model = glm::rotate(model, glm::radians(-135.0f), {0.0f, 1.0f, 0.0f});
    if (damage == 2 || damage == 3) model = glm::translate(model, {0.0f, -0.25f, 0.0f});
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    handShader->use();
    handShader->setMat4("projection", glm::value_ptr(guiProjection));
    handShader->setMat4("model", glm::value_ptr(model));
    handShader->setVec3("colorMultiplier", {1.0f, 1.0f, 1.0f});
    handShader->setVec4("colorOverlay", {0.0f, 0.0f, 0.0f, 0.0f});
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, skullTexture(*this, damage));
    glBindVertexArray(skullVao(*this, damage));
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Renderer::renderInventoryPlayer(const float x, const float y,
                                     const float previewWidth, const float previewHeight,
                                     const float mouseX, const float mouseY) const {
    const glm::mat4 savedProjection = projection;
    const glm::mat4 savedView = view;
    const glm::mat4 savedProjView = projView;
    const glm::vec3 savedCameraPos = cameraPos;
    const Rotation savedRotation = mc->player->rotation;
    const Rotation savedLastRotation = mc->player->lastRot;
    const float savedRenderYawOffset = mc->player->renderYawOffset;
    const float savedPrevRenderYawOffset = mc->player->prevRenderYawOffset;
    const float savedHeadYaw = mc->player->rotationYawHead;
    const float savedPrevHeadYaw = mc->player->prevRotationYawHead;

    const float bodyYaw = std::atan(mouseX / 40.0f) * 20.0f;
    const float headYaw = std::atan(mouseX / 40.0f) * 40.0f;
    const float pitch = -std::atan(mouseY / 40.0f) * 20.0f;
    mc->player->renderYawOffset = mc->player->prevRenderYawOffset = bodyYaw;
    mc->player->rotation.yaw = mc->player->lastRot.yaw = headYaw;
    mc->player->rotation.pitch = mc->player->lastRot.pitch = pitch;
    mc->player->rotationYawHead = mc->player->prevRotationYawHead = headYaw;

    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<int>(x), height - static_cast<int>(y + previewHeight),
        static_cast<int>(previewWidth), static_cast<int>(previewHeight));
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(static_cast<int>(x), height - static_cast<int>(y + previewHeight),
        static_cast<int>(previewWidth), static_cast<int>(previewHeight));
    projection = glm::perspective(glm::radians(30.0f), previewWidth / previewHeight, 0.05f, 20.0f);
    view = glm::mat4(1.0f);
    projView = projection;
    const float partialTicks = static_cast<float>(mc->timer->partialTicks);
    const glm::vec3 interpolatedPosition{
        std::lerp(static_cast<float>(mc->player->lastPos.x),
            static_cast<float>(mc->player->position.x), partialTicks),
        std::lerp(static_cast<float>(mc->player->lastPos.y),
            static_cast<float>(mc->player->position.y), partialTicks),
        std::lerp(static_cast<float>(mc->player->lastPos.z),
            static_cast<float>(mc->player->position.z), partialTicks)
    };
    cameraPos = interpolatedPosition + glm::vec3(0.0f, 0.9f, 4.2f);
    renderPlayerEntity(mc->player.get(), partialTicks);

    projection = savedProjection;
    view = savedView;
    projView = savedProjView;
    cameraPos = savedCameraPos;
    mc->player->rotation = savedRotation;
    mc->player->lastRot = savedLastRotation;
    mc->player->renderYawOffset = savedRenderYawOffset;
    mc->player->prevRenderYawOffset = savedPrevRenderYawOffset;
    mc->player->rotationYawHead = savedHeadYaw;
    mc->player->prevRotationYawHead = savedPrevHeadYaw;
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
