#pragma once
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <sstream>
#include <type_traits>
#include "nlohmann/json.hpp"
#include "constants.hpp"

struct AppConfig {
    // Scalars / weights / flags
    int eventDurationDays = 14;
    int eventDurationHours = 0;
    int eventDurationMinutes = 0;
    int eventDurationSeconds = 0;
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
    int maxOptimizationIterations = 20000;

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

inline std::string trimCopy(const std::string& s){
    const auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) {
        return std::string();
    }
    const auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

inline bool parseClockHours(const std::string& text, double& out){
    const std::string trimmed = trimCopy(text);
    const auto colon = trimmed.find(':');
    if (colon == std::string::npos) {
        return false;
    }
    const std::string hoursPart = trimCopy(trimmed.substr(0, colon));
    const std::string minsPart = trimCopy(trimmed.substr(colon + 1));
    if (hoursPart.empty() || minsPart.empty()) {
        return false;
    }
    try {
        size_t idx = 0;
        const int hours = std::stoi(hoursPart, &idx);
        if (idx != hoursPart.size()) {
            return false;
        }
        idx = 0;
        const int mins = std::stoi(minsPart, &idx);
        if (idx != minsPart.size() || mins < 0 || mins >= 60) {
            return false;
        }
        const double base = static_cast<double>(hours);
        const double frac = static_cast<double>(mins) / 60.0;
        out = base >= 0 ? base + frac : base - frac;
        return true;
    } catch (...) {
        return false;
    }
}

inline AppConfig loadConfig(const std::string& path){
    AppConfig cfg;
    std::error_code pathError;
    const std::filesystem::path absolutePath = std::filesystem::absolute(path, pathError);
    const std::string pathLabel = pathError ? path : absolutePath.string();
    std::cerr << "Loading config from: " << pathLabel << "\n";
    std::ifstream f(path);
    if(!f.good()){
        std::cerr << "Config not found at " << pathLabel << "; using defaults.\n";
        return cfg;
    }
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(f);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse config.json: " << e.what() << "\n";
        return cfg;
    }

    auto safeAssign = [&](const char* key, auto& out) {
        const nlohmann::json* it = j.find(key);
        if (!it || it->is_null()) {
            return;
        }
        try {
            using ValueType = std::decay_t<decltype(out)>;
            out = it->template get<ValueType>();
        } catch (const std::exception& e) {
            std::cerr << "Invalid value for '" << key << "': " << e.what() << "\n";
        }
    };

    safeAssign("eventDurationDays", cfg.eventDurationDays);
    safeAssign("eventDurationHours", cfg.eventDurationHours);
    safeAssign("eventDurationMinutes", cfg.eventDurationMinutes);
    safeAssign("eventDurationSeconds", cfg.eventDurationSeconds);
    safeAssign("EVENT_DURATION_DAYS", cfg.eventDurationDays);
    safeAssign("EVENT_DURATION_HOURS", cfg.eventDurationHours);
    safeAssign("EVENT_DURATION_MINUTES", cfg.eventDurationMinutes);
    safeAssign("EVENT_DURATION_SECONDS", cfg.eventDurationSeconds);
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
    safeAssign("maxOptimizationIterations", cfg.maxOptimizationIterations);

    if (const nlohmann::json* logPathIt = j.find("logFilePath")) {
        if (!logPathIt->is_null()) {
            if (logPathIt->is_string()) {
                cfg.logFilePath = logPathIt->get<std::string>();
            } else {
                std::cerr << "Invalid value for 'logFilePath': expected string.\n";
            }
        }
    }

    auto parseIntListString = [&](const std::string& text, std::vector<int>& out) {
        std::string normalized = text;
        for (char& c : normalized) {
            if (c == ',' || c == '[' || c == ']' || c == '{' || c == '}') {
                c = ' ';
            }
        }
        std::istringstream iss(normalized);
        std::string token;
        std::vector<int> values;
        while (iss >> token) {
            try {
                size_t idx = 0;
                int value = std::stoi(token, &idx);
                if (idx != token.size()) {
                    return false;
                }
                values.push_back(value);
            } catch (...) {
                return false;
            }
        }
        out = std::move(values);
        return true;
    };

