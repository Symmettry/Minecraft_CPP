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