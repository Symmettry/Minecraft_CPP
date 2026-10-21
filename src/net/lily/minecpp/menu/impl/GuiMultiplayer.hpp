#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "net/lily/minecpp/Minecraft.hpp"
#include "net/lily/minecpp/menu/Gui.hpp"
#include "net/lily/minecpp/menu/impl/GuiConnectionScreens.hpp"

class GuiMultiplayer final : public Gui {
    struct ServerData {
        std::string name;
        std::string address;
    };

    struct Bounds {
        float x;
        float y;
        float width;
        float height;
    };

    enum class Mode { List, DirectConnect, AddServer, EditServer };
    enum class Action { Select, Direct, Add, Edit, Delete, Refresh, Cancel, Confirm, ResourcePacks };

    struct Button {
        Bounds bounds;
        const char* label;
        Action action;
        bool enabled;
    };

    mutable std::vector<ServerData> servers;
    mutable std::vector<Button> buttons;
    mutable Mode mode = Mode::List;
    mutable int selected = -1;
    mutable int focusedField = 0;
    mutable double mouseX = -1.0;
    mutable double mouseY = -1.0;
    mutable std::string serverName = "Minecraft Server";
    mutable std::string serverAddress;
    mutable bool resourcePacksEnabled = false;

    static inline GLuint widgetsTexture = 0;
    static inline GLuint backgroundTexture = 0;

    [[nodiscard]] int scaleFactor() const {
        int factor = 1;
        const int requested = mc->settings->guiScale == 0 ? 1000 : mc->settings->guiScale;
        while (factor < requested && mc->width() / (factor + 1) >= 320
               && mc->height() / (factor + 1) >= 240) ++factor;
        return factor;
    }

    [[nodiscard]] int width() const {
        const int factor = scaleFactor();
        return (mc->width() + factor - 1) / factor;
    }

    [[nodiscard]] int height() const {
        const int factor = scaleFactor();
        return (mc->height() + factor - 1) / factor;
    }

    [[nodiscard]] static bool contains(const Bounds& bounds, const double x, const double y) {
        return x >= bounds.x && x < bounds.x + bounds.width
            && y >= bounds.y && y < bounds.y + bounds.height;
    }

    static std::filesystem::path serverListPath() {
        return "servers.txt";
    }

    void loadServers() const {
        servers.clear();
        std::ifstream stream(serverListPath());
        ServerData server;
        while (stream >> std::quoted(server.name) >> std::quoted(server.address)) servers.push_back(server);
    }

    void saveServers() const {
        std::ofstream stream(serverListPath(), std::ios::trunc);
        for (const auto& server : servers) {
            stream << std::quoted(server.name) << ' ' << std::quoted(server.address) << '\n';
        }
    }

    void createButtons() const {
        buttons.clear();
        const float center = static_cast<float>(width()) * 0.5f;
        switch (mode) {
            case Mode::List: {
                const float firstY = static_cast<float>(height() - 52);
                const bool hasSelection = selected >= 0 && selected < static_cast<int>(servers.size());
                buttons = {
                    {{center - 154, firstY, 100, 20}, "Join Server", Action::Select, hasSelection},
                    {{center - 50, firstY, 100, 20}, "Direct Connect", Action::Direct, true},
                    {{center + 54, firstY, 100, 20}, "Add server", Action::Add, true},
                    {{center - 154, firstY + 24, 70, 20}, "Edit", Action::Edit, hasSelection},
                    {{center - 74, firstY + 24, 70, 20}, "Delete", Action::Delete, hasSelection},
                    {{center + 4, firstY + 24, 70, 20}, "Refresh", Action::Refresh, true},
                    {{center + 80, firstY + 24, 75, 20}, "Cancel", Action::Cancel, true}
                };
                return;
            }
            case Mode::DirectConnect:
            case Mode::AddServer:
            case Mode::EditServer:
                break;
        }

        const bool addMode = mode == Mode::AddServer || mode == Mode::EditServer;
        const bool valid = !serverAddress.empty() && (!addMode || !serverName.empty());
        const float firstY = static_cast<float>(height() / 4 + (addMode ? 114 : 108));
        if (addMode) {
            buttons.push_back({{center - 100, static_cast<float>(height() / 4 + 72), 200, 20},
                resourcePacksEnabled ? "Server Resource Packs: Enabled" : "Server Resource Packs: Prompt",
                Action::ResourcePacks, true});
        }
        buttons.push_back({{center - 100, firstY, 200, 20},
            addMode ? (mode == Mode::EditServer ? "Save" : "Add Server") : "Join Server",
            Action::Confirm, valid});
        buttons.push_back({{center - 100, firstY + 24, 200, 20}, "Cancel", Action::Cancel, true});
    }

