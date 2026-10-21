#include "AuthSession.hpp"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <sstream>

#include <curl/curl.h>

#include "net/lily/minecpp/util/json/JsonValue.hpp"

namespace {
std::string stringMember(const JsonValue& root, const char* name) {
    const auto found = root.object.find(name);
    if (found == root.object.end() || found->second.type != JsonValue::Type::String) return {};
    return found->second.text;
}

std::string jsonString(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (const char character : value) {
        switch (character) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(character); break;
        }
    }
    escaped.push_back('"');
    return escaped;
}

size_t discardResponse(char*, const size_t size, const size_t count, void*) {
    return size * count;
}
}

bool AuthSession::canJoinOnlineServer() const {
    return !username.empty() && !uuid.empty() && !accessToken.empty();
}

std::optional<AuthSession> AuthSession::load(const std::string& path) {
    std::ifstream input(path);
    if (!input) return std::nullopt;
    std::stringstream contents;
    contents << input.rdbuf();
    try {
        const JsonValue root = JsonValue::parse(contents.str());
        if (root.type != JsonValue::Type::Object) return std::nullopt;
        AuthSession session{stringMember(root, "username"), stringMember(root, "uuid"),
            stringMember(root, "accessToken")};
        session.uuid.erase(std::ranges::remove(session.uuid, '-').begin(), session.uuid.end());
        return session;
    } catch (...) {
        return std::nullopt;
    }
}

bool AuthSession::joinServer(const std::string& serverHash, std::string& error) const {
    if (!canJoinOnlineServer()) {
        error = "Online-mode server requires account.json with username, uuid, and accessToken";
        return false;
    }
    static std::once_flag curlInitialization;
    std::call_once(curlInitialization, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        error = "Unable to initialize HTTPS authentication";
        return false;
    }
    const std::string body = "{\"accessToken\":" + jsonString(accessToken)
        + ",\"selectedProfile\":" + jsonString(uuid)
        + ",\"serverId\":" + jsonString(serverHash) + '}';
    curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_URL,
        "https://sessionserver.mojang.com/session/minecraft/join");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, discardResponse);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MineCPP/1.8.9");
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    if (result == CURLE_OK && status == 204) return true;
    error = result == CURLE_OK
        ? "Session server rejected the account (HTTP " + std::to_string(status) + ')'
        : "Session server request failed: " + std::string(curl_easy_strerror(result));
    return false;
}