    auto loadIntArray = [&](const char* key, std::vector<int>& target, size_t expected) {
        const nlohmann::json* node = j.find(key);
        if (!node) {
            return;
        }
        auto typeLabel = [](const nlohmann::json& value) {
            switch (value.type()) {
                case nlohmann::json::value_t::null: return "null";
                case nlohmann::json::value_t::object: return "object";
                case nlohmann::json::value_t::array: return "array";
                case nlohmann::json::value_t::string: return "string";
                case nlohmann::json::value_t::boolean: return "boolean";
                case nlohmann::json::value_t::number_integer: return "integer";
                case nlohmann::json::value_t::number_unsigned: return "unsigned";
                case nlohmann::json::value_t::number_float: return "float";
                case nlohmann::json::value_t::binary: return "binary";
                case nlohmann::json::value_t::discarded: return "discarded";
            }
            return "unknown";
        };
        std::vector<int> temp;
        if (node->is_array()) {
            const auto& arr = node->as_array();
            temp.reserve(arr.size());
            size_t index = 0;
            for (const auto& entry : arr) {
                if (entry.is_number_integer()) {
                    temp.push_back(entry.get<int>());
                } else if (entry.is_string()) {
                    const std::string text = entry.get<std::string>();
                    try {
                        size_t parsed = 0;
                        const int value = std::stoi(text, &parsed);
                        if (parsed != text.size()) {
                            std::cerr << "Invalid element in '" << key << "' at index " << index
                                      << " (type " << entry.type_name()
                                      << "): expected integer.\n";
                            return;
                        }
                        temp.push_back(value);
                    } catch (...) {
                        std::cerr << "Invalid element in '" << key << "' at index " << index
                                  << " (type " << entry.type_name()
                                  << "): expected integer.\n";
                        return;
                    }
                } else {
                    std::cerr << "Invalid element in '" << key << "' at index " << index
                              << " (type " << entry.type_name()
                              << "): expected integer.\n";
                    return;
                }
                ++index;
            }
        } else if (node->is_string()) {
            if (!parseIntListString(node->get<std::string>(), temp)) {
                std::cerr << "Invalid value for '" << key << "': expected CSV integer list.\n";
                return;
            }
        } else {
            std::cerr << "Invalid value for '" << key << "': expected array.\n";
            return;
        }
        if (temp.size() < expected) temp.resize(expected, 0);
        if (temp.size() > expected) temp.resize(expected);
        target = std::move(temp);
    };

    auto loadDoubleArray = [&](const char* key, std::vector<double>& target, size_t expected, double pad = 0.0) {
        const nlohmann::json* node = j.find(key);
        if (!node) {
            return;
        }
        if (!node->is_array()) {
            std::cerr << "Invalid value for '" << key << "': expected array.\n";
            return;
        }
        std::vector<double> temp;
        const auto& arr = node->as_array();
        temp.reserve(arr.size());
        for (const auto& entry : arr) {
            if (entry.is_number()) {
                temp.push_back(entry.get<double>());
                continue;
            }
            if (entry.is_string()) {
                double clockValue = 0.0;
                if (parseClockHours(entry.get<std::string>(), clockValue)) {
                    temp.push_back(clockValue);
                    continue;
                }
            }
            std::cerr << "Invalid element in '" << key << "': expected number or HH:MM string.\n";
            return;
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
    if (cfg.upgradePath.empty()) {
        loadIntArray("previousUpgradePath", cfg.upgradePath, 0);
    }
    loadDoubleArray("busyTimesStart", cfg.busyTimesStart, 0);
    loadDoubleArray("busyTimesEnd", cfg.busyTimesEnd, 0);

    for (size_t i = 0; i < cfg.currentLevels.size(); ++i) {
        if (cfg.currentLevels[i] < 0) {
            cfg.currentLevels[i] = 0;
        }
    }
    for (size_t i = NUM_RESOURCES; i < NUM_RESOURCES * 2 && i < cfg.currentLevels.size(); ++i) {
        if (cfg.currentLevels[i] > SPEED_LEVEL_CAP) {
            std::cerr << "Speed level at index " << (i - NUM_RESOURCES)
                      << " exceeds cap of " << SPEED_LEVEL_CAP << ". Clamping.\n";
            cfg.currentLevels[i] = SPEED_LEVEL_CAP;
        }
    }
    if (cfg.resourceCounts.size() > 9 && cfg.resourceCounts[9] > EVENT_CURRENCY_CAP) {
        std::cerr << "Event currency exceeds cap of " << EVENT_CURRENCY_CAP << ". Clamping.\n";
        cfg.resourceCounts[9] = EVENT_CURRENCY_CAP;
    }

    if (const nlohmann::json* resourcesIt = j.find("resourceNames")) {
        if (!resourcesIt->is_array()) {
            std::cerr << "Invalid value for 'resourceNames': expected array.\n";
        } else {
            const auto& arr = resourcesIt->as_array();
            for (size_t i = 0; i < cfg.resourceNames.size() && i < arr.size(); ++i) {
                const auto& entry = arr[i];
                if (entry.is_string()) {
                    cfg.resourceNames[i] = entry.get<std::string>();
                } else {
                    std::cerr << "Invalid element in 'resourceNames' at index " << i << ": expected string.\n";
                }
            }
        }
    }

    if (cfg.maxOptimizationIterations < 0) {
        std::cerr << "Invalid value for 'maxOptimizationIterations': expected non-negative integer. Clamping to 0.\n";
        cfg.maxOptimizationIterations = 0;
    }

    return cfg;
}
