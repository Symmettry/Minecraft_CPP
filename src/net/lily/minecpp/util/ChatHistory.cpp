#include "ChatHistory.hpp"
#include "json/JsonValue.hpp"
#include "Language.hpp"
#include "net/lily/minecpp/Minecraft.hpp"

#include <cctype>
#include <unordered_map>

std::vector<ChatLine> ChatHistory::chatHistory{};
std::vector<std::string> ChatHistory::sentMessages{};
ChatLine ChatHistory::actionBar{};
bool ChatHistory::hasActionBar = false;
std::mutex ChatHistory::mutex{};

namespace {
uint32_t namedColor(const std::string& name, const uint32_t fallback) {
    static const std::unordered_map<std::string, uint32_t> colors{
        {"black", 0xFF000000}, {"dark_blue", 0xFF0000AA}, {"dark_green", 0xFF00AA00},
        {"dark_aqua", 0xFF00AAAA}, {"dark_red", 0xFFAA0000}, {"dark_purple", 0xFFAA00AA},
        {"gold", 0xFFFFAA00}, {"gray", 0xFFAAAAAA}, {"dark_gray", 0xFF555555},
        {"blue", 0xFF5555FF}, {"green", 0xFF55FF55}, {"aqua", 0xFF55FFFF},
        {"red", 0xFFFF5555}, {"light_purple", 0xFFFF55FF}, {"yellow", 0xFFFFFF55},
        {"white", 0xFFFFFFFF}, {"reset", 0xFFFFFFFF},
    };
    const auto found = colors.find(name);
    return found == colors.end() ? fallback : found->second;
}

std::string primitiveText(const JsonValue& value) {
    if (value.type == JsonValue::Type::String || value.type == JsonValue::Type::Number) return value.text;
    if (value.type == JsonValue::Type::Boolean) return value.boolean ? "true" : "false";
    return {};
}

void append(const JsonValue& value, const ChatStyle& inherited, std::vector<ChatRun>& output);

std::string translated(const std::string& key, const std::vector<std::string>& arguments) {
    std::string format = key;
    const Minecraft* minecraft = Minecraft::getMinecraft();
    const std::string code = minecraft != nullptr && minecraft->settings != nullptr
        && !minecraft->settings->language.empty() ? minecraft->settings->language : "en_US";
    if (const std::string* entry = Language::find(key, code)) format = *entry;
    size_t sequential = 0;
    for (size_t position = 0; (position = format.find('%', position)) != std::string::npos;) {
        if (position + 1 < format.size() && format[position + 1] == '%') {
            format.erase(position, 1);
            ++position;
            continue;
        }
        size_t index = sequential;
        size_t end = position + 1;
        if (end < format.size() && std::isdigit(static_cast<unsigned char>(format[end]))) {
            index = static_cast<size_t>(format[end++] - '1');
            if (end < format.size() && format[end] == '$') ++end;
        } else ++sequential;
        if (end < format.size() && format[end] == 's') ++end;
        else { ++position; continue; }
        format.replace(position, end - position, index < arguments.size() ? arguments[index] : "");
    }
    return format;
}

void append(const JsonValue& value, const ChatStyle& inherited, std::vector<ChatRun>& output) {
    if (value.type == JsonValue::Type::Array) {
        for (const JsonValue& child : value.array) append(child, inherited, output);
        return;
    }
    if (value.type != JsonValue::Type::Object) {
        if (const std::string text = primitiveText(value); !text.empty()) output.push_back({text, inherited});
        return;
    }

    ChatStyle style = inherited;
    const auto setBool = [&](const char* key, bool& target) {
        if (const auto found = value.object.find(key); found != value.object.end()
            && found->second.type == JsonValue::Type::Boolean) target = found->second.boolean;
    };
    if (const auto found = value.object.find("color"); found != value.object.end())
        style.color = namedColor(primitiveText(found->second), style.color);
    setBool("bold", style.bold); setBool("italic", style.italic);
    setBool("underlined", style.underlined); setBool("strikethrough", style.strikethrough);
    setBool("obfuscated", style.obfuscated);
    if (const auto found = value.object.find("insertion"); found != value.object.end()) style.insertion = primitiveText(found->second);
    if (const auto event = value.object.find("clickEvent"); event != value.object.end() && event->second.type == JsonValue::Type::Object) {
        if (const auto action = event->second.object.find("action"); action != event->second.object.end()) style.clickAction = primitiveText(action->second);
        if (const auto content = event->second.object.find("value"); content != event->second.object.end()) style.clickValue = primitiveText(content->second);
    }
    if (const auto event = value.object.find("hoverEvent"); event != value.object.end() && event->second.type == JsonValue::Type::Object) {
        if (const auto content = event->second.object.find("value"); content != event->second.object.end()) style.hoverText = primitiveText(content->second);
    }

    std::string text;
    if (const auto found = value.object.find("text"); found != value.object.end()) text = primitiveText(found->second);
    else if (const auto found = value.object.find("translate"); found != value.object.end()) {
        std::vector<std::string> arguments;
        if (const auto with = value.object.find("with"); with != value.object.end()) {
            for (const JsonValue& argument : with->second.array) {
                std::vector<ChatRun> temporary; append(argument, style, temporary);
                std::string plain; for (const ChatRun& run : temporary) plain += run.text;
                arguments.push_back(std::move(plain));
            }
        }
        text = translated(primitiveText(found->second), arguments);
    } else if (const auto found = value.object.find("score"); found != value.object.end()
               && found->second.type == JsonValue::Type::Object) {
        if (const auto score = found->second.object.find("value"); score != found->second.object.end()) text = primitiveText(score->second);
    }
    if (!text.empty()) output.push_back({std::move(text), style});
    if (const auto extra = value.object.find("extra"); extra != value.object.end()) append(extra->second, style, output);
}
}

ChatComponent ChatComponent::fromJson(const std::string& json) {
    ChatComponent component;
    const size_t first = json.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return component;
    if (json[first] != '{' && json[first] != '[' && json[first] != '"') {
        component.runs.push_back({json, {}});
        return component;
    }
    try { append(JsonValue::parse(json), {}, component.runs); }
    catch (...) { component.runs.clear(); }
    if (component.runs.empty() && !json.empty()) component.runs.push_back({json, {}});
    return component;
}

std::string ChatComponent::plainText() const {
    std::string result;
    for (const auto&[text, style] : runs) result += text;
    return result;
}
