#pragma once

#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct ScoreboardObjective {
    std::string displayName;
    std::string renderType = "integer";
};

struct ScoreboardTeam {
    std::string displayName;
    std::string prefix;
    std::string suffix;
    std::string nameTagVisibility = "always";
    int friendlyFlags = 0;
    int color = -1;
    std::unordered_set<std::string> players;
};

struct ScoreboardSidebar {
    struct Row { std::string name; int value; };
    std::string title;
    std::vector<Row> rows;
};

class Scoreboard {
public:
    static inline std::mutex mutex;
    static inline std::unordered_map<std::string, ScoreboardObjective> objectives;
    static inline std::unordered_map<std::string, std::unordered_map<std::string, int>> scores;
    static inline std::unordered_map<std::string, ScoreboardTeam> teams;
    static inline std::unordered_map<std::string, std::string> playerTeams;
    static inline std::array<std::string, 19> displaySlots{};

    static void clear();
    static void updateObjective(const std::string& name, const std::string& displayName,
        const std::string& renderType, int action);
    static void updateScore(const std::string& player, const std::string& objective, int value, int action);
    static void setDisplaySlot(int slot, const std::string& objective);
    static void updateTeam(const std::string& name, int action, const ScoreboardTeam& values,
        const std::vector<std::string>& players);
    static std::optional<ScoreboardSidebar> sidebar(const std::string& localPlayer);
};