    static void drawCentered(const FontRenderer* font, const std::string& text,
                             const float centerX, const float y, const float scale,
                             const unsigned int color, const bool shadow = true) {
        font->renderText(text, centerX - font->getStringWidth(text) * scale * 0.5f,
            y, scale, color, shadow);
    }

    void drawBackground() const {
        const float factor = static_cast<float>(scaleFactor());
        for (int y = 0; y < height(); y += 32) {
            for (int x = 0; x < width(); x += 32) {
                drawTexture(backgroundTexture, x * factor, y * factor, 32 * factor, 32 * factor,
                    0, 0, 32, 32, 32, 32, {0.38f, 0.38f, 0.38f, 1.0f});
            }
        }
    }

    void drawButton(const Button& button) const {
        const float factor = static_cast<float>(scaleFactor());
        const bool hovered = button.enabled && contains(button.bounds, mouseX, mouseY);
        const float textureY = !button.enabled ? 46.0f : hovered ? 86.0f : 66.0f;
        const float half = button.bounds.width * 0.5f;
        glDisable(GL_DEPTH_TEST);
        drawTexture(widgetsTexture, button.bounds.x * factor, button.bounds.y * factor,
            half * factor, button.bounds.height * factor, 0, textureY, 256, 256, half, button.bounds.height);
        drawTexture(widgetsTexture, (button.bounds.x + half) * factor, button.bounds.y * factor,
            half * factor, button.bounds.height * factor, 200 - half, textureY, 256, 256, half, button.bounds.height);

        const unsigned int color = !button.enabled ? 0xFFA0A0A0 : hovered ? 0xFFFFFFA0 : 0xFFE0E0E0;
        drawCentered(mc->fontRenderer, button.label,
            (button.bounds.x + button.bounds.width * 0.5f) * factor,
            (button.bounds.y + (button.bounds.height - 8) * 0.5f) * factor, factor, color);
    }

    void drawTextField(const Bounds& bounds, const std::string& value, const bool focused) const {
        const float factor = static_cast<float>(scaleFactor());
        glDisable(GL_DEPTH_TEST);
        drawRect(bounds.x * factor, bounds.y * factor, bounds.width * factor, bounds.height * factor,
            focused ? glm::vec4{0.63f, 0.63f, 0.63f, 1.0f} : glm::vec4{0.38f, 0.38f, 0.38f, 1.0f});
        drawRect((bounds.x + 1) * factor, (bounds.y + 1) * factor,
            (bounds.width - 2) * factor, (bounds.height - 2) * factor, {0.0f, 0.0f, 0.0f, 1.0f});
        mc->fontRenderer->renderText(value, (bounds.x + 4) * factor, (bounds.y + 6) * factor,
            factor, 0xFFE0E0E0, false);
        if (focused && static_cast<int>(glfwGetTime() * 2.0) % 2 == 0) {
            const float cursorX = (bounds.x + 4 + mc->fontRenderer->getStringWidth(value)) * factor;
            drawRect(cursorX, (bounds.y + 5) * factor, factor, 10 * factor, {0.88f, 0.88f, 0.88f, 1.0f});
        }
    }

    void connect(const std::string& address) const {
        std::string host = address;
        uint16_t port = 25565;
        if (const size_t colon = address.rfind(':'); colon != std::string::npos) {
            const std::string portText = address.substr(colon + 1);
            try {
                const unsigned long parsed = std::stoul(portText);
                if (parsed <= 65535) {
                    host = address.substr(0, colon);
                    port = static_cast<uint16_t>(parsed);
                }
            } catch (...) {}
        }
        if (host.empty()) return;
        mc->displayGuiScreen(new GuiConnecting(const_cast<GuiMultiplayer*>(this), host, port));
    }

    void setMode(const Mode newMode) const {
        mode = newMode;
        switch (newMode) {
            case Mode::AddServer:
            case Mode::EditServer:
                focusedField = 0;
                break;
            case Mode::List:
            case Mode::DirectConnect:
                focusedField = 1;
                break;
        }
        createButtons();
    }

