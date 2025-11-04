#pragma once
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

struct AppConfig {
    // Scalars / weights / flags
    int UNLOCKED_PETS = 100;
    int DLs = 0;
    int outputInterval = 5000;
    double EVENT_CURRENCY_WEIGHT = 0.001;
    double FREE_EXP_WEIGHT = 0.00006;
    double PET_STONES_WEIGHT = 0.000045;
    double GROWTH_WEIGHT = 0.00007;
    bool isFullPath = true;
    bool allowSpeedUpgrades = true;
    bool runOptimization = true;

    // Vectors
    std::vector<int> currentLevels = std::vector<int>(21, 0);
    std::vector<double> resourceCounts = {0,500000,0,0,0,0,0,1,1,0};
    std::vector<int> upgradePath;
    std::vector<double> busyTimesStart;
    std::vector<double> busyTimesEnd;

    // Resource names
    std::array<std::string, 10> resourceNames{
        "Tomb","Bat","Ghost","Witch_Book","Witch_Soup",
        "Eye","PET_STONES","FREE_EXP","GROWTH","Black_Cat"
    };
};

inline AppConfig loadConfig(const std::string& path){
    AppConfig cfg;
    std::ifstream f(path);
    if(!f.good()){
        std::cerr << "config.json not found; using defaults.\n";
        return cfg;
    }
    nlohmann::json j; f >> j;

    auto get = [&](auto key, auto& out){
        if (j.contains(key)) out = j[key].get<std::decay_t<decltype(out)>>();
    };

    if (j.contains("UNLOCKED_PETS")) cfg.UNLOCKED_PETS = j["UNLOCKED_PETS"].get<int>();
    if (j.contains("DLs")) cfg.DLs = j["DLs"].get<int>();
    if (j.contains("outputInterval")) cfg.outputInterval = j["outputInterval"].get<int>();

    if (j.contains("EVENT_CURRENCY_WEIGHT")) cfg.EVENT_CURRENCY_WEIGHT = j["EVENT_CURRENCY_WEIGHT"].get<double>();
    if (j.contains("FREE_EXP_WEIGHT")) cfg.FREE_EXP_WEIGHT = j["FREE_EXP_WEIGHT"].get<double>();
    if (j.contains("PET_STONES_WEIGHT")) cfg.PET_STONES_WEIGHT = j["PET_STONES_WEIGHT"].get<double>();
    if (j.contains("GROWTH_WEIGHT")) cfg.GROWTH_WEIGHT = j["GROWTH_WEIGHT"].get<double>();

    if (j.contains("isFullPath")) cfg.isFullPath = j["isFullPath"].get<bool>();
    if (j.contains("allowSpeedUpgrades")) cfg.allowSpeedUpgrades = j["allowSpeedUpgrades"].get<bool>();
    if (j.contains("runOptimization")) cfg.runOptimization = j["runOptimization"].get<bool>();

    if (j.contains("currentLevels") && j["currentLevels"].is_array()) {
        cfg.currentLevels.clear();
        for (auto& it: j["currentLevels"]) cfg.currentLevels.push_back(it.get<int>());
        if (cfg.currentLevels.size() < 21) cfg.currentLevels.resize(21,0);
        if (cfg.currentLevels.size() > 21) cfg.currentLevels.resize(21);
    }
    if (j.contains("resourceCounts") && j["resourceCounts"].is_array()) {
        cfg.resourceCounts.clear();
        for (auto& it: j["resourceCounts"]) cfg.resourceCounts.push_back(it.get<double>());
        if (cfg.resourceCounts.size() < 10) cfg.resourceCounts.resize(10,0.0);
        if (cfg.resourceCounts.size() > 10) cfg.resourceCounts.resize(10);
    }
    if (j.contains("upgradePath") && j["upgradePath"].is_array()) {
        cfg.upgradePath.clear();
        for (auto& it: j["upgradePath"]) cfg.upgradePath.push_back(it.get<int>());
    }
    if (j.contains("busyTimesStart") && j["busyTimesStart"].is_array()) {
        cfg.busyTimesStart.clear();
        for (auto& it: j["busyTimesStart"]) cfg.busyTimesStart.push_back(it.get<double>());
    }
    if (j.contains("busyTimesEnd") && j["busyTimesEnd"].is_array()) {
        cfg.busyTimesEnd.clear();
        for (auto& it: j["busyTimesEnd"]) cfg.busyTimesEnd.push_back(it.get<double>());
    }
    if (j.contains("resourceNames") && j["resourceNames"].is_array()) {
        for (size_t i=0;i<10 && i<j["resourceNames"].size();++i){
            if (j["resourceNames"][i].is_string()) cfg.resourceNames[i] = j["resourceNames"][i].get<std::string>();
        }
    }

    return cfg;
}