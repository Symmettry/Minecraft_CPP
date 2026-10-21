#include "Scoreboard.hpp"

#include <algorithm>
#include <cctype>

namespace {
std::string lowercase(std::string value) {
    std::ranges::transform(value, value.begin(), [](const unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}
}

void Scoreboard::clear() {
    std::lock_guard lock(mutex);
    objectives.clear();
    scores.clear();
    teams.clear();
    playerTeams.clear();
    displaySlots.fill(std::string{});
}

void Scoreboard::updateObjective(const std::string& name, const std::string& displayName,
                                 const std::string& renderType, const int action) {
    std::lock_guard lock(mutex);
    switch (action) {
        case 0: objectives[name] = {displayName, renderType}; break;
        case 1:
            objectives.erase(name);
            scores.erase(name);
            for (std::string& slot : displaySlots) if (slot == name) slot.clear();
            break;
        case 2:
            if (const auto found = objectives.find(name); found != objectives.end())
                found->second = {displayName, renderType};
            break;
        default: break;
    }
}

void Scoreboard::updateScore(const std::string& player, const std::string& objective,
                             const int value, const int action) {
    std::lock_guard lock(mutex);
    switch (action) {
        case 0: scores[objective][player] = value; break;
        case 1:
            if (objective.empty()) {
                for (auto& [name, objectiveScores] : scores) objectiveScores.erase(player);
            } else if (const auto found = scores.find(objective); found != scores.end()) {
                found->second.erase(player);
            }
            break;
        default: break;
    }
}

void Scoreboard::setDisplaySlot(const int slot, const std::string& objective) {
    if (slot < 0 || slot >= static_cast<int>(displaySlots.size())) return;
    std::lock_guard lock(mutex);
    displaySlots[slot] = objective;
}

void Scoreboard::updateTeam(const std::string& name, const int action, const ScoreboardTeam& values,
                            const std::vector<std::string>& players) {
    std::lock_guard lock(mutex);
    switch (action) {
        case 0: {
            ScoreboardTeam& team = teams[name] = values;
            for (const std::string& player : players) {
                if (const auto previous = playerTeams.find(player); previous != playerTeams.end()) {
                    if (const auto oldTeam = teams.find(previous->second); oldTeam != teams.end())
                        oldTeam->second.players.erase(player);
                }
                playerTeams[player] = name;
                team.players.insert(player);
            }
            break;
        }
        case 1:
            if (const auto found = teams.find(name); found != teams.end()) {
                for (const std::string& player : found->second.players) playerTeams.erase(player);
                teams.erase(found);
            }
            break;
        case 2:
            if (const auto found = teams.find(name); found != teams.end()) {
                found->second.displayName = values.displayName;
                found->second.prefix = values.prefix;
                found->second.suffix = values.suffix;
                found->second.nameTagVisibility = values.nameTagVisibility;
                found->second.friendlyFlags = values.friendlyFlags;
                found->second.color = values.color;
            }
            break;
        case 3: {
            ScoreboardTeam& team = teams[name];
            for (const std::string& player : players) {
                if (const auto previous = playerTeams.find(player); previous != playerTeams.end()) {
                    if (const auto oldTeam = teams.find(previous->second); oldTeam != teams.end())
                        oldTeam->second.players.erase(player);
                }
                playerTeams[player] = name;
                team.players.insert(player);
            }
            break;
        }
        case 4:
            if (const auto found = teams.find(name); found != teams.end()) {
                for (const std::string& player : players) {
                    found->second.players.erase(player);
                    if (const auto assigned = playerTeams.find(player); assigned != playerTeams.end()
                        && assigned->second == name) playerTeams.erase(assigned);
                }
            }
            break;
        default: break;
    }
}

std::optional<ScoreboardSidebar> Scoreboard::sidebar(const std::string& localPlayer) {
    std::lock_guard lock(mutex);
    std::string objectiveName;
    if (const auto assignment = playerTeams.find(localPlayer); assignment != playerTeams.end()) {
        if (const auto team = teams.find(assignment->second); team != teams.end()
            && team->second.color >= 0 && team->second.color < 16)
            objectiveName = displaySlots[3 + team->second.color];
    }
    if (objectiveName.empty()) objectiveName = displaySlots[1];
    const auto objective = objectives.find(objectiveName);
    if (objective == objectives.end()) return std::nullopt;

    ScoreboardSidebar result;
    result.title = objective->second.displayName;
    if (const auto objectiveScores = scores.find(objectiveName); objectiveScores != scores.end()) {
        result.rows.reserve(objectiveScores->second.size());
        for (const auto& [player, value] : objectiveScores->second) {
            if (player.empty() || player.front() == '#') continue;
            std::string formatted = player;
            if (const auto assignment = playerTeams.find(player); assignment != playerTeams.end()) {
                if (const auto team = teams.find(assignment->second); team != teams.end())
                    formatted = team->second.prefix + player + team->second.suffix;
            }
            result.rows.push_back({std::move(formatted), value});
        }
    }
    std::ranges::sort(result.rows, [](const ScoreboardSidebar::Row& left, const ScoreboardSidebar::Row& right) {
        if (left.value != right.value) return left.value < right.value;
        return lowercase(left.name) > lowercase(right.name);
    });
    if (result.rows.size() > 15) result.rows.erase(result.rows.begin(), result.rows.end() - 15);
    return result;
}
