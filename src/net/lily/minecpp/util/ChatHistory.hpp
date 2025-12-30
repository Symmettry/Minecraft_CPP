#pragma once

#include <chrono>
#include <vector>
#include <string>

constexpr int MAX_HISTORY = 256;

using History = std::vector<std::pair<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>>>;

struct ChatHistory {
    static History chatHistory;

    static void addChat(const std::string& message) {
        chatHistory.emplace_back(message, std::chrono::high_resolution_clock::now());
        while (chatHistory.size() >= MAX_HISTORY) {
            chatHistory.pop_back();
        }
    }
};

struct EnumChatVisibility {
    enum Value {
        FULL,
        SYSTEM,
        HIDDEN,
    } value;

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