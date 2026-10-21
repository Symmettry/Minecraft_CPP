#pragma once

#include <algorithm>
#include <array>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/net/packets/play/client/C0DPacketCloseWindow.hpp"
#include "net/lily/minecpp/net/packets/play/client/C0EPacketClickWindow.hpp"
#include "net/lily/minecpp/util/ItemRegistry.hpp"

class GuiInventory final : public Gui {
    struct SlotPosition { int slot; int x; int y; };

    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    int dragButton = -1;
    std::vector<int> dragSlots;
    int pendingDragButton = -1;
    int pendingDragSlot = -999;
    double lastClickTime = -1.0;
    int lastClickSlot = -999;
    int lastClickButton = -1;
    static inline GLuint backgroundTexture = 0;
    static inline std::array<GLuint, 4> armorSlotTextures{};

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    [[nodiscard]] static std::array<SlotPosition, 45> slotPositions() {
        std::array<SlotPosition, 45> slots{};
        size_t index = 0;
        slots[index++] = {0, 144, 36};
        for (int row = 0; row < 2; ++row) for (int column = 0; column < 2; ++column)
            slots[index++] = {1 + row * 2 + column, 88 + column * 18, 26 + row * 18};
        for (int armor = 0; armor < 4; ++armor) slots[index++] = {5 + armor, 8, 8 + armor * 18};
        for (int row = 0; row < 3; ++row) for (int column = 0; column < 9; ++column)
            slots[index++] = {9 + row * 9 + column, 8 + column * 18, 84 + row * 18};
        for (int column = 0; column < 9; ++column)
            slots[index++] = {36 + column, 8 + column * 18, 142};
        return slots;
    }

    [[nodiscard]] int hoveredSlot() const {
        const int factor = scaleFactor();
        const float logicalWidth = static_cast<float>((mc->width() + factor - 1) / factor);
        const float logicalHeight = static_cast<float>((mc->height() + factor - 1) / factor);
        const float left = (logicalWidth - 176.0f) * 0.5f;
        const float top = (logicalHeight - 166.0f) * 0.5f;
        for (const SlotPosition& slot : slotPositions()) {
            const float x = left + static_cast<float>(slot.x);
            const float y = top + static_cast<float>(slot.y);
            if (mouseX >= x && mouseX < x + 16.0f
                && mouseY >= y && mouseY < y + 16.0f) return slot.slot;
        }
        return -999;
    }

    [[nodiscard]] static bool canMerge(const NetworkItemStack& first,
                                       const NetworkItemStack& second) {
        return first && second && first.itemId == second.itemId && first.damage == second.damage
            && first.nbt == second.nbt;
    }

    [[nodiscard]] static int armorSlotFor(const NetworkItemStack& stack) {
        if (!stack) return -1;
        switch (stack.itemId) {
            case 86: case 397:
            case 298: case 302: case 306: case 310: case 314: return 5;
            case 299: case 303: case 307: case 311: case 315: return 6;
            case 300: case 304: case 308: case 312: case 316: return 7;
            case 301: case 305: case 309: case 313: case 317: return 8;
            default: return -1;
        }
    }

    [[nodiscard]] static int shiftEquipSlotFor(const NetworkItemStack& stack) {
        if (!stack || stack.itemId == 86 || stack.itemId == 397) return -1;
        return armorSlotFor(stack);
    }

    [[nodiscard]] static bool canPlaceInSlot(const int slot,
                                             const NetworkItemStack& stack) {
        if (slot == 0) return false;
        if (slot >= 5 && slot <= 8) return !stack || armorSlotFor(stack) == slot;
        return true;
    }

    [[nodiscard]] static int slotStackLimit(const int slot) {
        return slot >= 5 && slot <= 8 ? 1 : 64;
    }

    void sendClick(const int slot, const int button, const int mode,
                   const NetworkItemStack& clickedItem) const {
        mc->netClient->sendPacket(C0EPacketClickWindow{0, static_cast<int16_t>(slot),
            static_cast<uint8_t>(button), mc->player->inventoryTransactionId++,
            static_cast<uint8_t>(mode),
            clickedItem});
    }

