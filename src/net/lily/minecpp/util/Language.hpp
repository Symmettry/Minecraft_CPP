#pragma once

#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>

// vanilla .lang lookup: en_US is always loaded, then the selected language overrides it
class Language {
public:
    static const std::string* find(const std::string& key, const std::string& languageCode) {
        static std::mutex mutex;
        static std::string loadedCode;
        static std::unordered_map<std::string, std::string> entries;
        std::lock_guard lock(mutex);
        if (entries.empty() || loadedCode != languageCode) {
            entries.clear();
            load("en_US", entries);
            if (languageCode != "en_US") load(languageCode, entries);
            loadedCode = languageCode;
        }
        const auto found = entries.find(key);
        return found == entries.end() ? nullptr : &found->second;
    }

private:
    static void load(const std::string& code, std::unordered_map<std::string, std::string>& entries) {
        std::ifstream file("assets/minecraft/lang/" + code + ".lang");
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            const size_t separator = line.find('=');
            if (separator == std::string::npos) continue;
            entries[line.substr(0, separator)] = line.substr(separator + 1);
        }
    }
};
