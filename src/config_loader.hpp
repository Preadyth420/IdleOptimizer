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
    bool logToConsole = true;
    bool logToFile = false;
    bool appendLogFile = false;
    std::string logFilePath = "logs/run_latest.txt";
    bool pauseOnExit = false;

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
    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse config.json: " << e.what() << "\n";
        return cfg;
    }

    auto safeAssign = [&](const char* key, auto& out) {
        auto it = j.find(key);
        if (it == j.end() || it->is_null()) {
            return;
        }
        try {
            out = it->get<std::decay_t<decltype(out)>>();
        } catch (const std::exception& e) {
            std::cerr << "Invalid value for '" << key << "': " << e.what() << "\n";
        }
    };

    safeAssign("UNLOCKED_PETS", cfg.UNLOCKED_PETS);
    safeAssign("DLs", cfg.DLs);
    safeAssign("outputInterval", cfg.outputInterval);
    safeAssign("EVENT_CURRENCY_WEIGHT", cfg.EVENT_CURRENCY_WEIGHT);
    safeAssign("FREE_EXP_WEIGHT", cfg.FREE_EXP_WEIGHT);
    safeAssign("PET_STONES_WEIGHT", cfg.PET_STONES_WEIGHT);
    safeAssign("GROWTH_WEIGHT", cfg.GROWTH_WEIGHT);
    safeAssign("isFullPath", cfg.isFullPath);
    safeAssign("allowSpeedUpgrades", cfg.allowSpeedUpgrades);
    safeAssign("runOptimization", cfg.runOptimization);
    safeAssign("logToConsole", cfg.logToConsole);
    safeAssign("logToFile", cfg.logToFile);
    safeAssign("appendLogFile", cfg.appendLogFile);
    safeAssign("pauseOnExit", cfg.pauseOnExit);

    auto logPathIt = j.find("logFilePath");
    if (logPathIt != j.end() && !logPathIt->is_null()) {
        if (logPathIt->is_string()) {
            cfg.logFilePath = logPathIt->get<std::string>();
        } else {
            std::cerr << "Invalid value for 'logFilePath': expected string.\n";
        }
    }

    auto loadIntArray = [&](const char* key, std::vector<int>& target, size_t expected) {
        auto it = j.find(key);
        if (it == j.end()) {
            return;
        }
        if (!it->is_array()) {
            std::cerr << "Invalid value for '" << key << "': expected array.\n";
            return;
        }
        std::vector<int> temp;
        temp.reserve(it->size());
        for (const auto& entry : *it) {
            if (!entry.is_number_integer()) {
                std::cerr << "Invalid element in '" << key << "': expected integer.\n";
                return;
            }
            temp.push_back(entry.get<int>());
        }
        if (temp.size() < expected) temp.resize(expected, 0);
        if (temp.size() > expected) temp.resize(expected);
        target = std::move(temp);
    };

    auto loadDoubleArray = [&](const char* key, std::vector<double>& target, size_t expected, double pad = 0.0) {
        auto it = j.find(key);
        if (it == j.end()) {
            return;
        }
        if (!it->is_array()) {
            std::cerr << "Invalid value for '" << key << "': expected array.\n";
            return;
        }
        std::vector<double> temp;
        temp.reserve(it->size());
        for (const auto& entry : *it) {
            if (!entry.is_number()) {
                std::cerr << "Invalid element in '" << key << "': expected number.\n";
                return;
            }
            temp.push_back(entry.get<double>());
        }
        if (expected > 0) {
            if (temp.size() < expected) temp.resize(expected, pad);
            if (temp.size() > expected) temp.resize(expected);
        }
        target = std::move(temp);
    };

    loadIntArray("currentLevels", cfg.currentLevels, 21);
    loadDoubleArray("resourceCounts", cfg.resourceCounts, 10, 0.0);
    loadIntArray("upgradePath", cfg.upgradePath, 0);
    loadDoubleArray("busyTimesStart", cfg.busyTimesStart, 0);
    loadDoubleArray("busyTimesEnd", cfg.busyTimesEnd, 0);

    auto resourcesIt = j.find("resourceNames");
    if (resourcesIt != j.end()) {
        if (!resourcesIt->is_array()) {
            std::cerr << "Invalid value for 'resourceNames': expected array.\n";
        } else {
            for (size_t i = 0; i < cfg.resourceNames.size() && i < resourcesIt->size(); ++i) {
                const auto& entry = (*resourcesIt)[i];
                if (entry.is_string()) {
                    cfg.resourceNames[i] = entry.get<std::string>();
                } else {
                    std::cerr << "Invalid element in 'resourceNames' at index " << i << ": expected string.\n";
                }
            }
        }
    }

    return cfg;
}