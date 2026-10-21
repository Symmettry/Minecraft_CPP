#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <mutex>
#include <vector>

struct ChatStyle {
    uint32_t color = 0xFFFFFFFF;
    bool bold = false;
    bool italic = false;
    bool underlined = false;
    bool strikethrough = false;
    bool obfuscated = false;
    std::string insertion;
    std::string clickAction;
    std::string clickValue;
    std::string hoverText;
};

struct ChatRun {
    std::string text;
    ChatStyle style;
};

struct ChatComponent {
    std::vector<ChatRun> runs;

    static ChatComponent fromJson(const std::string& json);
    [[nodiscard]] std::string plainText() const;
};

struct ChatLine {
    ChatComponent component;
    std::chrono::steady_clock::time_point created = std::chrono::steady_clock::now();
};

constexpr int MAX_HISTORY = 256;

struct ChatHistory {
    static std::vector<ChatLine> chatHistory;
    static std::vector<std::string> sentMessages;
    static ChatLine actionBar;
    static bool hasActionBar;
    static std::mutex mutex;

    static void addChat(const std::string& json) {
        std::lock_guard lock(mutex);
        chatHistory.insert(chatHistory.begin(), {ChatComponent::fromJson(json)});
        if (chatHistory.size() > MAX_HISTORY) chatHistory.resize(MAX_HISTORY);
    }

    static void addSentMessage(const std::string& message) {
        if (message.empty() || (!sentMessages.empty() && sentMessages.back() == message)) return;
        sentMessages.push_back(message);
        if (sentMessages.size() > MAX_HISTORY) sentMessages.erase(sentMessages.begin());
    }

    static void setActionBar(const std::string& json) {
        std::lock_guard lock(mutex);
        actionBar = {ChatComponent::fromJson(json)};
        hasActionBar = true;
    }
};

struct EnumChatVisibility {
    enum Value { FULL, SYSTEM, HIDDEN } value;

    static Value fromId(const int id) {
        switch (id) {
            case 0: return FULL;
            case 1: return SYSTEM;
            case 2: return HIDDEN;
            default: throw std::runtime_error("Enum Chat Visibility id out of bounds");
        }
    }
    static int toId(const Value value) {
        switch (value) {
            case FULL: return 0;
            case SYSTEM: return 1;
            case HIDDEN: return 2;
            default: throw std::runtime_error("This shouldn't have happened.");
        }
    }
};