    static void mergeIntoRange(NetworkItemStack& source, const int begin, const int end) {
        for (int slot = begin; slot < end && source; ++slot) {
            NetworkItemStack& target = mc->player->inventory[slot];
            if (!canMerge(source, target) || target.count >= 64) continue;
            const int moved = std::min<int>(source.count, 64 - target.count);
            target.count = static_cast<int8_t>(target.count + moved);
            source.count = static_cast<int8_t>(source.count - moved);
            if (source.count == 0) source = {};
        }
        for (int slot = begin; slot < end && source; ++slot) {
            NetworkItemStack& target = mc->player->inventory[slot];
            if (target) continue;
            target = source;
            source = {};
        }
    }

    void shiftClick(const int slot) const {
        if (slot < 0 || slot >= static_cast<int>(mc->player->inventory.size())) return;
        NetworkItemStack& source = mc->player->inventory[slot];
        const NetworkItemStack clicked = source;
        if (!source) return;
        const int armorSlot = shiftEquipSlotFor(source);
        if (slot >= 9 && armorSlot >= 0 && !mc->player->inventory[armorSlot]) {
            mc->player->inventory[armorSlot] = source;
            mc->player->inventory[armorSlot].count = 1;
            if (--source.count == 0) source = {};
        } else if (slot >= 9 && slot < 36) mergeIntoRange(source, 36, 45);
        else if (slot >= 36) mergeIntoRange(source, 9, 36);
        else {
            mergeIntoRange(source, 9, 36);
            mergeIntoRange(source, 36, 45);
        }
        sendClick(slot, 0, 1, clicked);
    }

    void pickupClick(const int slot, const bool rightClick) const {
        NetworkItemStack& carried = mc->player->carriedItem;
        if (slot == -999) {
            if (!carried) return;
            sendClick(slot, rightClick ? 1 : 0, 0, {});
            if (rightClick && carried.count > 1) --carried.count;
            else carried = {};
            return;
        }
        if (slot < 0 || slot >= static_cast<int>(mc->player->inventory.size())) return;

        NetworkItemStack& target = mc->player->inventory[slot];
        const NetworkItemStack clicked = target;
        if (!carried) {
            if (target) {
                if (rightClick) {
                    carried = target;
                    carried.count = static_cast<int8_t>((target.count + 1) / 2);
                    target.count = static_cast<int8_t>(target.count / 2);
                    if (target.count == 0) target = {};
                } else {
                    carried = target;
                    target = {};
                }
            }
        } else if (!canPlaceInSlot(slot, carried)) {
            sendClick(slot, rightClick ? 1 : 0, 0, clicked);
            return;
        } else if (!target) {
            target = carried;
            const int moved = rightClick ? 1
                : std::min<int>(carried.count, slotStackLimit(slot));
            target.count = static_cast<int8_t>(moved);
            carried.count = static_cast<int8_t>(carried.count - moved);
            if (carried.count == 0) carried = {};
        } else if (canMerge(carried, target) && target.count < slotStackLimit(slot)) {
            const int moved = rightClick ? 1 : std::min<int>(carried.count,
                slotStackLimit(slot) - target.count);
            target.count = static_cast<int8_t>(target.count + moved);
            carried.count = static_cast<int8_t>(carried.count - moved);
            if (carried.count == 0) carried = {};
        } else if (!rightClick) {
            std::swap(target, carried);
        }
        sendClick(slot, rightClick ? 1 : 0, 0, clicked);
    }

    void beginDrag(const int button, const int firstSlot) {
        dragButton = button;
        dragSlots.clear();
        sendClick(-999, button == GLFW_MOUSE_BUTTON_LEFT ? 0 : 4, 5, {});
        addDragSlot(firstSlot);
        addDragSlot(hoveredSlot());
    }

    void clearPendingDrag() {
        pendingDragButton = -1;
        pendingDragSlot = -999;
    }

    void addDragSlot(const int slot) {
        if (dragButton < 0 || slot < 0
            || slot >= static_cast<int>(mc->player->inventory.size())) return;
        if (!canPlaceInSlot(slot, mc->player->carriedItem)) return;
        if (std::ranges::find(dragSlots, slot) != dragSlots.end()) return;
        if (const NetworkItemStack& target = mc->player->inventory[slot]; target
            && (!canMerge(target, mc->player->carriedItem)
                || target.count >= slotStackLimit(slot))) return;
        dragSlots.push_back(slot);
        sendClick(slot, dragButton == GLFW_MOUSE_BUTTON_LEFT ? 1 : 5, 5, {});
    }

