#pragma once

#include <optional>
#include <string>

struct AuthSession {
    std::string username;
    std::string uuid;
    std::string accessToken;

    [[nodiscard]] bool canJoinOnlineServer() const;
    [[nodiscard]] static std::optional<AuthSession> load(const std::string& path);
    [[nodiscard]] bool joinServer(const std::string& serverHash, std::string& error) const;
};
