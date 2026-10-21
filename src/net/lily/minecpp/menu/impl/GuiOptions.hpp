#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"

class GuiOptions final : public Gui {
    enum class Page { Main, Video, Controls, Language, Chat, ResourcePacks, Snooper, Skin, Sounds, Stream };
    enum class Type { Toggle, Cycle, Slider, PageLink, Action, Keybind };

    struct Option {
        std::string label;
        std::string key;
        Type type;
        std::vector<std::string> values{};
        float minimum = 0.0f;
        float maximum = 1.0f;
        float step = 0.0f;
        Page target = Page::Main;
        KeybindSetting* binding = nullptr;
    };

    struct Bounds { float x; float y; float width; float height; };

    Page page = Page::Main;
    Gui* parent = nullptr;
    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    int awaitingKey = -1;
    int draggedSlider = -1;
    std::string status;
    static inline GLuint widgetsTexture = 0;
    static inline GLuint backgroundTexture = 0;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320 && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    [[nodiscard]] int guiWidth() const { const int factor = scaleFactor(); return (mc->width() + factor - 1) / factor; }
    [[nodiscard]] int guiHeight() const { const int factor = scaleFactor(); return (mc->height() + factor - 1) / factor; }

    [[nodiscard]] std::string title() const {
        switch (page) {
            case Page::Main: return "Options";
            case Page::Video: return "Video Settings";
            case Page::Controls: return "Controls";
            case Page::Language: return "Language";
            case Page::Chat: return "Chat Settings";
            case Page::ResourcePacks: return "Resource Packs";
            case Page::Snooper: return "Snooper Settings";
            case Page::Skin: return "Skin Customization";
            case Page::Sounds: return "Music & Sound Options";
            case Page::Stream: return "Broadcast Settings";
        }
        return "Options";
    }

    static Option toggle(std::string label, std::string key) {
        return {std::move(label), std::move(key), Type::Toggle};
    }
    static Option cycle(std::string label, std::string key, std::vector<std::string> values) {
        return {std::move(label), std::move(key), Type::Cycle, std::move(values)};
    }
    static Option slider(std::string label, std::string key, const float min = 0.0f, const float max = 1.0f, const float step = 0.0f) {
        return {std::move(label), std::move(key), Type::Slider, {}, min, max, step};
    }
    static Option link(std::string label, const Page target) {
        Option result{std::move(label), {}, Type::PageLink}; result.target = target; return result;
    }
    static Option key(std::string label, std::string name, KeybindSetting* binding) {
        Option result{std::move(label), std::move(name), Type::Keybind}; result.binding = binding; return result;
    }

