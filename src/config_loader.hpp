#pragma once
#include <fstream>
#include <vector>
#include <string>
#include <iostream>
#include <json.hpp>

extern int EVENT_DURATION_DAYS;
extern int EVENT_DURATION_HOURS;
extern int EVENT_DURATION_MINUTES;
extern int EVENT_DURATION_SECONDS;

extern int UNLOCKED_PETS;
extern int DLs;

extern std::vector<int> currentLevels;
extern std::vector<double> resourceCounts;
extern std::vector<int> upgradePath;

extern bool isFullPath;
extern bool allowSpeedUpgrades;
extern bool runOptimization;

extern int outputInterval;
extern double EVENT_CURRENCY_WEIGHT;
extern double FREE_EXP_WEIGHT;
extern double PET_STONES_WEIGHT;
extern double GROWTH_WEIGHT;

extern std::vector<double> busyTimesStart;
extern std::vector<double> busyTimesEnd;

using nlohmann::json;

template <typename T>
static void maybe_set(const json& j, const char* key, T& out) {
    if (j.contains(key) && !j[key].is_null()) out = j[key].get<T>();
}

inline void loadConfigJson(const std::string& path) {
    std::ifstream f(path);
    if (!f) { std::cerr << "No config file found: " << path << "\n"; return; }
    json j;
    try { f >> j; }
    catch (const std::exception& e) {
        std::cerr << "JSON parse error: " << e.what() << "\n"; return;
    }

    maybe_set(j, "EVENT_DURATION_DAYS",    EVENT_DURATION_DAYS);
    maybe_set(j, "EVENT_DURATION_HOURS",   EVENT_DURATION_HOURS);
    maybe_set(j, "EVENT_DURATION_MINUTES", EVENT_DURATION_MINUTES);
    maybe_set(j, "EVENT_DURATION_SECONDS", EVENT_DURATION_SECONDS);
    maybe_set(j, "UNLOCKED_PETS", UNLOCKED_PETS);
    maybe_set(j, "DLs", DLs);
    maybe_set(j, "isFullPath", isFullPath);
    maybe_set(j, "allowSpeedUpgrades", allowSpeedUpgrades);
    maybe_set(j, "runOptimization", runOptimization);
    maybe_set(j, "outputInterval", outputInterval);
    maybe_set(j, "EVENT_CURRENCY_WEIGHT", EVENT_CURRENCY_WEIGHT);
    maybe_set(j, "FREE_EXP_WEIGHT", FREE_EXP_WEIGHT);
    maybe_set(j, "PET_STONES_WEIGHT", PET_STONES_WEIGHT);
    maybe_set(j, "GROWTH_WEIGHT", GROWTH_WEIGHT);

    if (j.contains("currentLevels"))   currentLevels   = j["currentLevels"].get<std::vector<int>>();
    if (j.contains("resourceCounts"))  resourceCounts  = j["resourceCounts"].get<std::vector<double>>();
    if (j.contains("upgradePath"))     upgradePath     = j["upgradePath"].get<std::vector<int>>();
    if (j.contains("busyTimesStart"))  busyTimesStart  = j["busyTimesStart"].get<std::vector<double>>();
    if (j.contains("busyTimesEnd"))    busyTimesEnd    = j["busyTimesEnd"].get<std::vector<double>>();
}
