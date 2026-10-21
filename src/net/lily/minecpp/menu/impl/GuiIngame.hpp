//
// created by lily on 12/27/25
//

#ifndef MINECRAFTCLIENT_GUIINGAME_HPP
#define MINECRAFTCLIENT_GUIINGAME_HPP
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <vector>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/client/multiplayer/PlayerControllerMP.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/menu/impl/GuiDebugOverlay.hpp"
#include "net/lily/minecpp/menu/impl/GuiInventory.hpp"
#include "net/lily/minecpp/net/packets/play/client/C09PacketHeldItemChange.hpp"
#include "net/lily/minecpp/net/packets/play/client/MissingPlayPackets.hpp"
#include "net/lily/minecpp/net/packets/play/NetHandlerPlay.hpp"
#include "net/lily/minecpp/util/ChatHistory.hpp"
#include "net/lily/minecpp/util/ItemRegistry.hpp"
#include "net/lily/minecpp/util/PlayerList.hpp"
#include "net/lily/minecpp/util/Scoreboard.hpp"
#include "net/lily/minecpp/util/Utf8.hpp"

class GuiIngame final : public Gui {

    std::string chatHistory[256];
    static inline GLuint iconsTexture = 0;
    static inline GLuint widgetsTexture = 0;
    mutable std::unique_ptr<PlayerControllerMP> playerController;
    mutable bool attackHeld = false;
    mutable bool rightMouseHeld = false;
    mutable bool useHeld = false;
    mutable bool useCompletedWhileHeld = false;
    mutable int rightClickDelayTimer = 0;
    mutable bool chatOpen = false;
    // guiIngame.remainingHighlightTicks: the held item's name shows above the hotbar for 40 ticks
    mutable int remainingHighlightTicks = 0;
    mutable int16_t highlightedItemId = -1, highlightedItemDamage = 0;
    // guiIngame.renderPlayerStats state: damage/heal flash and the shake seed
    mutable int64_t healthUpdateCounter = 0;
    mutable int playerHealth = 0, lastPlayerHealth = 0;
    mutable int64_t lastSystemTime = 0;
    mutable bool debugVisible = false;
    mutable bool profilerVisible = false;
    mutable bool frameGraphVisible = false;
    mutable bool playerListVisible = false;
    mutable std::unique_ptr<GuiDebugOverlay> debugOverlay;
    struct MouseButtonEvent {
        int button;
        int action;
        int modifiers;
    };
    mutable std::vector<MouseButtonEvent> pendingMouseButtonEvents;
    mutable std::vector<double> pendingScrollEvents;
    mutable int pendingHotbarSlot = -1;

    void openChat(const std::string& initial = {});