    [[nodiscard]] std::vector<Option> options() const {
        switch (page) {
            case Page::Main:
                return {
                    slider("FOV", "fov"), toggle("Realms Notifications", "realmsNotifications"),
                    link("Skin Customization...", Page::Skin), {"Super Secret Settings...", "superSecretShader", Type::Action},
                    link("Music & Sounds...", Page::Sounds), link("Broadcast Settings...", Page::Stream),
                    link("Video Settings...", Page::Video), link("Controls...", Page::Controls),
                    link("Language...", Page::Language), link("Chat Settings...", Page::Chat),
                    link("Resource Packs...", Page::ResourcePacks), link("Snooper Settings...", Page::Snooper)
                };
            case Page::Video:
                return {
                    cycle("Graphics", "fancyGraphics", {"Fast", "Fancy"}), slider("Render Distance", "renderDistance", 2, 32, 1),
                    cycle("Smooth Lighting", "ambientOcclusion", {"OFF", "Minimum", "Maximum"}), slider("Max Framerate", "maxFps", 10, 260, 10),
                    toggle("3D Anaglyph", "anaglyph3d"), toggle("View Bobbing", "viewBobbing"),
                    cycle("GUI Scale", "guiScale", {"Auto", "Small", "Normal", "Large"}), slider("Brightness", "gamma"),
                    cycle("Clouds", "renderClouds", {"OFF", "Fast", "Fancy"}), cycle("Particles", "particles", {"All", "Decreased", "Minimal"}),
                    toggle("Fullscreen", "fullscreen"), toggle("Use VSync", "enableVsync"),
                    slider("Mipmap Levels", "mipmapLevels", 0, 4, 1), toggle("Use VBOs", "useVbo"),
                    toggle("Alternate Blocks", "blockAlternatives"), toggle("Entity Shadows", "entityShadows")
                };
            case Page::Controls:
                return {
                    toggle("Invert Mouse", "invertYMouse"), slider("Sensitivity", "mouseSensitivity"), toggle("Touchscreen Mode", "touchscreen"),
                    key("Forward", "key_key.forward", &mc->settings->moveForward), key("Left", "key_key.left", &mc->settings->moveLeft),
                    key("Back", "key_key.back", &mc->settings->moveBackward), key("Right", "key_key.right", &mc->settings->moveRight),
                    key("Jump", "key_key.jump", &mc->settings->jump), key("Sneak", "key_key.sneak", &mc->settings->sneak),
                    key("Sprint", "key_key.sprint", &mc->settings->sprint)
                };
            case Page::Language:
                return {cycle("Language", "lang", {"English (US)", "English (UK)", "Deutsch", "Español", "Français", "Italiano", "Português", "Русский", "日本語", "한국어", "简体中文"}), toggle("Force Unicode Font", "forceUnicodeFont")};
            case Page::Chat:
                return {
                    cycle("Chat", "chatVisibility", {"Shown", "Commands Only", "Hidden"}), toggle("Colors", "chatColors"),
                    toggle("Web Links", "chatLinks"), slider("Opacity", "chatOpacity", 0.0f, 1.0f), toggle("Prompt on Links", "chatLinksPrompt"),
                    slider("Scale", "chatScale", 0.0f, 1.0f), slider("Focused Height", "chatHeightFocused"),
                    slider("Unfocused Height", "chatHeightUnfocused"), slider("Width", "chatWidth"), toggle("Reduced Debug Info", "reducedDebugInfo")
                };
            case Page::ResourcePacks:
                return {cycle("Selected Resource Pack", "resourcePacks", {"Default", "Programmer Art"}), {"Open resource pack folder", "openResourcePackFolder", Type::Action}};
            case Page::Snooper:
                return {toggle("Allow Snooper", "snooperEnabled"), {"Client: MineCPP", "", Type::Action}, {"Version: 1.8.9", "", Type::Action}};
            case Page::Skin:
                return {toggle("Cape", "modelPart_0"), toggle("Jacket", "modelPart_1"), toggle("Left Sleeve", "modelPart_2"), toggle("Right Sleeve", "modelPart_3"), toggle("Left Pants Leg", "modelPart_4"), toggle("Right Pants Leg", "modelPart_5"), toggle("Hat", "modelPart_6")};
            case Page::Sounds:
                return {slider("Master Volume", "soundCategory_master"), slider("Music", "soundCategory_music"), slider("Jukebox/Note Blocks", "soundCategory_record"), slider("Weather", "soundCategory_weather"), slider("Blocks", "soundCategory_block"), slider("Hostile Creatures", "soundCategory_hostile"), slider("Friendly Creatures", "soundCategory_neutral"), slider("Players", "soundCategory_player"), slider("Ambient/Environment", "soundCategory_ambient")};
            case Page::Stream:
                return {slider("Stream Quality", "streamBytesPerPixel"), slider("Microphone Volume", "streamMicVolume"), slider("System Volume", "streamSystemVolume"), slider("Bitrate", "streamKbps"), slider("Framerate", "streamFps"), cycle("Compression", "streamCompression", {"Low", "Medium", "High"}), toggle("Send Metadata", "streamSendMetadata"), cycle("Chat", "streamChatEnabled", {"Always", "While Streaming", "Never"}), cycle("User Filter", "streamChatUserFilter", {"All", "Moderators", "Subscribers"}), cycle("Mic Activation", "streamMicToggleBehavior", {"Push to Talk", "Toggle"})};
        }
        return {};
    }

