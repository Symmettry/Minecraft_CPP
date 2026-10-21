#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/menu/impl/GuiInventory.hpp"
#include "net/lily/minecpp/net/packets/play/client/C0DPacketCloseWindow.hpp"
#include "net/lily/minecpp/net/packets/play/client/C0EPacketClickWindow.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"

class GuiChest final : public Gui {
    uint8_t windowId;
    std::string title;
    int containerSlots;
    std::vector<NetworkItemStack> slots;
    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    bool serverClosing = false;
    int16_t transactionId = 0;
    static inline GLuint backgroundTexture = 0;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    [[nodiscard]] int rows() const {
        return std::clamp((containerSlots + 8) / 9, 1, 6);
    }

    [[nodiscard]] int guiHeight() const {
        return 114 + rows() * 18;
    }

    [[nodiscard]] int playerSlotIndex(const int inventorySlot) const {
        if (inventorySlot >= 9 && inventorySlot < 36)
            return containerSlots + inventorySlot - 9;
        if (inventorySlot >= 36 && inventorySlot < 45)
            return containerSlots + 27 + inventorySlot - 36;
        return -1;
    }

    [[nodiscard]] int hoveredSlot() const {
        const int factor = scaleFactor();
        const float width = static_cast<float>((mc->width() + factor - 1) / factor);
        const float height = static_cast<float>((mc->height() + factor - 1) / factor);
        const float left = (width - 176.0f) * 0.5f;
        const float top = (height - static_cast<float>(guiHeight())) * 0.5f;
        for (int slot = 0; slot < containerSlots; ++slot) {
            const float x = left + 8.0f + static_cast<float>(slot % 9) * 18.0f;
            const float y = top + 18.0f + static_cast<float>(slot / 9) * 18.0f;
            if (mouseX >= x && mouseX < x + 16.0f && mouseY >= y && mouseY < y + 16.0f)
                return slot;
        }
        const float inventoryTop = top + static_cast<float>(rows() * 18 + 32);
        for (int row = 0; row < 3; ++row) for (int column = 0; column < 9; ++column) {
            const float x = left + 8.0f + static_cast<float>(column) * 18.0f;
            const float y = inventoryTop + static_cast<float>(row) * 18.0f;
            if (mouseX >= x && mouseX < x + 16.0f && mouseY >= y && mouseY < y + 16.0f)
                return containerSlots + row * 9 + column;
        }
        for (int column = 0; column < 9; ++column) {
            const float x = left + 8.0f + static_cast<float>(column) * 18.0f;
            const float y = inventoryTop + 58.0f;
            if (mouseX >= x && mouseX < x + 16.0f && mouseY >= y && mouseY < y + 16.0f)
                return containerSlots + 27 + column;
        }
        return -999;
    }

    void synchronizePlayerInventory() const {
        for (int slot = 9; slot < 45; ++slot) {
            const int windowSlot = playerSlotIndex(slot);
            if (windowSlot >= 0 && windowSlot < static_cast<int>(slots.size()))
                mc->player->inventory[slot] = slots[windowSlot];
        }
    }

    void clickSlot(const int slot, const bool rightClick, const bool shiftClick) {
        const NetworkItemStack clicked = slot >= 0 && slot < static_cast<int>(slots.size())
            ? slots[slot] : NetworkItemStack{};
        const int mode = shiftClick ? 1 : 0;
        mc->netClient->sendPacket(C0EPacketClickWindow{windowId, static_cast<int16_t>(slot),
            static_cast<uint8_t>(rightClick ? 1 : 0), transactionId++,
            static_cast<uint8_t>(mode), clicked});

        if (shiftClick) return;
        if (slot < 0 || slot >= static_cast<int>(slots.size())) {
            NetworkItemStack& carried = mc->player->carriedItem;
            if (carried && rightClick && carried.count > 1) --carried.count;
            else carried = {};
            return;
        }
        NetworkItemStack& carried = mc->player->carriedItem;
        NetworkItemStack& target = slots[slot];
        if (!carried) {
            carried = target;
            if (rightClick && carried) {
                carried.count = static_cast<int8_t>((target.count + 1) / 2);
                target.count = static_cast<int8_t>(target.count / 2);
                if (target.count == 0) target = {};
            } else target = {};
        } else if (!target) {
            target = carried;
            if (rightClick) {
                target.count = 1;
                if (--carried.count == 0) carried = {};
            } else carried = {};
        } else if (target.itemId == carried.itemId && target.damage == carried.damage
                   && target.nbt == carried.nbt
                   && target.count < 64) {
            const int moved = rightClick ? 1 : std::min<int>(carried.count, 64 - target.count);
            target.count = static_cast<int8_t>(target.count + moved);
            carried.count = static_cast<int8_t>(carried.count - moved);
            if (carried.count == 0) carried = {};
        } else if (!rightClick) std::swap(target, carried);
        synchronizePlayerInventory();
    }

public:
    GuiChest(const uint8_t id, std::string titleJson, const int slotCount)
        : windowId(id), title(ChatComponent::fromJson(titleJson).plainText()),
          containerSlots(slotCount), slots(static_cast<size_t>(slotCount + 36)) {
        if (title.empty()) title = "Chest";
    }