    void activate(const Action action) const {
        switch (action) {
            case Action::Select:
                if (selected >= 0 && selected < static_cast<int>(servers.size())) connect(servers[selected].address);
                break;
            case Action::Direct:
                serverAddress.clear();
                setMode(Mode::DirectConnect);
                break;
            case Action::Add:
                serverName = "Minecraft Server";
                serverAddress.clear();
                setMode(Mode::AddServer);
                break;
            case Action::Edit:
                if (selected >= 0 && selected < static_cast<int>(servers.size())) {
                    serverName = servers[selected].name;
                    serverAddress = servers[selected].address;
                    setMode(Mode::EditServer);
                }
                break;
            case Action::Delete:
                if (selected >= 0 && selected < static_cast<int>(servers.size())) {
                    servers.erase(servers.begin() + selected);
                    selected = -1;
                    saveServers();
                    createButtons();
                }
                break;
            case Action::Refresh:
                loadServers();
                selected = -1;
                createButtons();
                break;
            case Action::Cancel:
                if (mode == Mode::List) mc->displayGuiScreen(nullptr);
                else setMode(Mode::List);
                break;
            case Action::Confirm:
                if (mode == Mode::DirectConnect) connect(serverAddress);
                else if (!serverName.empty() && !serverAddress.empty()) {
                    if (mode == Mode::EditServer && selected >= 0) servers[selected] = {serverName, serverAddress};
                    else servers.push_back({serverName, serverAddress});
                    saveServers();
                    setMode(Mode::List);
                }
                break;
            case Action::ResourcePacks:
                resourcePacksEnabled = !resourcePacksEnabled;
                createButtons();
                break;
        }
    }

public:
    void onOpen() override {
        if (widgetsTexture == 0) {
            widgetsTexture = loadTexture("assets/minecraft/textures/gui/widgets.png");
            backgroundTexture = loadTexture("assets/minecraft/textures/gui/options_background.png");
        }
        loadServers();
        createButtons();
        glfwSetInputMode(mc->renderer->window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void onClose() override {}
    void update(float) override {}

    void render() const override {
        Gui::render();
        drawBackground();
        const float factor = static_cast<float>(scaleFactor());
        const float physicalWidth = static_cast<float>(mc->width());
        const float physicalHeight = static_cast<float>(mc->height());
        const FontRenderer* font = mc->fontRenderer;
        font->shader->use();
        font->shader->setMat4("projection", glm::value_ptr(glm::ortho(0.0f, physicalWidth, physicalHeight, 0.0f)));

        switch (mode) {
            case Mode::List:
                drawCentered(font, "Play Multiplayer", width() * factor * 0.5f, 20 * factor, factor, 0xFFFFFFFF);
                glDisable(GL_DEPTH_TEST);
                for (int y = 32; y < height() - 64; y += 32) {
                    const int tileHeight = std::min(32, height() - 64 - y);
                    for (int x = 0; x < width(); x += 32) {
                        const int tileWidth = std::min(32, width() - x);
                        drawTexture(backgroundTexture, x * factor, y * factor,
                            tileWidth * factor, tileHeight * factor, 0, 0, 32, 32,
                            tileWidth, tileHeight, {0.20f, 0.20f, 0.20f, 1.0f});
                    }
                }
                for (size_t i = 0; i < servers.size(); ++i) {
                    const float y = (36.0f + static_cast<float>(i) * 36.0f);
                    if (y + 32 > height() - 64) break;
                    if (selected == static_cast<int>(i)) {
                        drawRect(2 * factor, (y - 2) * factor, (width() - 4) * factor, 36 * factor,
                            {0.45f, 0.45f, 0.45f, 0.75f});
                    }
                    font->renderText(servers[i].name, 36 * factor, y * factor, factor, 0xFFFFFFFF, true);
                    font->renderText(servers[i].address, 36 * factor, (y + 12) * factor, factor, 0xFFAAAAAA, true);
                }
                if (servers.empty()) {
                    drawCentered(font, "Scanning for games on your local network", width() * factor * 0.5f,
                        48 * factor, factor, 0xFFAAAAAA);
                }
                break;
            case Mode::DirectConnect:
                drawCentered(font, "Direct Connect", width() * factor * 0.5f, 20 * factor, factor, 0xFFFFFFFF);
                font->renderText("Server Address", (width() * 0.5f - 100) * factor, 100 * factor,
                    factor, 0xFFA0A0A0, false);
                drawTextField({width() * 0.5f - 100, 116, 200, 20}, serverAddress, focusedField == 1);
                break;
            case Mode::AddServer:
            case Mode::EditServer:
                drawCentered(font, "Edit Server Info", width() * factor * 0.5f, 20 * factor, factor, 0xFFFFFFFF);
                font->renderText("Server Name", (width() * 0.5f - 100) * factor, 53 * factor,
                    factor, 0xFFA0A0A0, false);
                drawTextField({width() * 0.5f - 100, 66, 200, 20}, serverName, focusedField == 0);
                font->renderText("Server Address", (width() * 0.5f - 100) * factor, 94 * factor,
                    factor, 0xFFA0A0A0, false);
                drawTextField({width() * 0.5f - 100, 106, 200, 20}, serverAddress, focusedField == 1);
                break;
        }

        createButtons();
        for (const auto& button : buttons) drawButton(button);
    }

    bool handleKey(const int key, int, const int action, const int mods) override {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) return false;
        if (mode == Mode::List) {
            if (key == GLFW_KEY_ESCAPE) activate(Action::Cancel);
            else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) activate(Action::Select);
            else if (key == GLFW_KEY_F5) activate(Action::Refresh);
            else if (key == GLFW_KEY_UP && selected > 0) --selected;
            else if (key == GLFW_KEY_DOWN && selected + 1 < static_cast<int>(servers.size())) ++selected;
            else return false;
            createButtons();
            return true;
        }

        std::string& field = focusedField == 0 ? serverName : serverAddress;
        if (key == GLFW_KEY_BACKSPACE && !field.empty()) field.pop_back();
        else if (key == GLFW_KEY_TAB && mode != Mode::DirectConnect) focusedField = 1 - focusedField;
        else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) activate(Action::Confirm);
        else if (key == GLFW_KEY_ESCAPE) activate(Action::Cancel);
        else if (key == GLFW_KEY_V && (mods & GLFW_MOD_CONTROL)) {
            if (const char* clipboard = glfwGetClipboardString(mc->renderer->window)) field += clipboard;
        } else return false;
        createButtons();
        return true;
    }

    bool handleChar(const unsigned int codepoint) override {
        if (mode == Mode::List || codepoint < 32 || codepoint > 126) return false;
        std::string& field = focusedField == 0 ? serverName : serverAddress;
        if (field.size() < 128) field.push_back(static_cast<char>(codepoint));
        createButtons();
        return true;
    }

    bool handleMouseButton(const int button, const int action, int) override {
        if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return false;
        if (mode == Mode::List && mouseY >= 32 && mouseY < height() - 64) {
            const int index = static_cast<int>((mouseY - 36) / 36);
            if (index >= 0 && index < static_cast<int>(servers.size())) {
                const bool doubleClick = selected == index && glfwGetTime() - lastClickTime < 0.25;
                selected = index;
                lastClickTime = glfwGetTime();
                createButtons();
                if (doubleClick) activate(Action::Select);
                return true;
            }
        } else if (mode != Mode::List) {
            const bool addMode = mode == Mode::AddServer || mode == Mode::EditServer;
            if (addMode && contains({width() * 0.5f - 100, 66, 200, 20}, mouseX, mouseY)) focusedField = 0;
            if (contains({width() * 0.5f - 100, static_cast<float>(addMode ? 106 : 116), 200, 20}, mouseX, mouseY)) focusedField = 1;
        }
        for (const auto& control : buttons) {
            if (control.enabled && contains(control.bounds, mouseX, mouseY)) {
                activate(control.action);
                return true;
            }
        }
        return false;
    }

    bool handleCursorPos(const double x, const double y) override {
        int windowWidth = 1;
        int windowHeight = 1;
        glfwGetWindowSize(mc->renderer->window, &windowWidth, &windowHeight);
        mouseX = x * mc->width() / windowWidth / scaleFactor();
        mouseY = y * mc->height() / windowHeight / scaleFactor();
        return true;
    }

    [[nodiscard]] bool blocksWorldUpdate() const override { return true; }
    [[nodiscard]] bool blocksWorldRender() const override { return true; }
    void tick() const override {}

private:
    mutable double lastClickTime = 0.0;
};