    [[nodiscard]] Bounds optionBounds(const size_t index) const {
        const float center = static_cast<float>(guiWidth()) * 0.5f;
        switch (page) {
            case Page::Main:
                if (index < 2) return {
                    center - 155.0f + static_cast<float>(index) * 160.0f,
                    static_cast<float>(guiHeight()) / 6.0f - 12.0f, 150.0f, 20.0f
                };
                return {center - 155.0f + static_cast<float>((index - 2) % 2) * 160.0f,
                    static_cast<float>(guiHeight()) / 6.0f + 42.0f
                        + static_cast<float>((index - 2) / 2) * 24.0f, 150.0f, 20.0f};
            case Page::Controls:
                if (index < 3) return {
                    center - 155.0f + static_cast<float>(index % 2) * 160.0f,
                    18.0f + static_cast<float>(index / 2) * 24.0f, 150.0f, 20.0f
                };
                return {center - 50.0f, 68.0f + static_cast<float>(index - 3) * 20.0f, 75.0f, 20.0f};
            default:
                return {center - 155.0f + static_cast<float>(index % 2) * 160.0f,
                    58.0f + static_cast<float>(index / 2) * 24.0f, 150.0f, 20.0f};
        }
    }

    [[nodiscard]] Bounds doneBounds() const {
        const float center = static_cast<float>(guiWidth()) * 0.5f;
        switch (page) {
            case Page::Main:
                return {center - 100.0f, static_cast<float>(guiHeight()) / 6.0f + 168.0f, 200.0f, 20.0f};
            case Page::Controls:
                return {center - 155.0f, static_cast<float>(guiHeight() - 29), 150.0f, 20.0f};
            default:
                return {center - 100.0f, static_cast<float>(guiHeight() - 29), 200.0f, 20.0f};
        }
    }
    [[nodiscard]] Bounds resetAllBounds() const { return {static_cast<float>(guiWidth()) * 0.5f + 5.0f, static_cast<float>(guiHeight() - 29), 150.0f, 20.0f}; }
    [[nodiscard]] Bounds keyResetBounds(const size_t index) const {
        const Bounds binding = optionBounds(index);
        return {static_cast<float>(guiWidth()) * 0.5f + 35.0f, binding.y, 50.0f, 20.0f};
    }
    [[nodiscard]] bool hovered(const Bounds& b) const { return mouseX >= b.x && mouseX < b.x + b.width && mouseY >= b.y && mouseY < b.y + b.height; }

    void drawDirt(const float top, const float bottom, const float brightness) const {
        const float scale = static_cast<float>(scaleFactor());
        for (int y = static_cast<int>(top); y < static_cast<int>(bottom); y += 32) {
            for (int x = 0; x < guiWidth(); x += 32) {
                const float height = std::min(32.0f, bottom - static_cast<float>(y));
                drawTexture(backgroundTexture, x * scale, y * scale, 32 * scale, height * scale,
                    0, 0, 32, 32, 32, height, {brightness, brightness, brightness, 1.0f});
            }
        }
    }

    void drawSlotBackground(const float top, const float bottom) const {
        drawDirt(top, bottom, 32.0f / 255.0f);
        const float scale = static_cast<float>(scaleFactor());
        for (int strip = 0; strip < 4; ++strip) {
            const float alpha = 1.0f - static_cast<float>(strip) / 4.0f;
            drawRect(0.0f, (top + strip) * scale, static_cast<float>(mc->width()), scale,
                {0.0f, 0.0f, 0.0f, alpha});
            drawRect(0.0f, (bottom - strip - 1.0f) * scale, static_cast<float>(mc->width()), scale,
                {0.0f, 0.0f, 0.0f, alpha});
        }
    }

    static std::string boolText(const bool value) { return value ? "ON" : "OFF"; }
    [[nodiscard]] float normalizedSliderValue(const Option& option) const {
        if (option.key == "fov") return std::clamp((mc->settings->getFOV() - 30.0f) / 80.0f, 0.0f, 1.0f);
        const float value = mc->settings->getFloat(option.key);
        return std::clamp((value - option.minimum) / (option.maximum - option.minimum), 0.0f, 1.0f);
    }