    void renderChat(const bool focused) const {
        if (mc->settings->getInt("chatVisibility") == 2) return;
        std::lock_guard lock(ChatHistory::mutex);
        const int factor = scaleFactor();
        const float guiScale = static_cast<float>(factor);
        const float chatScale = std::max(0.01f, mc->settings->getFloat("chatScale"));
        const float textScale = guiScale * chatScale;
        const float configuredWidth = mc->settings->getFloat("chatWidth") * 280.0f + 40.0f;
        const int maxWidth = static_cast<int>(std::ceil(configuredWidth / chatScale));
        const int maxLines = focused
            ? static_cast<int>((mc->settings->getFloat("chatHeightFocused") * 160.0f + 20.0f) / 9.0f)
            : static_cast<int>((mc->settings->getFloat("chatHeightUnfocused") * 160.0f + 20.0f) / 9.0f);
        const float opacitySetting = mc->settings->getFloat("chatOpacity") * 0.9f + 0.1f;
        const float bottom = static_cast<float>(mc->height()) - 40.0f * guiScale;
        int displayed = 0;

        for (const ChatLine& message : ChatHistory::chatHistory) {
            const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - message.created).count() / 50.0f;
            if (!focused && age >= 200.0f) continue;
            float alpha = 1.0f;
            if (!focused) {
                float fade = 1.0f - age / 200.0f;
                fade = std::clamp(fade * 10.0f, 0.0f, 1.0f);
                alpha = fade * fade;
            }
            alpha *= opacitySetting;

            std::vector<std::vector<ChatRun>> lines(1);
            int lineWidth = 0;
            for (const ChatRun& run : message.component.runs) {
                ChatRun part{"", run.style};
                for (size_t offset = 0; offset < run.text.size();) {
                    const uint32_t codepoint = Utf8::next(run.text, offset);
                    std::string character;
                    Utf8::append(character, codepoint);
                    const int characterWidth = mc->fontRenderer->getStringWidth(character);
                    if (codepoint == '\n' || (lineWidth > 0
                        && lineWidth + characterWidth > maxWidth)) {
                        if (!part.text.empty()) lines.back().push_back(part);
                        lines.emplace_back(); part.text.clear(); lineWidth = 0;
                        if (codepoint == '\n') continue;
                    }
                    part.text += character;
                    lineWidth += characterWidth;
                }
                if (!part.text.empty()) lines.back().push_back(std::move(part));
            }

            for (auto line = lines.rbegin(); line != lines.rend() && displayed < maxLines; ++line, ++displayed) {
                const float y = bottom - static_cast<float>(displayed + 1) * 9.0f * textScale;
                glDisable(GL_DEPTH_TEST);
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                drawRect(2.0f * guiScale, y - guiScale, static_cast<float>(maxWidth + 4) * textScale,
                    9.0f * textScale, {0.0f, 0.0f, 0.0f, 0.5f * alpha});
                float x = 4.0f * guiScale;
                for (const ChatRun& run : *line) {
                    uint32_t color = mc->settings->getBool("chatColors") ? run.style.color : 0xFFFFFFFF;
                    color = (color & 0x00FFFFFFU) | static_cast<uint32_t>(alpha * 255.0f) << 24U;
                    mc->fontRenderer->renderTextStyled(run.text, x, y, textScale, color, true, run.style.italic);
                    if (run.style.bold) mc->fontRenderer->renderTextStyled(run.text, x + textScale, y,
                        textScale, color, false, run.style.italic);
                    const float width = static_cast<float>(mc->fontRenderer->getStringWidth(run.text)) * textScale;
                    const glm::vec4 lineColor = Math::mcColorToVec4(color, false);
                    if (run.style.underlined || run.style.strikethrough) glDisable(GL_DEPTH_TEST);
                    if (run.style.underlined) drawRect(x, y + 8.0f * textScale, width, textScale, lineColor);
                    if (run.style.strikethrough) drawRect(x, y + 4.0f * textScale, width, textScale, lineColor);
                    x += width + (run.style.bold ? textScale : 0.0f);
                }
            }
            if (displayed >= maxLines) break;
        }
    }

    void renderSelectedItemName() const {
        if (remainingHighlightTicks <= 0 || mc->player->gameType == GameType::SPECTATOR) return;
        const NetworkItemStack& held = mc->player->inventory[36 + mc->player->currentHotbarSlot];
        if (!held) return;
        const std::string name = GuiInventory::tooltipText(held.customName.empty()
            ? ItemRegistry::displayName(held.itemId, held.damage) : held.customName);
        const float scale = static_cast<float>(scaleFactor());
        int alpha = remainingHighlightTicks * 256 / 10;
        alpha = std::min(alpha, 255);
        if (alpha <= 0) return;
        const float x = (static_cast<float>(mc->width())
            - static_cast<float>(mc->fontRenderer->getStringWidth(name)) * scale) * 0.5f;
        float y = static_cast<float>(mc->height()) - 59.0f * scale;
        if (mc->player->gameType == GameType::CREATIVE) y += 14.0f * scale; // no health and food rows to clear
        mc->fontRenderer->renderText(name, x, y, scale, 0x00FFFFFFU | static_cast<uint32_t>(alpha) << 24U, true);
    }

    void renderActionBar() const {
        std::lock_guard lock(ChatHistory::mutex);
        if (!ChatHistory::hasActionBar) return;
        const float age = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - ChatHistory::actionBar.created).count() / 50.0f;
        if (age >= 60.0f) { ChatHistory::hasActionBar = false; return; }
        const float alpha = std::clamp((60.0f - age) / 20.0f, 0.0f, 1.0f);
        const float scale = static_cast<float>(scaleFactor());
        float totalWidth = 0.0f;
        for (const ChatRun& run : ChatHistory::actionBar.component.runs)
            totalWidth += static_cast<float>(mc->fontRenderer->getStringWidth(run.text)) * scale;
        float x = (static_cast<float>(mc->width()) - totalWidth) * 0.5f;
        const float y = static_cast<float>(mc->height()) - 68.0f * scale;
        for (const ChatRun& run : ChatHistory::actionBar.component.runs) {
            uint32_t color = mc->settings->getBool("chatColors") ? run.style.color : 0xFFFFFFFF;
            color = (color & 0x00FFFFFFU) | static_cast<uint32_t>(alpha * 255.0f) << 24U;
            mc->fontRenderer->renderText(run.text, x, y, scale, color, true);
            if (run.style.bold) mc->fontRenderer->renderText(run.text, x + scale, y, scale, color, false);
            x += static_cast<float>(mc->fontRenderer->getStringWidth(run.text)) * scale;
        }
    }

    void selectHotbarSlot(const int slot) const {
        if (slot < 0 || slot > 8 || slot == mc->player->currentHotbarSlot) return;
        mc->player->usingItem = false;
        mc->player->itemUseTicks = 0;
        mc->player->currentHotbarSlot = slot;
        playerController->heldItemChanged();
    }

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested
            && mc->width() / (factor + 1) >= 320
            && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    void renderHotbar() const {
        const float scale = static_cast<float>(scaleFactor());
        const float width = static_cast<float>((mc->width() + scaleFactor() - 1) / scaleFactor());
        const float height = static_cast<float>((mc->height() + scaleFactor() - 1) / scaleFactor());
        const float left = width * 0.5f - 91.0f;
        const float top = height - 22.0f;

        drawTexture(widgetsTexture, left * scale, top * scale, 182.0f * scale, 22.0f * scale,
            0.0f, 0.0f, 256.0f, 256.0f, 182.0f, 22.0f);
        drawTexture(widgetsTexture, (left - 1.0f + mc->player->currentHotbarSlot * 20.0f) * scale, (top - 1.0f) * scale,
            24.0f * scale, 22.0f * scale,
            0.0f, 22.0f, 256.0f, 256.0f, 24.0f, 22.0f);
        mc->fontRenderer->shader->use();
        mc->fontRenderer->shader->setMat4("projection", glm::value_ptr(glm::ortho(
            0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        for (int slot = 0; slot < 9; ++slot) {
            GuiInventory::drawStack(mc->player->inventory[36 + slot],
                (left + 3.0f + slot * 20.0f) * scale, (top + 3.0f) * scale, scale);
        }
    }

    void renderCrosshair() const {
        const float scale = static_cast<float>(scaleFactor());
        const float width = static_cast<float>((mc->width() + scaleFactor() - 1) / scaleFactor());
        const float height = static_cast<float>((mc->height() + scaleFactor() - 1) / scaleFactor());

        glBlendFuncSeparate(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_ONE, GL_ZERO);
        drawTexture(iconsTexture, (width * 0.5f - 7.0f) * scale, (height * 0.5f - 7.0f) * scale,
            16.0f * scale, 16.0f * scale,
            0.0f, 0.0f, 256.0f, 256.0f, 16.0f, 16.0f);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void renderPlayerList() const {
        if (!playerListVisible) return;
        std::vector<PlayerListEntry> players = PlayerList::snapshot();
        if (players.empty()) return;
        std::ranges::sort(players, {}, &PlayerListEntry::name);
        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const int rows = std::min<int>(20, players.size());
        const int columns = static_cast<int>((players.size() + rows - 1) / rows);
        int columnWidth = 80;
        for (const PlayerListEntry& player : players)
            columnWidth = std::max(columnWidth, mc->fontRenderer->getStringWidth(player.name) + 24);
        const float totalWidth = static_cast<float>(columns * columnWidth + (columns - 1) * 5);
        const float left = (static_cast<float>(mc->width()) / scale - totalWidth) * 0.5f;
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawRect((left - 1.0f) * scale, 9.0f * scale, (totalWidth + 2.0f) * scale,
            static_cast<float>(rows * 9 + 2) * scale, {0.0f, 0.0f, 0.0f, 0.5f});
        for (size_t index = 0; index < players.size(); ++index) {
            const int column = static_cast<int>(index) / rows;
            const int row = static_cast<int>(index) % rows;
            const float x = (left + column * (columnWidth + 5)) * scale;
            const float y = static_cast<float>(10 + row * 9) * scale;
            drawRect(x, y, static_cast<float>(columnWidth) * scale, 8.0f * scale,
                {1.0f, 1.0f, 1.0f, 0.13f});
        }
        for (size_t index = 0; index < players.size(); ++index) {
            const int column = static_cast<int>(index) / rows;
            const int row = static_cast<int>(index) % rows;
            const float x = (left + column * (columnWidth + 5)) * scale;
            const float y = static_cast<float>(10 + row * 9) * scale;
            mc->fontRenderer->renderText(players[index].name, x + scale, y, scale,
                players[index].gameMode == 3 ? 0xFF909090 : 0xFFFFFFFF, true);
            int pingIcon = 4;
            if (players[index].ping < 0) pingIcon = 5;
            else if (players[index].ping < 150) pingIcon = 0;
            else if (players[index].ping < 300) pingIcon = 1;
            else if (players[index].ping < 600) pingIcon = 2;
            else if (players[index].ping < 1000) pingIcon = 3;
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawTexture(iconsTexture, x + static_cast<float>(columnWidth - 11) * scale, y,
                10.0f * scale, 8.0f * scale, 0.0f, static_cast<float>(176 + pingIcon * 8),
                256.0f, 256.0f, 10.0f, 8.0f);
        }
        mc->fontRenderer->shader->use();
        mc->fontRenderer->shader->setMat4("projection", glm::value_ptr(glm::ortho(0.0f,
            static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
    }

    void renderScoreboard() const {
        const std::optional<ScoreboardSidebar> sidebar = Scoreboard::sidebar(mc->player->username);
        if (!sidebar.has_value() || sidebar->rows.empty()) return;
        // the backgrounds are translucent, and the text pass before this one leaves blending in an unknown state
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const float scaledWidth = static_cast<float>((mc->width() + factor - 1) / factor);
        const float scaledHeight = static_cast<float>((mc->height() + factor - 1) / factor);
        int width = mc->fontRenderer->getStringWidth(sidebar->title);
        for (const auto& row : sidebar->rows) {
            const std::string line = row.name + ": \xC2\xA7" "c" + std::to_string(row.value);
            width = std::max(width, mc->fontRenderer->getStringWidth(line));
        }
        const int contentHeight = static_cast<int>(sidebar->rows.size()) * 9;
        const int bottom = static_cast<int>(scaledHeight) / 2 + contentHeight / 3;
        constexpr int margin = 3;
        const int left = static_cast<int>(scaledWidth) - width - margin;
        int rowIndex = 0;
        for (const auto& row : sidebar->rows) {
            ++rowIndex;
            const std::string value = "\xC2\xA7" "c" + std::to_string(row.value);
            const int y = bottom - rowIndex * 9;
            const int right = static_cast<int>(scaledWidth) - margin + 2;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawRect(static_cast<float>(left - 2) * scale, static_cast<float>(y) * scale,
                static_cast<float>(width + 4) * scale, 9.0f * scale, {0.0f, 0.0f, 0.0f, 80.0f / 255.0f});
            mc->fontRenderer->renderText(row.name, static_cast<float>(left) * scale,
                static_cast<float>(y) * scale, scale, 0xFFFFFFFF, false);
            mc->fontRenderer->renderText(value,
                static_cast<float>(right - mc->fontRenderer->getStringWidth(value)) * scale,
                static_cast<float>(y) * scale, scale, 0xFFFFFFFF, false);
            if (rowIndex != static_cast<int>(sidebar->rows.size())) continue;
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            drawRect(static_cast<float>(left - 2) * scale, static_cast<float>(y - 10) * scale,
                static_cast<float>(width + 4) * scale, 9.0f * scale, {0.0f, 0.0f, 0.0f, 96.0f / 255.0f});
            drawRect(static_cast<float>(left - 2) * scale, static_cast<float>(y - 1) * scale,
                static_cast<float>(width + 4) * scale, scale, {0.0f, 0.0f, 0.0f, 80.0f / 255.0f});
            const float titleX = static_cast<float>(left) + static_cast<float>(width
                - mc->fontRenderer->getStringWidth(sidebar->title)) * 0.5f;
            mc->fontRenderer->renderText(sidebar->title, titleX * scale,
                static_cast<float>(y - 9) * scale, scale, 0xFFFFFFFF, false);
        }
    }

    void renderPlayerStats() const {
        if (mc->player->gameType == GameType::CREATIVE
            || mc->player->gameType == GameType::SPECTATOR) return;

        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const float width = static_cast<float>((mc->width() + factor - 1) / factor);
        const float height = static_cast<float>((mc->height() + factor - 1) / factor);
        const float left = width * 0.5f - 91.0f;
        const float right = width * 0.5f + 91.0f;
        const float top = height - 39.0f;
        // java.util.Random, so shake patterns match vanilla for a given tick
        struct JavaRandom {
            int64_t seed;
            explicit JavaRandom(const int64_t value) : seed((value ^ 0x5DEECE66DLL) & ((1LL << 48) - 1)) {}
            int next(const int bits) {
                seed = (seed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
                return static_cast<int>(seed >> (48 - bits));
            }
            int nextInt(const int bound) {
                if ((bound & -bound) == bound) return static_cast<int>((static_cast<int64_t>(bound) * next(31)) >> 31);
                int bits, value;
                do { bits = next(31); value = bits % bound; } while (bits - value + (bound - 1) < 0);
                return value;
            }
        };
        using Clock = std::chrono::steady_clock;
        static const Clock::time_point start = Clock::now();
        const int64_t systemTime = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
        const int64_t updateCounter = systemTime / 50; // client ticks at 20 Hz

        const int health = static_cast<int>(std::ceil(mc->player->health));
        const bool flash = healthUpdateCounter > updateCounter && (healthUpdateCounter - updateCounter) / 3 % 2 == 1;
        if (health < playerHealth && mc->player->hurtResistantTime > 0) {
            lastSystemTime = systemTime;
            healthUpdateCounter = updateCounter + 20;
        } else if (health > playerHealth && mc->player->hurtResistantTime > 0) {
            lastSystemTime = systemTime;
            healthUpdateCounter = updateCounter + 10;
        }
        if (systemTime - lastSystemTime > 1000) {
            playerHealth = health;
            lastPlayerHealth = health;
            lastSystemTime = systemTime;
        }
        playerHealth = health;
        const int displayedLastHealth = lastPlayerHealth;
        JavaRandom random(updateCounter * 312871);

        const auto draw = [&](const float x, const float y, const int u, const int v) {
            drawTexture(iconsTexture, x * scale, y * scale, 9.0f * scale, 9.0f * scale,
                static_cast<float>(u), static_cast<float>(v), 256.0f, 256.0f, 9.0f, 9.0f);
        };

        // armor: the sum of the worn pieces' defense points
        static constexpr int defense[5][4]{{1, 3, 2, 1}, {2, 5, 4, 1}, {2, 6, 5, 2}, {3, 8, 6, 3}, {2, 5, 3, 1}};
        int armor = 0;
        for (int slot = 0; slot < 4; ++slot) {
            const NetworkItemStack& piece = mc->player->inventory[5 + slot]; // helmet, chest, legs, boots
            if (piece && piece.itemId >= 298 && piece.itemId <= 317 && (piece.itemId - 298) % 4 == slot)
                armor += defense[(piece.itemId - 298) / 4][slot];
        }
        // layout rows: extra heart rows (max health, absorption) push the armor bar up
        const float maxHealth = mc->player->maxHealth;
        const float absorption = mc->player->absorption;
        const int heartRows = static_cast<int>(std::ceil((maxHealth + absorption) / 2.0f / 10.0f));
        const int rowSpacing = std::max(10 - (heartRows - 2), 3);
        const float armorY = top - static_cast<float>((heartRows - 1) * rowSpacing) - 10.0f;
        const auto& effects = mc->player->activeEffects;
        const int regenerating = effects[10]
            ? static_cast<int>(updateCounter % static_cast<int64_t>(std::ceil(maxHealth + 5.0f))) : -1;

        for (int slot = 0; slot < 10 && armor > 0; ++slot) {
            const float x = left + slot * 8.0f;
            if (slot * 2 + 1 < armor) draw(x, armorY, 34, 9);
            else if (slot * 2 + 1 == armor) draw(x, armorY, 25, 9);
            else draw(x, armorY, 16, 9);
        }

        // health, drawn from the last heart to the first so the shake rolls match vanilla
        const int hardcoreRow = mc->player->hardcore ? 5 * 9 : 0;
        float remainingAbsorption = absorption;
        for (int heart = static_cast<int>(std::ceil((maxHealth + absorption) / 2.0f)) - 1; heart >= 0; --heart) {
            int textureX = 16;
            if (effects[19]) textureX += 36;       // poison
            else if (effects[20]) textureX += 72;  // wither
            const int row = static_cast<int>(std::ceil(static_cast<float>(heart + 1) / 10.0f)) - 1;
            const float x = left + static_cast<float>(heart % 10) * 8.0f;
            float y = top - static_cast<float>(row * rowSpacing);
            if (health <= 4) y += static_cast<float>(random.nextInt(2));
            if (heart == regenerating) y -= 2.0f;
            draw(x, y, 16 + (flash ? 9 : 0), hardcoreRow);
            if (flash) {
                if (heart * 2 + 1 < displayedLastHealth) draw(x, y, textureX + 54, hardcoreRow);
                else if (heart * 2 + 1 == displayedLastHealth) draw(x, y, textureX + 63, hardcoreRow);
            }
            if (remainingAbsorption > 0.0f) {
                if (remainingAbsorption == absorption && std::fmod(absorption, 2.0f) == 1.0f)
                    draw(x, y, textureX + 153, hardcoreRow);
                else draw(x, y, textureX + 144, hardcoreRow);
                remainingAbsorption -= 2.0f;
            } else {
                if (heart * 2 + 1 < health) draw(x, y, textureX + 36, hardcoreRow);
                else if (heart * 2 + 1 == health) draw(x, y, textureX + 45, hardcoreRow);
            }
        }

        const std::shared_ptr<Entity> mountEntity = mc->player->ridingEntityId < 0 ? nullptr
            : mc->world->getEntityById(mc->player->ridingEntityId);
        if (mountEntity == nullptr) {
            // food: it shakes while saturation is empty
            const int food = mc->player->foodLevel;
            int textureX = 16, backgroundRow = 0;
            if (effects[17]) { textureX += 36; backgroundRow = 13; } // hunger
            for (int shank = 0; shank < 10; ++shank) {
                float y = top;
                if (mc->player->foodSaturationLevel <= 0.0f && updateCounter % (food * 3 + 1) == 0)
                    y += static_cast<float>(random.nextInt(3) - 1);
                const float x = right - shank * 8.0f - 9.0f;
                draw(x, y, 16 + backgroundRow * 9, 27);
                if (shank * 2 + 1 < food) draw(x, y, textureX + 36, 27);
                else if (shank * 2 + 1 == food) draw(x, y, textureX + 45, 27);
            }
        } else if (mountEntity->getType() == MOB) {
            // mount health replaces the food bar, ten hearts to a row
            const Entity* mount = mountEntity.get();
            const int mountHealth = static_cast<int>(std::ceil(mount->health));
            int remaining = std::min(static_cast<int>(mount->maxHealth + 0.5f) / 2, 30);
            float y = top;
            for (int base = 0; remaining > 0; base += 20) {
                const int count = std::min(remaining, 10);
                remaining -= count;
                for (int icon = 0; icon < count; ++icon) {
                    const float x = right - icon * 8.0f - 9.0f;
                    draw(x, y, 52 + (flash ? 9 : 0), 9);
                    if (icon * 2 + 1 + base < mountHealth) draw(x, y, 52 + 36, 9);
                    else if (icon * 2 + 1 + base == mountHealth) draw(x, y, 52 + 45, 9);
                }
                y -= 10.0f;
            }
        }

        // air bubbles while the eyes are underwater
        const glm::dvec3 eyes = mc->player->getPosEyes(1.0f);
        const Block eyeBlock = mc->world->getBlockAt(static_cast<int>(std::floor(eyes.x)),
            static_cast<int>(std::floor(eyes.y)), static_cast<int>(std::floor(eyes.z)));
        if (blockId(eyeBlock) == blockId(BLOCK_WATER) || blockId(eyeBlock) == blockId(BLOCK_FLOWING_WATER)) {
            const int air = mc->player->air;
            const int full = static_cast<int>(std::ceil(static_cast<double>(air - 2) * 10.0 / 300.0));
            const int popping = static_cast<int>(std::ceil(static_cast<double>(air) * 10.0 / 300.0)) - full;
            for (int bubble = 0; bubble < full + popping; ++bubble)
                draw(right - bubble * 8.0f - 9.0f, armorY, bubble < full ? 16 : 25, 18);
        }
    }

    void renderExperienceBar() const {
        if (mc->player->gameType == GameType::CREATIVE
            || mc->player->gameType == GameType::SPECTATOR) return;

        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const float width = static_cast<float>((mc->width() + factor - 1) / factor);
        const float height = static_cast<float>((mc->height() + factor - 1) / factor);
        const float left = width * 0.5f - 91.0f;
        const float top = height - 29.0f;

        drawTexture(iconsTexture, left * scale, top * scale, 182.0f * scale, 5.0f * scale,
            0.0f, 64.0f, 256.0f, 256.0f, 182.0f, 5.0f);
        const int filledWidth = std::clamp(
            static_cast<int>(mc->player->experience * 183.0f), 0, 182);
        if (filledWidth > 0) {
            drawTexture(iconsTexture, left * scale, top * scale,
                static_cast<float>(filledWidth) * scale, 5.0f * scale,
                0.0f, 69.0f, 256.0f, 256.0f, static_cast<float>(filledWidth), 5.0f);
        }
    }

public:
    void onOpen() override {
        if (iconsTexture == 0) iconsTexture = loadTexture("assets/minecraft/textures/gui/icons.png");
        if (widgetsTexture == 0) widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");
        mc->settings->inputEnabled = true;
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        if (playerController == nullptr) playerController = std::make_unique<PlayerControllerMP>(mc);
    }
    void onClose() override {
        attackHeld = false;
        rightMouseHeld = false;
        rightClickDelayTimer = 0;
        useHeld = false;
        pendingMouseButtonEvents.clear();
        pendingScrollEvents.clear();
        pendingHotbarSlot = -1;
        if (playerController != nullptr) playerController->abortDestroyBlock();
    }

    void update(float deltaSeconds) override {

    }
    void renderHud() const;
    void render() const override;

    bool handleKey(const int key, int, const int action, const int mods) override {
        if (key == GLFW_KEY_TAB) {
            playerListVisible = action != GLFW_RELEASE;
            return true;
        }
        if (action != GLFW_PRESS) return true;
        switch (key) {
            case GLFW_KEY_F3:
                if ((mods & GLFW_MOD_SHIFT) != 0) {
                    profilerVisible = !profilerVisible;
                    debugVisible = true;
                } else if ((mods & GLFW_MOD_ALT) != 0) {
                    frameGraphVisible = !frameGraphVisible;
                    debugVisible = true;
                } else {
                    debugVisible = !debugVisible;
                    if (!debugVisible) {
                        profilerVisible = false;
                        frameGraphVisible = false;
                    }
                }
                break;
            case GLFW_KEY_F5:
                mc->settings->thirdPersonView = (mc->settings->thirdPersonView + 1) % 3;
                break;
            case GLFW_KEY_ESCAPE: mc->openPauseMenu(); break;
            case GLFW_KEY_T: openChat(); break;
            case GLFW_KEY_SLASH: openChat("/"); break;
            case GLFW_KEY_E: mc->displayGuiScreen(new GuiInventory()); break;
            case GLFW_KEY_Q: {
                NetworkItemStack& held = mc->player->inventory[36 + mc->player->currentHotbarSlot];
                if (!held) break;
                const bool all = (mods & GLFW_MOD_CONTROL) != 0;
                if (mc->netClient != nullptr) mc->netClient->sendPacket(C07PacketPlayerDigging{
                    all ? C07PacketPlayerDigging::Action::DropAllItems
                        : C07PacketPlayerDigging::Action::DropItem,
                    {0, 0, 0}, 0});
                if (all || held.count <= 1) held = {};
                else --held.count;
                break;
            }
            case GLFW_KEY_1: pendingHotbarSlot = 0; break;
            case GLFW_KEY_2: pendingHotbarSlot = 1; break;
            case GLFW_KEY_3: pendingHotbarSlot = 2; break;
            case GLFW_KEY_4: pendingHotbarSlot = 3; break;
            case GLFW_KEY_5: pendingHotbarSlot = 4; break;
            case GLFW_KEY_6: pendingHotbarSlot = 5; break;
            case GLFW_KEY_7: pendingHotbarSlot = 6; break;
            case GLFW_KEY_8: pendingHotbarSlot = 7; break;
            case GLFW_KEY_9: pendingHotbarSlot = 8; break;
            default: break;
        }
        return true;
    }
    bool handleMouseButton(const int button, const int action, const int modifiers) override {
        pendingMouseButtonEvents.push_back({button, action, modifiers});
        return true;
    }

    bool processMouseButton(const int button, const int action, int) const {
        if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            switch (action) {
                case GLFW_PRESS: {
                    rightMouseHeld = true;
                    useCompletedWhileHeld = false;
                    // minecraft.rightClickMouse returns immediately while the controller is breaking a block; it must not send C08 while START_DIGGING is active
                    if (playerController->getIsHittingBlock()) break;
                    playerController->rightClick(mc->objectMouseOver);
                    rightClickDelayTimer = 4;
                    const NetworkItemStack& held =
                        mc->player->inventory[36 + mc->player->currentHotbarSlot];
                    useHeld = ItemRegistry::hasUseAction(held.itemId);
                    if (useHeld) {
                        attackHeld = false;
                        mc->player->usingItem = true;
                        mc->player->itemUseTicks = 0;
                        mc->player->itemInUseId = held.itemId;
                        mc->player->itemInUseDamage = held.damage;
                    }
                    break;
                }
                case GLFW_RELEASE:
                    rightMouseHeld = false;
                    if (useHeld) playerController->stopUsingItem();
                    useHeld = false;
                    useCompletedWhileHeld = false;
                    mc->player->usingItem = false;
                    mc->player->itemUseTicks = 0;
                    break;
                default: break;
            }
            return true;
        }
        if (button == GLFW_MOUSE_BUTTON_MIDDLE) {
            if (action == GLFW_PRESS) playerController->pickBlock(mc->objectMouseOver);
            return true;
        }
        if (button != GLFW_MOUSE_BUTTON_LEFT) return true;
        if (mc->player->usingItem) {
            attackHeld = false;
            return true;
        }
        switch (action) {
            case GLFW_PRESS:
                attackHeld = true;
                mc->player->swingItem();
                if (mc->netClient != nullptr) mc->netClient->sendPacket(C0APacketAnimation{});
                if (mc->objectMouseOver.valid
                    && mc->objectMouseOver.typeOfHit == MovingObjectPosition::BLOCK) {
                    playerController->clickBlock(mc->objectMouseOver.blockPos, mc->objectMouseOver.sideHit);
                } else if (mc->objectMouseOver.valid
                    && mc->objectMouseOver.typeOfHit == MovingObjectPosition::ENTITY
                    && mc->objectMouseOver.entityHit != nullptr) {
                    playerController->attackEntity(*mc->objectMouseOver.entityHit);
                }
                break;
            case GLFW_RELEASE:
                attackHeld = false;
                playerController->abortDestroyBlock();
                break;
            default:
                break;
        }
        return true;
    }

    bool handleCursorPos(double x, double y) override {
        return true;
    }

    bool handleScroll(double, const double yOffset) override {
        if (yOffset != 0.0) pendingScrollEvents.push_back(yOffset);
        return true;
    }

    [[nodiscard]] bool blocksWorldUpdate() const override {
        return true;
    }
    [[nodiscard]] bool blocksWorldRender() const override {
        return true;
    }
    [[nodiscard]] bool capturesMouse() const override { return true; }

    void tick() const override {
        if (!mc->world || (mc->netClient != nullptr && !mc->netClient->isHandlerDone())) return;
        mc->input->updateMouse();
        mc->player->prepareMouseLook();
        mc->input->consumeMouseDelta();
        mc->player->getMouseOver(1.0f);
        playerController->syncCurrentPlayItem();

        // gLFW callbacks run between ticks. Vanilla consumes those events at the beginning of minecraft.runTick, before click/use handling and before the player sends its movement
        for (const MouseButtonEvent& event : pendingMouseButtonEvents) {
            processMouseButton(event.button, event.action, event.modifiers);
        }
        pendingMouseButtonEvents.clear();

        for (const double scroll : pendingScrollEvents) {
            if (scroll == 0.0) continue;
            const int direction = scroll > 0.0 ? -1 : 1;
            selectHotbarSlot((mc->player->currentHotbarSlot + direction + 9) % 9);
        }
        pendingScrollEvents.clear();
        if (pendingHotbarSlot >= 0) {
            selectHotbarSlot(pendingHotbarSlot);
            pendingHotbarSlot = -1;
        }

        {
            // guiIngame.updateTick: restart the timer when the held item changes
            const NetworkItemStack& held = mc->player->inventory[36 + mc->player->currentHotbarSlot];
            if (!held) {
                remainingHighlightTicks = 0;
                highlightedItemId = -1;
            } else if (held.itemId != highlightedItemId || held.damage != highlightedItemDamage) {
                remainingHighlightTicks = 40;
            } else if (remainingHighlightTicks > 0) {
                --remainingHighlightTicks;
            }
            highlightedItemId = held ? held.itemId : -1;
            highlightedItemDamage = held ? held.damage : 0;
        }
        // minecraft.runTick handles the mouse (clicks, digging progress, the stop-destroy packet) before the world update, whose player update sends this tick's position packet. Doing it after made every digging packet arrive one movement tick late, which anti-cheat reads as breaking too fast or slow
        if (rightClickDelayTimer > 0) --rightClickDelayTimer;
        if (mc->player->usingItem) {
            const NetworkItemStack& held =
                mc->player->inventory[36 + mc->player->currentHotbarSlot];
            if (!rightMouseHeld || held.itemId != mc->player->itemInUseId
                || held.damage != mc->player->itemInUseDamage) {
                mc->player->usingItem = false;
                mc->player->itemUseTicks = 0;
                useHeld = false;
            } else {
                ++mc->player->itemUseTicks;
                const ItemRegistry::UseAction action = ItemRegistry::useAction(held.itemId);
                if ((action == ItemRegistry::UseAction::EAT
                    || action == ItemRegistry::UseAction::DRINK)
                    && mc->player->itemUseTicks >= 32) {
                    mc->player->usingItem = false;
                    mc->player->itemUseTicks = 0;
                    useHeld = false;
                    useCompletedWhileHeld = true;
                }
            }
        }
        if (rightMouseHeld && rightClickDelayTimer == 0 && !useHeld && !useCompletedWhileHeld
            && !playerController->getIsHittingBlock()) {
            playerController->rightClick(mc->objectMouseOver);
            rightClickDelayTimer = 4;
        }
        if (attackHeld && !mc->player->usingItem) {
            if (mc->objectMouseOver.valid
                && mc->objectMouseOver.typeOfHit == MovingObjectPosition::BLOCK) {
                mc->player->swingItem();
                if (mc->netClient != nullptr) mc->netClient->sendPacket(C0APacketAnimation{});
                playerController->damageBlock(mc->objectMouseOver.blockPos, mc->objectMouseOver.sideHit);
            } else {
                playerController->abortDestroyBlock();
            }
        }
        mc->world->update();
    }

    void setChatOpen(const bool value) const { chatOpen = value; }

};

#endif //mINECRAFTCLIENT_GUIINGAME_HPP