    void finishDrag() {
        if (dragButton < 0) return;
        NetworkItemStack& carried = mc->player->carriedItem;
        if (carried && !dragSlots.empty()) {
            const int originalCount = carried.count;
            const int perSlot = dragButton == GLFW_MOUSE_BUTTON_RIGHT
                ? 1 : std::max(1, originalCount / static_cast<int>(dragSlots.size()));
            for (const int slot : dragSlots) {
                if (!carried) break;
                NetworkItemStack& target = mc->player->inventory[slot];
                const int existing = target ? target.count : 0;
                const int moved = std::min({perSlot, static_cast<int>(carried.count),
                    slotStackLimit(slot) - existing});
                if (moved <= 0) continue;
                if (!target) {
                    target = carried;
                    target.count = 0;
                }
                target.count = static_cast<int8_t>(target.count + moved);
                carried.count = static_cast<int8_t>(carried.count - moved);
                if (carried.count == 0) carried = {};
            }
        }
        sendClick(-999, dragButton == GLFW_MOUSE_BUTTON_LEFT ? 2 : 6, 5, {});
        dragButton = -1;
        dragSlots.clear();
    }

    [[nodiscard]] std::pair<NetworkItemStack, NetworkItemStack> dragPreview(
            const int requestedSlot) const {
        NetworkItemStack requested = requestedSlot >= 0
            ? mc->player->inventory[requestedSlot] : NetworkItemStack{};
        NetworkItemStack carried = mc->player->carriedItem;
        if (dragButton < 0 || !carried || dragSlots.empty()) return {requested, carried};
        const int perSlot = dragButton == GLFW_MOUSE_BUTTON_RIGHT
            ? 1 : std::max(1, static_cast<int>(carried.count) / static_cast<int>(dragSlots.size()));
        for (const int slot : dragSlots) {
            NetworkItemStack target = mc->player->inventory[slot];
            const int existing = target ? target.count : 0;
            const int moved = std::min({perSlot, static_cast<int>(carried.count),
                slotStackLimit(slot) - existing});
            if (moved <= 0) continue;
            if (!target) {
                target = carried;
                target.count = 0;
            }
            target.count = static_cast<int8_t>(target.count + moved);
            carried.count = static_cast<int8_t>(carried.count - moved);
            if (slot == requestedSlot) requested = target;
            if (carried.count == 0) {
                carried = {};
                break;
            }
        }
        return {requested, carried};
    }

    void collectMatching(const int slot) const {
        NetworkItemStack& carried = mc->player->carriedItem;
        if (!carried) return;
        const NetworkItemStack clicked = slot >= 0 ? mc->player->inventory[slot] : NetworkItemStack{};
        for (NetworkItemStack& stack : mc->player->inventory) {
            if (!canMerge(stack, carried)) continue;
            const int moved = std::min<int>(stack.count, 64 - carried.count);
            carried.count = static_cast<int8_t>(carried.count + moved);
            stack.count = static_cast<int8_t>(stack.count - moved);
            if (stack.count == 0) stack = {};
            if (carried.count >= 64) break;
        }
        sendClick(slot, 0, 6, clicked);
    }

public:
    static std::string tooltipText(const std::string& source) {
        return source;
    }