    [[nodiscard]] std::string displayValue(const Option& option, const size_t index) const {
        switch (option.type) {
            case Type::PageLink:
            case Type::Action:
                return option.label;
            case Type::Keybind: {
                if (static_cast<int>(index) == awaitingKey) return "> ??? <";
                const int value = option.binding->key;
                if (const char* name = glfwGetKeyName(value, 0)) return option.label + ": " + std::string(name);
                const std::string special = value == GLFW_KEY_SPACE ? "SPACE" : value == GLFW_KEY_LEFT_SHIFT ? "LSHIFT" : "KEY " + std::to_string(value);
                return option.label + ": " + special;
            }
            case Type::Toggle:
                return option.label + ": " + boolText(mc->settings->getBool(option.key));
            case Type::Cycle: {
                int selected = mc->settings->getInt(option.key);
                if (option.key == "fancyGraphics") selected = mc->settings->getBool(option.key) ? 1 : 0;
                if (option.key == "renderClouds" && mc->settings->getOption(option.key) == "true") selected = 2;
                if (option.key == "lang") {
                    constexpr std::array codes{"en_US", "en_GB", "de_DE", "es_ES", "fr_FR", "it_IT", "pt_PT", "ru_RU", "ja_JP", "ko_KR", "zh_CN"};
                    const auto found = std::find(codes.begin(), codes.end(), mc->settings->language);
                    selected = found == codes.end() ? 0 : static_cast<int>(found - codes.begin());
                }
                selected = std::clamp(selected, 0, static_cast<int>(option.values.size()) - 1);
                return option.label + ": " + option.values[selected];
            }
            case Type::Slider:
                break;
        }
        const float value = mc->settings->getFloat(option.key);
        if (option.key == "fov") {
            const int fov = static_cast<int>(mc->settings->getFOV());
            return fov == 70 ? "FOV: Normal" : fov >= 110 ? "FOV: Quake Pro" : "FOV: " + std::to_string(fov);
        }
        if (option.key == "mouseSensitivity") {
            const float normalized = normalizedSliderValue(option);
            if (normalized <= 0.0f) return "Sensitivity: *yawn*";
            if (normalized >= 1.0f) return "Sensitivity: HYPERSPEED!!!";
            return "Sensitivity: "
                + std::to_string(static_cast<int>(normalized * 200.0f)) + "%";
        }
        if (option.key == "renderDistance") return "Render Distance: " + std::to_string(static_cast<int>(value)) + " chunks";
        if (option.key.find("soundCategory_") == 0 && value <= 0.001f) return option.label + ": OFF";
        return option.label + ": " + std::to_string(static_cast<int>(std::round(value * (option.maximum <= 1.0f ? 100.0f : 1.0f)))) + (option.maximum <= 1.0f ? "%" : "");
    }

    void drawButtonSurface(const Bounds& bounds, const bool enabled = true) const {
        const float scale = static_cast<float>(scaleFactor());
        const bool highlight = enabled && hovered(bounds);
        const float textureY = !enabled ? 46.0f : highlight ? 86.0f : 66.0f;
        const float half = bounds.width * 0.5f;
        drawTexture(widgetsTexture, bounds.x * scale, bounds.y * scale, half * scale, bounds.height * scale, 0, textureY, 256, 256, half, bounds.height);
        drawTexture(widgetsTexture, (bounds.x + half) * scale, bounds.y * scale, half * scale, bounds.height * scale, 200 - half, textureY, 256, 256, half, bounds.height);
    }

    void drawButtonText(const Bounds& bounds, const std::string& text, const bool enabled = true) const {
        const float scale = static_cast<float>(scaleFactor());
        const bool highlight = enabled && hovered(bounds);
        const float textWidth = static_cast<float>(mc->fontRenderer->getStringWidth(text)) * scale;
        mc->fontRenderer->renderText(text, (bounds.x + bounds.width * 0.5f) * scale - textWidth * 0.5f,
            (bounds.y + 6.0f) * scale, scale, !enabled ? 0xFFA0A0A0 : highlight ? 0xFFFFFFA0 : 0xFFE0E0E0, true);
    }

    void drawButton(const Bounds& bounds, const std::string& text, const bool enabled = true) const {
        drawButtonSurface(bounds, enabled);
        drawButtonText(bounds, text, enabled);
    }