    [[nodiscard]] uint8_t getWindowId() const { return windowId; }

    void setItems(const std::vector<NetworkItemStack>& items) {
        slots = items;
        if (slots.size() < static_cast<size_t>(containerSlots + 36))
            slots.resize(static_cast<size_t>(containerSlots + 36));
        synchronizePlayerInventory();
    }

    void setSlot(const int slot, const NetworkItemStack& stack) {
        if (slot < 0) return;
        if (slot >= static_cast<int>(slots.size())) slots.resize(static_cast<size_t>(slot + 1));
        slots[slot] = stack;
        synchronizePlayerInventory();
    }

    void closeFromServer() {
        serverClosing = true;
        mc->player->carriedItem = {};
        mc->displayGuiScreen(nullptr);
    }

    void onOpen() override {
        if (backgroundTexture == 0) backgroundTexture = loadTexture(
            "assets/minecraft/textures/gui/container/generic_54.png");
        mc->settings->inputEnabled = false;
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void onClose() override {
        if (!serverClosing && mc->netClient != nullptr && mc->netClient->isHandlerDone())
            mc->netClient->sendPacket(C0DPacketCloseWindow{windowId});
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
        const float width = static_cast<float>((mc->width() + factor - 1) / factor);
        const float height = static_cast<float>((mc->height() + factor - 1) / factor);
        const float left = (width - 176.0f) * 0.5f;
        const float top = (height - static_cast<float>(guiHeight())) * 0.5f;
        const float upperHeight = static_cast<float>(rows() * 18 + 17);
        drawTexture(backgroundTexture, left * scale, top * scale, 176.0f * scale,
            upperHeight * scale, 0.0f, 0.0f, 256.0f, 256.0f, 176.0f, upperHeight);
        drawTexture(backgroundTexture, left * scale, (top + upperHeight) * scale,
            176.0f * scale, 96.0f * scale, 0.0f, 126.0f, 256.0f, 256.0f, 176.0f, 96.0f);

        mc->fontRenderer->renderText(title, (left + 8.0f) * scale, (top + 6.0f) * scale,
            scale, 0xFF404040, false);
        mc->fontRenderer->renderText("Inventory", (left + 8.0f) * scale,
            (top + static_cast<float>(guiHeight() - 94)) * scale, scale, 0xFF404040, false);

        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const int hovered = hoveredSlot();
        const auto drawSlot = [&](const int slot, const float x, const float y) {
            if (slot >= 0 && slot < static_cast<int>(slots.size()))
                GuiInventory::drawStack(slots[slot], x * scale, y * scale, scale);
            if (slot == hovered) drawRect(x * scale, y * scale, 16.0f * scale, 16.0f * scale,
                {1.0f, 1.0f, 1.0f, 0.35f});
        };
        for (int slot = 0; slot < containerSlots; ++slot)
            drawSlot(slot, left + 8.0f + static_cast<float>(slot % 9) * 18.0f,
                top + 18.0f + static_cast<float>(slot / 9) * 18.0f);
        const float inventoryTop = top + static_cast<float>(rows() * 18 + 32);
        for (int row = 0; row < 3; ++row) for (int column = 0; column < 9; ++column)
            drawSlot(containerSlots + row * 9 + column,
                left + 8.0f + static_cast<float>(column) * 18.0f,
                inventoryTop + static_cast<float>(row) * 18.0f);
        for (int column = 0; column < 9; ++column)
            drawSlot(containerSlots + 27 + column,
                left + 8.0f + static_cast<float>(column) * 18.0f, inventoryTop + 58.0f);
        GuiInventory::drawStack(mc->player->carriedItem,
            (static_cast<float>(mouseX) - 8.0f) * scale,
            (static_cast<float>(mouseY) - 8.0f) * scale, scale);
        if (!mc->player->carriedItem && hovered >= 0
            && hovered < static_cast<int>(slots.size()) && slots[hovered])
            GuiInventory::drawTooltip(slots[hovered], scale, mouseX, mouseY);
        glEnable(GL_DEPTH_TEST);
    }

    bool handleKey(const int key, int, const int action, int) override {
        if (action == GLFW_PRESS && (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_E))
            mc->displayGuiScreen(nullptr);
        return true;
    }

    bool handleMouseButton(const int button, const int action, const int mods) override {
        if (action != GLFW_PRESS || (button != GLFW_MOUSE_BUTTON_LEFT
            && button != GLFW_MOUSE_BUTTON_RIGHT)) return true;
        clickSlot(hoveredSlot(), button == GLFW_MOUSE_BUTTON_RIGHT, (mods & GLFW_MOD_SHIFT) != 0);
        return true;
    }

    bool handleCursorPos(const double x, const double y) override {
        int windowWidth = 1;
        int windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        const int factor = scaleFactor();
        mouseX = x * mc->width() / std::max(windowWidth, 1) / factor;
        mouseY = y * mc->height() / std::max(windowHeight, 1) / factor;
        return true;
    }

    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    [[nodiscard]] bool pausesGame() const override { return mc->isSingleplayer(); }
    void tick() const override {
        if (mc->world != nullptr && mc->netClient != nullptr && mc->netClient->isHandlerDone())
            mc->world->update();
    }
};
