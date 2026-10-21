#include "GuiIngame.hpp"
#include "GuiChat.hpp"

void GuiIngame::openChat(const std::string& initial) {
    mc->displayGuiScreen(new GuiChat(this, initial));
}

void GuiIngame::renderHud() const {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    renderHotbar();
    renderExperienceBar();
    renderPlayerStats();
    if (!debugVisible) renderCrosshair();

    const FontRenderer* font = mc->fontRenderer;
    font->shader->use();
    font->shader->setMat4("projection", glm::value_ptr(glm::ortho(0.0f,
        static_cast<float>(mc->width()), static_cast<float>(mc->height()), 0.0f)));
    if (mc->player->experienceLevel > 0 && mc->player->gameType != GameType::CREATIVE
        && mc->player->gameType != GameType::SPECTATOR) {
        const int factor = scaleFactor();
        const float scale = static_cast<float>(factor);
        const float width = static_cast<float>((mc->width() + factor - 1) / factor);
        const float height = static_cast<float>((mc->height() + factor - 1) / factor);
        const std::string level = std::to_string(mc->player->experienceLevel);
        const float x = (width * 0.5f - static_cast<float>(font->getStringWidth(level)) * 0.5f) * scale;
        font->renderText(level, x, (height - 35.0f) * scale, scale, 0xFF80FF20, true);
    }

    renderScoreboard();
    if (debugVisible) {
        if (!debugOverlay) debugOverlay = std::make_unique<GuiDebugOverlay>(mc);
        debugOverlay->renderDirections();
        debugOverlay->render(profilerVisible, frameGraphVisible);
    }
    renderChat(chatOpen);
    renderSelectedItemName();
    renderActionBar();
    renderPlayerList();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}

void GuiIngame::render() const {
    using Clock = std::chrono::high_resolution_clock;
    const auto worldStart = Clock::now();
    mc->renderer->render(mc->world);
    mc->frameMetrics.worldMs = std::chrono::duration<double, std::milli>(Clock::now() - worldStart).count();
    renderHud();
}