    void drawSlider(const Bounds& bounds, const Option& option, const size_t index) const {
        const float scale = static_cast<float>(scaleFactor());
        const float normalized = normalizedSliderValue(option);
        glDisable(GL_DEPTH_TEST);
        // guiOptionSlider#getHoverState always returns 0. Vanilla therefore draws the disabled-looking button strip as the track even though the slider remains enabled and interactive
        drawButtonSurface(bounds, false);
        const float thumbX = bounds.x + static_cast<float>(static_cast<int>(normalized * (bounds.width - 8.0f)));
        drawTexture(widgetsTexture, thumbX * scale, bounds.y * scale,
            4.0f * scale, 20.0f * scale, 0, 66, 256, 256, 4, 20);
        drawTexture(widgetsTexture, (thumbX + 4.0f) * scale, bounds.y * scale,
            4.0f * scale, 20.0f * scale, 196, 66, 256, 256, 4, 20);
        drawButtonText(bounds, displayValue(option, index));
    }

    [[nodiscard]] bool controlsChanged() const {
        return mc->settings->moveForward.key != GLFW_KEY_W || mc->settings->moveBackward.key != GLFW_KEY_S
            || mc->settings->moveLeft.key != GLFW_KEY_A || mc->settings->moveRight.key != GLFW_KEY_D
            || mc->settings->jump.key != GLFW_KEY_SPACE || mc->settings->sneak.key != GLFW_KEY_LEFT_SHIFT
            || mc->settings->sprint.key != GLFW_KEY_R;
    }

    static int defaultKey(const std::string& keyName) {
        if (keyName == "key_key.forward") return GLFW_KEY_W;
        if (keyName == "key_key.back") return GLFW_KEY_S;
        if (keyName == "key_key.left") return GLFW_KEY_A;
        if (keyName == "key_key.right") return GLFW_KEY_D;
        if (keyName == "key_key.jump") return GLFW_KEY_SPACE;
        if (keyName == "key_key.sneak") return GLFW_KEY_LEFT_SHIFT;
        return GLFW_KEY_R;
    }

    void setSlider(const Option& option, const Bounds& bounds) const {
        float normalized = static_cast<float>((mouseX - bounds.x - 4.0) / (bounds.width - 8.0f));
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        float value = option.minimum + normalized * (option.maximum - option.minimum);
        if (option.step > 0.0f) value = std::round(value / option.step) * option.step;
        if (option.key == "fov") mc->settings->setFOV(30.0f + value * 80.0f);
        else if (option.key == "renderDistance") mc->settings->setRenderDistance(static_cast<int>(value));
        mc->settings->setFloat(option.key, value);
    }

    void activate(const Option& option, const size_t index) {
        switch (option.type) {
            case Type::PageLink:
                page = option.target;
                awaitingKey = -1;
                status.clear();
                return;
            case Type::Toggle: {
                const bool value = !mc->settings->getBool(option.key);
                mc->settings->setBool(option.key, value);
                if (option.key.find("modelPart_") == 0) mc->settings->modelParts[std::stoi(option.key.substr(10))] = value;
                return;
            }
            case Type::Cycle: {
                int value = mc->settings->getInt(option.key);
                if (option.key == "fancyGraphics") value = mc->settings->getBool(option.key) ? 1 : 0;
                if (option.key == "renderClouds" && mc->settings->getOption(option.key) == "true") value = 2;
                if (option.key == "lang") {
                    constexpr std::array codes{"en_US", "en_GB", "de_DE", "es_ES", "fr_FR", "it_IT", "pt_PT", "ru_RU", "ja_JP", "ko_KR", "zh_CN"};
                    const auto found = std::find(codes.begin(), codes.end(), mc->settings->language);
                    value = found == codes.end() ? 0 : static_cast<int>(found - codes.begin());
                }
                value = (value + 1) % static_cast<int>(option.values.size());
                if (option.key == "fancyGraphics") mc->settings->setBool(option.key, value != 0);
                else if (option.key == "renderClouds") mc->settings->setOption(option.key, std::to_string(value));
                else if (option.key == "guiScale") { mc->settings->guiScale = value; mc->settings->setOption(option.key, std::to_string(value)); }
                else if (option.key == "lang") {
                    constexpr std::array codes{"en_US", "en_GB", "de_DE", "es_ES", "fr_FR", "it_IT", "pt_PT", "ru_RU", "ja_JP", "ko_KR", "zh_CN"};
                    mc->settings->language = codes[value]; mc->settings->setOption(option.key, codes[value]);
                } else mc->settings->setOption(option.key, std::to_string(value));
                return;
            }
            case Type::Keybind:
                awaitingKey = static_cast<int>(index);
                return;
            case Type::Slider:
                return;
            case Type::Action:
                break;
        }
        if (option.key == "superSecretShader") {
            mc->settings->setOption(option.key, std::to_string((mc->settings->getInt(option.key) + 1) % 18));
            status = "Super secret setting changed!";
        } else if (option.key == "openResourcePackFolder") {
            std::filesystem::create_directories("resourcepacks");
            status = "Resource pack folder: resourcepacks";
        } else if (option.key == "resetKeys") {
            mc->settings->moveForward.key = GLFW_KEY_W; mc->settings->moveBackward.key = GLFW_KEY_S;
            mc->settings->moveLeft.key = GLFW_KEY_A; mc->settings->moveRight.key = GLFW_KEY_D;
            mc->settings->jump.key = GLFW_KEY_SPACE; mc->settings->sneak.key = GLFW_KEY_LEFT_SHIFT;
            mc->settings->sprint.key = GLFW_KEY_R; mc->settings->saveOptions();
            status = "Controls reset";
        }
    }