    static void drawTooltip(const NetworkItemStack& stack, const float scale,
                            const double mouseX, const double mouseY) {
        std::vector<std::string> lines;
        lines.push_back(tooltipText(stack.customName.empty()
            ? ItemRegistry::displayName(stack.itemId, stack.damage) : stack.customName));
        for (const std::string& line : stack.lore) lines.push_back(tooltipText(line));
        const float logicalWidth = static_cast<float>(mc->width()) / scale;
        const float logicalHeight = static_cast<float>(mc->height()) / scale;
        int widestLine = 0;
        for (const std::string& line : lines)
            widestLine = std::max(widestLine, mc->fontRenderer->getStringWidth(line));
        const float tooltipWidth = static_cast<float>(widestLine);
        float x = static_cast<float>(mouseX) + 12.0f;
        float y = static_cast<float>(mouseY) - 12.0f;
        if (x + tooltipWidth + 4.0f > logicalWidth) x -= tooltipWidth + 24.0f;
        const float textHeight = lines.size() == 1 ? 8.0f
            : 10.0f + static_cast<float>(lines.size() - 1) * 10.0f;
        if (y + textHeight + 8.0f > logicalHeight) y = logicalHeight - textHeight - 8.0f;

        const float left = x - 3.0f;
        const float top = y - 4.0f;
        const float width = tooltipWidth + 6.0f;
        const float height = textHeight + 8.0f;
        const auto rect = [scale](const float rx, const float ry, const float rw,
                                  const float rh, const glm::vec4& color) {
            drawRect(rx * scale, ry * scale, rw * scale, rh * scale, color);
        };
        constexpr glm::vec4 background{16.0f / 255.0f, 0.0f, 16.0f / 255.0f,
            240.0f / 255.0f};
        constexpr glm::vec4 borderTop{80.0f / 255.0f, 0.0f, 1.0f, 80.0f / 255.0f};
        constexpr glm::vec4 borderBottom{40.0f / 255.0f, 0.0f, 127.0f / 255.0f,
            80.0f / 255.0f};
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        // guiScreen.drawHoveringText uses five background strips so each corner is cut back by one GUI pixel
        rect(left, top, width, 1.0f, background);
        rect(left, top + height - 1.0f, width, 1.0f, background);
        rect(left, top + 1.0f, width, height - 2.0f, background);
        rect(left - 1.0f, top + 1.0f, 1.0f, height - 2.0f, background);
        rect(left + width, top + 1.0f, 1.0f, height - 2.0f, background);

        rect(left, top + 1.0f, width, 1.0f, borderTop);
        rect(left, top + height - 2.0f, width, 1.0f, borderBottom);
        const float borderMiddle = (height - 4.0f) * 0.5f;
        rect(left, top + 2.0f, 1.0f, borderMiddle, borderTop);
        rect(left + width - 1.0f, top + 2.0f, 1.0f, borderMiddle, borderTop);
        rect(left, top + 2.0f + borderMiddle, 1.0f, borderMiddle, borderBottom);
        rect(left + width - 1.0f, top + 2.0f + borderMiddle,
            1.0f, borderMiddle, borderBottom);
        for (size_t line = 0; line < lines.size(); ++line) {
            const float lineY = y + (line == 0 ? 0.0f : 2.0f + static_cast<float>(line) * 10.0f);
            mc->fontRenderer->renderText(lines[line], x * scale, lineY * scale, scale,
                line == 0 ? 0xFFFFFFFF : 0xFFAA00AA, true);
        }
    }

    static void drawStack(const NetworkItemStack& stack, const float x, const float y, const float scale) {
        if (!stack) return;
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        GLuint texture = 0;
        float u = 0.0f;
        float v = 0.0f;
        float textureSize;
        constexpr float regionSize = 16.0f;
        if (stack.itemId == 397) {
            mc->renderer->renderGuiSkullItem(stack.damage, x, y, 16.0f * scale);
        } else if (stack.itemId < 256) {
            const Block block = BlockUtil::itemBlockState(stack.itemId, stack.damage);
            const bool generatedModel = stack.itemId == 101 || stack.itemId == 102
                || stack.itemId == 160 || BlockUtil::getCrossPlantItemTexture(block) != nullptr;
            if (!generatedModel) {
                mc->renderer->renderGuiBlockItem(block, x, y, 16.0f * scale);
                texture = 0;
            }
            const char* name = BlockUtil::getCrossPlantItemTexture(block);
            if (stack.itemId == 101) name = BlockUtil::getAtlasTexturePtr(
                mc->renderer->blockAtlas.second, "iron_bars");
            else if (stack.itemId == 102) name = BlockUtil::getAtlasTexturePtr(
                mc->renderer->blockAtlas.second, "glass");
            else if (stack.itemId == 160) name = BlockUtil::getAtlasTexturePtr(
                mc->renderer->blockAtlas.second,
                "glass_" + std::string(BlockUtil::dyeColorNames[stack.damage & 15]));
            else if (name == nullptr) name = BlockUtil::getBlockTexture(block, 4);
            // the atlas is keyed by its own string pointers, so a literal name has to be resolved first
            name = BlockUtil::getAtlasTexturePtr(mc->renderer->blockAtlas.second, name);
            const auto [tileX, tileY] = mc->renderer->blockAtlas.second.blockAtlasPos.at(name);
            if (generatedModel) texture = mc->renderer->blockAtlasTexture;
            u = static_cast<float>(tileX * atlasTileSize);
            v = static_cast<float>(tileY * atlasTileSize);
            textureSize = static_cast<float>(atlasTilesPerRow * atlasTileSize);
        } else {
            const uint16_t tile = mc->renderer->itemAtlas.tile(
                ItemRegistry::identifier(stack.itemId, stack.damage));
            texture = mc->renderer->getItemAtlasTexture();
            u = static_cast<float>((tile % ItemAtlas::tilesPerRow) * atlasTileSize);
            v = static_cast<float>((tile / ItemAtlas::tilesPerRow) * atlasTileSize);
            textureSize = static_cast<float>(ItemAtlas::tilesPerRow * atlasTileSize);
        }
        if (texture != 0) drawTexture(texture, x, y, 16.0f * scale, 16.0f * scale,
            u, v, textureSize, textureSize, regionSize, regionSize);
        if (stack.count > 1) {
            const std::string count = std::to_string(stack.count);
            mc->fontRenderer->renderText(count,
                x + (17.0f - mc->fontRenderer->getStringWidth(count)) * scale,
                y + 9.0f * scale, scale, 0xFFFFFFFF, true);
        }
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void onOpen() override {
        if (backgroundTexture == 0) backgroundTexture = loadTexture(
            "assets/minecraft/textures/gui/container/inventory.png");
        static constexpr std::array<const char*, 4> armorTextures{
            "empty_armor_slot_helmet", "empty_armor_slot_chestplate",
            "empty_armor_slot_leggings", "empty_armor_slot_boots"
        };
        for (size_t index = 0; index < armorSlotTextures.size(); ++index) {
            if (armorSlotTextures[index] == 0) armorSlotTextures[index] = loadTexture(
                "assets/minecraft/textures/items/" + std::string(armorTextures[index]) + ".png");
        }
        mc->settings->inputEnabled = false;
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    void onClose() override {
        if (mc->netClient != nullptr && mc->netClient->isHandlerDone()) {
            mc->netClient->sendPacket(C0DPacketCloseWindow{0});
        }
        mc->player->carriedItem = {};
    }
    void update(float) override {}

    void render() const override {
        mc->renderer->render(mc->world);
        mc->renderIngameHud();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawRect(0.0f, 0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()),
            {0.0f, 0.0f, 0.0f, 0.45f});

        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const float logicalWidth = static_cast<float>((mc->width() + factor - 1) / factor);
        const float logicalHeight = static_cast<float>((mc->height() + factor - 1) / factor);
        const float left = (logicalWidth - 176.0f) * 0.5f;
        const float top = (logicalHeight - 166.0f) * 0.5f;
        drawTexture(backgroundTexture, left * scale, top * scale, 176.0f * scale, 166.0f * scale,
            0.0f, 0.0f, 256.0f, 256.0f, 176.0f, 166.0f);
        mc->renderer->renderInventoryPlayer((left + 26.0f) * scale, (top + 8.0f) * scale,
            51.0f * scale, 70.0f * scale,
            left + 51.0f - static_cast<float>(mouseX),
            top + 25.0f - static_cast<float>(mouseY));

        mc->fontRenderer->shader->use();
        mc->fontRenderer->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        mc->fontRenderer->renderText("Crafting", (left + 86.0f) * scale,
            (top + 6.0f) * scale, scale, 0xFF404040, false);

        // fontRenderer restores world-rendering state after drawing. Inventory sprites need alpha blending even when their slot has no item and drawStack therefore returns without restoring GUI state
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (const SlotPosition& slot : slotPositions()) {
            const float slotX = left + static_cast<float>(slot.x);
            const float slotY = top + static_cast<float>(slot.y);
            if (slot.slot >= 5 && slot.slot <= 8 && !mc->player->inventory[slot.slot]) {
                drawTexture(armorSlotTextures[slot.slot - 5], slotX * scale, slotY * scale,
                    16.0f * scale, 16.0f * scale, 0.0f, 0.0f, 16.0f, 16.0f, 16.0f, 16.0f);
            }
            const NetworkItemStack displayed = dragPreview(slot.slot).first;
            drawStack(displayed, slotX * scale, slotY * scale, scale);
            if (mouseX >= slotX && mouseX < slotX + 16.0f
                && mouseY >= slotY && mouseY < slotY + 16.0f) {
                drawRect(slotX * scale, slotY * scale, 16.0f * scale, 16.0f * scale,
                    {1.0f, 1.0f, 1.0f, 0.35f});
            }
        }

        if (const NetworkItemStack displayedCarried = dragPreview(-1).second) drawStack(displayedCarried,
                                        (static_cast<float>(mouseX) - 8.0f) * scale,
                                        (static_cast<float>(mouseY) - 8.0f) * scale, scale);
        else if (const int slot = hoveredSlot(); slot >= 0 && mc->player->inventory[slot]) {
            drawTooltip(mc->player->inventory[slot], scale, mouseX, mouseY);
        }
        glEnable(GL_DEPTH_TEST);
    }

    bool handleKey(const int key, int, const int action, int) override {
        if (action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_E)) {
            mc->displayGuiScreen(nullptr);
        } else if (action == GLFW_PRESS && key >= GLFW_KEY_1 && key <= GLFW_KEY_9) {
            if (const int slot = hoveredSlot(); slot != -999) {
                const int hotbar = key - GLFW_KEY_1;
                const int hotbarSlot = 36 + hotbar;
                if (!canPlaceInSlot(slot, mc->player->inventory[hotbarSlot])) return true;
                const NetworkItemStack clicked = mc->player->inventory[slot];
                std::swap(mc->player->inventory[slot], mc->player->inventory[hotbarSlot]);
                sendClick(slot, hotbar, 2, clicked);
            }
        } else if (action == GLFW_PRESS && key == GLFW_KEY_Q) {
            const int slot = hoveredSlot();
            if (slot != -999 && mc->player->inventory[slot]) {
                const NetworkItemStack clicked = mc->player->inventory[slot];
                if (glfwGetKey(mc->renderer->window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                    || glfwGetKey(mc->renderer->window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS) {
                    mc->player->inventory[slot] = {};
                    sendClick(slot, 1, 4, clicked);
                } else {
                    if (--mc->player->inventory[slot].count == 0) mc->player->inventory[slot] = {};
                    sendClick(slot, 0, 4, clicked);
                }
            }
        }
        return true;
    }
    bool handleMouseButton(const int button, const int action, const int mods) override {
        if (button == GLFW_MOUSE_BUTTON_MIDDLE && action == GLFW_PRESS) {
            const int slot = hoveredSlot();
            if (mc->player->gameType == GameType::CREATIVE && slot >= 0
                && mc->player->inventory[slot]) {
                const NetworkItemStack clicked = mc->player->inventory[slot];
                mc->player->carriedItem = clicked;
                mc->player->carriedItem.count = 64;
                sendClick(slot, 2, 3, clicked);
            }
            return true;
        }
        if (button != GLFW_MOUSE_BUTTON_LEFT && button != GLFW_MOUSE_BUTTON_RIGHT) return true;
        if (action == GLFW_RELEASE) {
            if (dragButton == button) {
                finishDrag();
            } else if (pendingDragButton == button) {
                pickupClick(pendingDragSlot, button == GLFW_MOUSE_BUTTON_RIGHT);
            }
            clearPendingDrag();
            return true;
        }
        if (action != GLFW_PRESS) return true;
        const int slot = hoveredSlot();
        const double now = glfwGetTime();
        const bool doubleClick = button == GLFW_MOUSE_BUTTON_LEFT && slot == lastClickSlot
            && lastClickButton == button && now - lastClickTime < 0.25;
        if (doubleClick && mc->player->carriedItem) collectMatching(slot);
        else if (button == GLFW_MOUSE_BUTTON_LEFT && (mods & GLFW_MOD_SHIFT) != 0) shiftClick(slot);
        else if (mc->player->carriedItem && slot >= 0) {
            pendingDragButton = button;
            pendingDragSlot = slot;
        }
        else pickupClick(slot, button == GLFW_MOUSE_BUTTON_RIGHT);
        lastClickTime = now;
        lastClickSlot = slot;
        lastClickButton = button;
        return true;
    }
    bool handleCursorPos(const double x, const double y) override {
        int windowWidth = 1;
        int windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        const int factor = scaleFactor();
        mouseX = x * mc->width() / std::max(windowWidth, 1) / factor;
        mouseY = y * mc->height() / std::max(windowHeight, 1) / factor;
        const int currentSlot = hoveredSlot();
        if (pendingDragButton >= 0 && currentSlot >= 0 && currentSlot != pendingDragSlot) {
            beginDrag(pendingDragButton, pendingDragSlot);
            clearPendingDrag();
        }
        if (dragButton >= 0) addDragSlot(currentSlot);
        return true;
    }
    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    [[nodiscard]] bool pausesGame() const override { return mc->isSingleplayer(); }
    void tick() const override {
        if (mc->world != nullptr && mc->netClient != nullptr && mc->netClient->isHandlerDone()) {
            mc->world->update();
        }
    }
};