    void goBack() {
        switch (page) {
            case Page::Main:
                mc->displayGuiScreen(parent);
                return;
            default:
                page = Page::Main;
                awaitingKey = -1;
                return;
        }
    }

public:
    explicit GuiOptions(Gui* parent = nullptr) : parent(parent) {}

    void onOpen() override {
        if (widgetsTexture == 0) widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");
        if (backgroundTexture == 0) backgroundTexture = loadTexture("assets/minecraft/textures/gui/options_background.png");
        for (int i = 0; i < 7; ++i) {
            const std::string name = "modelPart_" + std::to_string(i);
            if (!mc->settings->options.contains(name)) mc->settings->options[name] = "true";
            mc->settings->modelParts[i] = mc->settings->getBool(name);
        }
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    void onClose() override { mc->settings->saveOptions(); }
    void update(float) override {}

    void render() const override {
        const float scale = static_cast<float>(scaleFactor());
        const bool slotPage = page == Page::Controls || page == Page::Language || page == Page::Snooper;
        const bool dirtBackground = mc->world == nullptr || page == Page::ResourcePacks || slotPage;
        if (dirtBackground) {
            Gui::render();
            drawDirt(0.0f, static_cast<float>(guiHeight()), 64.0f / 255.0f);
        } else {
            mc->renderer->render(mc->world);
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            constexpr int strips = 32;
            for (int strip = 0; strip < strips; ++strip) {
                const float top = static_cast<float>(mc->height()) * strip / strips;
                const float bottom = static_cast<float>(mc->height()) * (strip + 1) / strips;
                const float alpha = (192.0f + 16.0f * strip / (strips - 1)) / 255.0f;
                drawRect(0.0f, top, static_cast<float>(mc->width()), bottom - top,
                    {16.0f / 255.0f, 16.0f / 255.0f, 16.0f / 255.0f, alpha});
            }
        }
        mc->fontRenderer->shader->use();
        mc->fontRenderer->shader->setMat4("projection", glm::value_ptr(glm::ortho(0.0f, static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
        const std::string heading = title();
        float headingY = 15.0f;
        switch (page) {
            case Page::Controls: headingY = 8.0f; break;
            default: break;
        }
        mc->fontRenderer->renderText(heading, (guiWidth() * 0.5f - mc->fontRenderer->getStringWidth(heading) * 0.5f) * scale,
            headingY * scale, scale, 0xFFFFFFFF, true);
        const auto entries = options();
        switch (page) {
            case Page::Controls: {
                constexpr float listTop = 63.0f;
                const float listBottom = static_cast<float>(guiHeight() - 32);
                drawSlotBackground(listTop, listBottom);
                break;
            }
            case Page::Language: {
                static constexpr std::array<const char*, 11> languages{
                    "English (US)", "English (UK)", "Deutsch", "Español", "Français",
                    "Italiano", "Português", "Русский", "日本語", "한국어", "简体中文"
                };
                drawSlotBackground(32.0f, static_cast<float>(guiHeight() - 61));
                static constexpr std::array<const char*, 11> codes{
                    "en_US", "en_GB", "de_DE", "es_ES", "fr_FR", "it_IT",
                    "pt_PT", "ru_RU", "ja_JP", "ko_KR", "zh_CN"
                };
                const auto selectedCode = std::find(codes.begin(), codes.end(), mc->settings->language);
                const int selected = selectedCode == codes.end() ? 0
                    : static_cast<int>(selectedCode - codes.begin());
                for (size_t index = 0; index < languages.size(); ++index) {
                    const float y = 36.0f + static_cast<float>(index) * 18.0f;
                    if (static_cast<int>(index) == selected) {
                        drawRect((guiWidth() * 0.5f - 110.0f) * scale, (y - 2.0f) * scale,
                            220.0f * scale, 16.0f * scale, {0.5f, 0.5f, 0.5f, 1.0f});
                        drawRect((guiWidth() * 0.5f - 109.0f) * scale, (y - 1.0f) * scale,
                            218.0f * scale, 14.0f * scale, {0.0f, 0.0f, 0.0f, 1.0f});
                    }
                    const std::string label = languages[index];
                    mc->fontRenderer->renderText(label,
                        (guiWidth() * 0.5f - mc->fontRenderer->getStringWidth(label) * 0.5f) * scale,
                        y * scale, scale, 0xFFFFFFFF, true);
                }
                const std::string warning = "(Language translations may not be 100% accurate)";
                mc->fontRenderer->renderText(warning,
                    (guiWidth() * 0.5f - mc->fontRenderer->getStringWidth(warning) * 0.5f) * scale,
                    (guiHeight() - 56.0f) * scale, scale, 0xFF808080, true);
                drawButton({guiWidth() * 0.5f - 155.0f, guiHeight() - 38.0f, 150.0f, 20.0f},
                    "Force Unicode Font: " + boolText(mc->settings->getBool("forceUnicodeFont")));
                drawButton({guiWidth() * 0.5f + 5.0f, guiHeight() - 38.0f, 150.0f, 20.0f}, "Done");
                return;
            }
            case Page::Snooper: {
                drawSlotBackground(80.0f, static_cast<float>(guiHeight() - 40));
                mc->fontRenderer->renderText("Client", 10.0f * scale, 85.0f * scale, scale, 0xFFFFFFFF, true);
                mc->fontRenderer->renderText("MineCPP", 230.0f * scale, 85.0f * scale, scale, 0xFFFFFFFF, true);
                mc->fontRenderer->renderText("Version", 10.0f * scale, 95.0f * scale, scale, 0xFFFFFFFF, true);
                mc->fontRenderer->renderText("1.8.9", 230.0f * scale, 95.0f * scale, scale, 0xFFFFFFFF, true);
                drawButton({guiWidth() * 0.5f - 152.0f, guiHeight() - 30.0f, 150.0f, 20.0f},
                    "Allow Snooper: " + boolText(mc->settings->getBool("snooperEnabled")));
                drawButton({guiWidth() * 0.5f + 2.0f, guiHeight() - 30.0f, 150.0f, 20.0f}, "Done");
                return;
            }
            default:
                break;
        }
        for (size_t i = 0; i < entries.size(); ++i) {
            const Bounds bounds = optionBounds(i);
            if (page == Page::Controls && i >= 3) {
                mc->fontRenderer->renderText(entries[i].label,
                    (static_cast<float>(guiWidth()) * 0.5f - 155.0f) * scale,
                    (bounds.y + 6.0f) * scale, scale, 0xFFFFFFFF, true);
                const std::string bindingText = static_cast<int>(i) == awaitingKey
                    ? "> ??? <"
                    : displayValue(entries[i], i).substr(entries[i].label.size() + 2);
                drawButton(bounds, bindingText);
                const bool changed = entries[i].binding->key != defaultKey(entries[i].key);
                drawButton(keyResetBounds(i), "Reset", changed);
                continue;
            }
            const bool enabled = !(page == Page::Snooper && i > 0);
            switch (entries[i].type) {
                case Type::Slider:
                    drawSlider(bounds, entries[i], i);
                    break;
                default:
                    drawButton(bounds, displayValue(entries[i], i), enabled);
                    break;
            }
        }
        if (!status.empty()) mc->fontRenderer->renderText(status,
            (guiWidth() * 0.5f - mc->fontRenderer->getStringWidth(status) * 0.5f) * scale,
            (guiHeight() - 43) * scale, scale, 0xFFFFFF55, true);
        drawButton(doneBounds(), "Done");
        if (page == Page::Controls) drawButton(resetAllBounds(), "Reset All", controlsChanged());
    }

    bool handleKey(const int keyCode, int, const int action, int) override {
        if (action != GLFW_PRESS) return false;
        if (awaitingKey >= 0) {
            const auto entries = options();
            if (awaitingKey < static_cast<int>(entries.size()) && entries[awaitingKey].binding) {
                entries[awaitingKey].binding->key = keyCode == GLFW_KEY_ESCAPE ? GLFW_KEY_UNKNOWN : keyCode;
                mc->settings->setOption(entries[awaitingKey].key, std::to_string(entries[awaitingKey].binding->key));
            }
            awaitingKey = -1;
            return true;
        }
        if (keyCode == GLFW_KEY_ESCAPE) { goBack(); return true; }
        return false;
    }
    bool handleMouseButton(const int button, const int action, int) override {
        if (button != GLFW_MOUSE_BUTTON_LEFT) return false;
        switch (action) {
            case GLFW_RELEASE:
                draggedSlider = -1;
                return true;
            case GLFW_PRESS:
                break;
            default:
                return false;
        }
        if (page == Page::Language) {
            static constexpr std::array<const char*, 11> codes{
                "en_US", "en_GB", "de_DE", "es_ES", "fr_FR", "it_IT",
                "pt_PT", "ru_RU", "ja_JP", "ko_KR", "zh_CN"
            };
            const int row = static_cast<int>((mouseY - 34.0) / 18.0);
            if (mouseY >= 34.0 && row >= 0 && row < static_cast<int>(codes.size())) {
                mc->settings->language = codes[row];
                mc->settings->setOption("lang", codes[row]);
                return true;
            }
            const Bounds unicode{guiWidth() * 0.5f - 155.0f, guiHeight() - 38.0f, 150.0f, 20.0f};
            const Bounds done{guiWidth() * 0.5f + 5.0f, guiHeight() - 38.0f, 150.0f, 20.0f};
            if (hovered(unicode)) {
                mc->settings->setBool("forceUnicodeFont", !mc->settings->getBool("forceUnicodeFont"));
                return true;
            }
            if (hovered(done)) { goBack(); return true; }
            return true;
        }
        if (page == Page::Snooper) {
            const Bounds toggleBounds{guiWidth() * 0.5f - 152.0f, guiHeight() - 30.0f, 150.0f, 20.0f};
            const Bounds done{guiWidth() * 0.5f + 2.0f, guiHeight() - 30.0f, 150.0f, 20.0f};
            if (hovered(toggleBounds)) {
                mc->settings->setBool("snooperEnabled", !mc->settings->getBool("snooperEnabled"));
            } else if (hovered(done)) goBack();
            return true;
        }
        if (hovered(doneBounds())) { goBack(); return true; }
        const auto entries = options();
        if (page == Page::Controls && hovered(resetAllBounds()) && controlsChanged()) {
            activate({"Reset All", "resetKeys", Type::Action}, entries.size());
            return true;
        }
        if (page == Page::Controls) for (size_t i = 3; i < entries.size(); ++i) {
            if (!hovered(keyResetBounds(i))) continue;
            entries[i].binding->key = defaultKey(entries[i].key);
            mc->settings->setOption(entries[i].key, std::to_string(entries[i].binding->key));
            return true;
        }
        for (size_t i = 0; i < entries.size(); ++i) if (hovered(optionBounds(i))) {
            switch (entries[i].type) {
                case Type::Slider:
                    draggedSlider = static_cast<int>(i);
                    setSlider(entries[i], optionBounds(i));
                    break;
                default:
                    activate(entries[i], i);
                    break;
            }
            return true;
        }
        return false;
    }
    bool handleCursorPos(const double x, const double y) override {
        int windowWidth = 1, windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        mouseX = x * mc->width() / static_cast<double>(windowWidth) / scaleFactor();
        mouseY = y * mc->height() / static_cast<double>(windowHeight) / scaleFactor();
        if (draggedSlider >= 0) {
            const auto entries = options();
            if (draggedSlider < static_cast<int>(entries.size())) setSlider(entries[draggedSlider], optionBounds(draggedSlider));
        }
        return true;
    }
    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    void tick() const override {}
};
