#include <iostream>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <array>
#include <unordered_set>
#include <iomanip>
#include <sstream>
#include <limits>
#include <filesystem>
#include <cmath>

#include "constants.hpp"
#include "config_loader.hpp"
using namespace std;
typedef long long ll;

// ======================= EVENT DURATION (runtime) ======================
int eventDurationDays = 14;
int eventDurationHours = 0;
int eventDurationMinutes = 0;
int eventDurationSeconds = 0;

// ======================= GLOBAL SETTINGS (runtime) =====================
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
bool logToConsoleEnabled = true;
bool logToFileEnabled = false;
bool appendToLogFile = false;
string logFilePath = "logs/run_latest.txt";
bool pauseOnExit = false;
int maxOptimizationIterations = 20000;

// END USER SETTINGS (runtime) ------------------------------------------

// =================== PROGRAM SETTINGS ==================================
array<string, NUM_RESOURCES> resourceNames = {
    "Tomb","Bat","Ghost","Witch_Book","Witch_Soup",
    "Eye","PET_STONES","FREE_EXP","GROWTH","Black_Cat"
};
constexpr double INFINITY_VALUE = (1e100);
int totalSeconds = ((14)*24*3600 + 0*3600 + 0*60 + 0);

map<int, string> upgradeNames;
vector<double> timeNeededSeconds;

// Vectors filled from config.json
vector<int> currentLevels(21, 0);     // 10 level, 10 speed, 1 dummy
vector<double> resourceCounts = {0,500000,0,0,0,0,0,1,1,0};
vector<int> upgradePath;
vector<double> busyTimesStart;
vector<double> busyTimesEnd;

// =================== UTILITY FUNCTIONS =================================
bool pathRespectsSpeedCaps(const vector<int>& path, const vector<int>& startingLevels) {
    vector<int> simulatedLevels = startingLevels;
    for (int upgrade : path) {
        if (upgrade < 0) {
            return false;
        }
        if (upgrade == NUM_RESOURCES * 2) {
            continue; // Sentinel "Complete" upgrade is always allowed
        }
        if (upgrade >= NUM_RESOURCES * 2) {
            return false;
        }
        if (upgrade >= static_cast<int>(simulatedLevels.size())) {
            return false;
        }
        if (upgrade >= NUM_RESOURCES) {
            if (simulatedLevels[upgrade] >= SPEED_LEVEL_CAP) {
                return false;
            }
            simulatedLevels[upgrade]++;
        } else {
            simulatedLevels[upgrade]++;
        }
    }
    return true;
}

void pruneCappedSpeedUpgrades(vector<int>& path, const vector<int>& startingLevels) {
    if (path.empty()) {
        return;
    }

    vector<int> sanitized;
    sanitized.reserve(path.size());
    vector<int> simulatedLevels = startingLevels;

    for (int upgrade : path) {
        if (upgrade == NUM_RESOURCES * 2) {
            sanitized.push_back(upgrade);
            break;
        }
        if (upgrade < 0 || upgrade >= NUM_RESOURCES * 2) {
            continue;
        }
        if (upgrade >= static_cast<int>(simulatedLevels.size())) {
            continue;
        }
        if (upgrade >= NUM_RESOURCES) {
            if (simulatedLevels[upgrade] >= SPEED_LEVEL_CAP) {
                continue;
            }
            simulatedLevels[upgrade] = min(SPEED_LEVEL_CAP, simulatedLevels[upgrade] + 1);
        } else {
            simulatedLevels[upgrade]++;
        }
        sanitized.push_back(upgrade);
    }

    if (sanitized.empty() || sanitized.back() != NUM_RESOURCES * 2) {
        sanitized.push_back(NUM_RESOURCES * 2);
    }

    path.swap(sanitized);
}

template <typename T>
void printVector(const vector<T>& x, ostream& out = cout) {
    for (size_t i=0;i<x.size();++i){
        out << x[i];
        if (i+1<x.size()) out << ",";
    }
}
class Logger {
    int interval;
    mutable chrono::steady_clock::time_point lastLogTime;
    ostream* consoleOut;
    mutable ofstream fileOut;
    bool logToFile = false;
    bool logToConsole = true;
public:
    Logger(int outputInterval,
           bool enableConsole,
           const string& logFilePath,
           bool appendToLog)
        : interval(outputInterval),
          lastLogTime(chrono::steady_clock::now()),
          consoleOut(enableConsole ? &cout : nullptr),
          logToFile(!logFilePath.empty()),
          logToConsole(enableConsole) {
        if (logToFile) {
            try {
                std::filesystem::path logPath(logFilePath);
                if (logPath.has_parent_path()) {
                    std::filesystem::create_directories(logPath.parent_path());
                }
            } catch (const std::filesystem::filesystem_error& e) {
                cerr << "Failed to prepare log directory: " << e.what() << "\n";
            }
            ios::openmode mode = ios::out;
            if (appendToLog) {
                mode |= ios::app;
            } else {
                mode |= ios::trunc;
            }
            fileOut.open(logFilePath, mode);
            if (!fileOut.good()) {
                cerr << "Failed to open log file: " << logFilePath << "\n";
                logToFile = false;
            }
        }
    }
    bool isConsoleEnabled() const {
        return logToConsole && consoleOut;
    }
    bool isFileEnabled() const {
        return logToFile && fileOut.good();
    }
    void logLine(const string& message) const {
        if (logToConsole && consoleOut) {
            (*consoleOut) << message;
            consoleOut->flush();
        }
        if (logToFile && fileOut.good()) {
            fileOut << message;
            fileOut.flush();
        }
    }
    void logLineToFileOnly(const string& message) const {
        if (logToFile && fileOut.good()) {
            fileOut << message;
            fileOut.flush();
        }
    }
    void logImprovement(const string& type, vector<int>& path, const double score) const {
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastLogTime).count();
        if (interval <= 0 || elapsed >= interval) {
            ostringstream ss;
            ss << "Improved path (" << type << "): \n{";
            printVector(path, ss);
            ss << "}\nScore: " << score << "\n";
            logLine(ss.str());
            lastLogTime = now;
        }
    }
};
struct SearchContext {
    Logger& logger;
    const vector<double>& resources;
    const vector<int>& levels;
};
struct OptimizationPackage {
    vector<int> path;
    double score;
    mt19937 randomEngine;
    unordered_set<string> deadMoves = {};
};
struct Proposal {
    string type;
    double newScore;
    int indexA = 0;
    int indexB = 0;
    int rotateIndex = 0;
    int upgrade = 0;
    static Proposal Insert(int index, int upgradeType, double score) {
        return Proposal{"Insert", score, index, 0, 0, upgradeType};
    }
    static Proposal Remove(int index,  double score) {
        return Proposal{"Remove", score, index, 0, 0, 0};
    }
    static Proposal Swap(int indexA, int IndexB, double score) {
        return Proposal{"Swap", score, indexA, IndexB, 0, 0};
    }
    static Proposal Rotate(int indexA, int indexB, int rotateIndex, double score) {
        return Proposal{"Rotate", score, indexA, indexB, rotateIndex, 0};
    }
};
void nameUpgrades() {
    for (int i = 0; i < NUM_RESOURCES; i++) {
        upgradeNames[i] = string(resourceNames[i]) + "_Level";
        upgradeNames[i + NUM_RESOURCES] = string(resourceNames[i]) + "_Speed";
    }
    upgradeNames[20] = "Complete";
}
void adjustFullPath(const vector<int>& startingLevels){ // not thread-safe
    vector<int> levels = startingLevels;
    if (levels.size() > 1 && levels[1] > 0) {
        levels[1]--; // Adjust first level because it always starts at 1
    }
    auto it = upgradePath.begin();
    while (it != upgradePath.end()) {
        const int upgrade = *it;
        if (upgrade < 0 || upgrade >= static_cast<int>(levels.size())) {
            ++it;
            continue;
        }
        if (levels[upgrade] > 0) {
            levels[upgrade]--;
            it = upgradePath.erase(it);
        } else {
            ++it;
        }
    }
    if (upgradePath.empty() || upgradePath.back() != NUM_RESOURCES * 2) {
        upgradePath.push_back(NUM_RESOURCES * 2);
    }
}
string formatResultsReport(const vector<int>& path,
                          const vector<int>& simulationLevels,
                          const vector<double>& simulationResources,
                          double finalScore) {
    ostringstream out;
    out << "Upgrade Path: \n{";
    printVector(path, out);
    out << "}\n";
    out << "Final Resource Counts: ";
    printVector(simulationResources, out);
    out << "\n";
    out << "Final Upgrade Levels: ";
    printVector(simulationLevels, out);
    out << "\n";
    out << "Event Currency: " << min(simulationResources[9], EVENT_CURRENCY_CAP) << "\n";
    out << "Free Exp (" << DLs << " DLs): "
        << simulationResources[7] * (500.0 + DLs) / 5.0
        << " (" << simulationResources[7] << " levels * cycles)" << "\n";
    out << "Pet Stones: " << simulationResources[6] << "\n";
    out << "Growth (" << UNLOCKED_PETS << " pets): "
        << simulationResources[8] * UNLOCKED_PETS / 100.0
        << " (" << simulationResources[8] << " levels * cycles)" << "\n";
    out << "Score: " << finalScore << "\n\n";
    return out.str();
}
void preprocessBusyTimes(const vector<double>& startHours, const vector<double>& endHours) {
    if (static_cast<int>(timeNeededSeconds.size()) != totalSeconds) {
        timeNeededSeconds.assign(max(1, totalSeconds), 0.0);
    } else {
        fill(timeNeededSeconds.begin(), timeNeededSeconds.end(), 0.0);
    }
    for (size_t i = 0; i < startHours.size() && i < endHours.size(); ++i) {
        int startSec = static_cast<int>(startHours[i] * 3600.0);
        int endSec = static_cast<int>(endHours[i] * 3600.0);
        startSec = clamp(startSec, 0, max(0, totalSeconds - 1));
        endSec = clamp(endSec, 0, max(0, totalSeconds - 1));
        if (endSec < startSec) {
            swap(startSec, endSec);
        }
        for (int s = startSec; s <= endSec && s < totalSeconds; ++s) {
            timeNeededSeconds[s] = endSec - s;
        }
    }
}
inline double additionalTimeNeeded(double expectedTimeSeconds) {
    int idx = static_cast<int>(expectedTimeSeconds);
    if (idx < 0 || idx >= static_cast<int>(timeNeededSeconds.size())) return 0.0;
    return timeNeededSeconds[idx];
}
vector <int> generateRandomPath(int length = -1) {
    if (length < 0) {
        length = max(1, totalSeconds / 3600);
    }
    vector<int> randomPath = {};
    for (int i = 0; i < length; i++) {
        randomPath.push_back(rand() % NUM_RESOURCES + 10 * (rand() % 2));
    }
    randomPath.push_back(NUM_RESOURCES * 2);
    return randomPath;
}
string formatUpgradeReadout(int upgradeType, const vector<int>& levels, int elapsedSeconds) {
    ostringstream ss;
    const int days = elapsedSeconds / (24 * 3600);
    const int hours = (elapsedSeconds / 3600) % 24;
    const int minutes = (elapsedSeconds / 60) % 60;
    ss << upgradeNames[upgradeType] << " " << levels[upgradeType]
       << " " << days << " days, " << hours << " hours, " << minutes << " minutes";
    return ss.str();
}

inline void clampEventCurrency(vector<double>& resources) {
    if (resources.size() > 9) {
        resources[9] = min(resources[9], EVENT_CURRENCY_CAP);
    }
}

// =================== ALGORITHM FUNCTIONS ===============================
double performUpgrade(vector<int>& levels, vector<double>& resources, int upgradeType, double& remainingTime) {
    if (upgradeType >= NUM_RESOURCES && upgradeType < NUM_RESOURCES * 2) {
        if (levels[upgradeType] >= SPEED_LEVEL_CAP) {
            return 0.0;
        }
    }
    constexpr double cycleTimeMultiplier[10] = {
        1.0/3.0, 1.0, 1.0/3.0, 1.0/3.0, 1.0/3.0,
        1.0/3.0, 1.0/1200.0, 1.0/2500.0, 1.0/1800.0, 1.0/5000.0
    };
    constexpr double speedMultipliers[11] = {
        1.0, 1.25, 1.5625, 1.953125, 2.44140625, 3.0517578125,
        3.814697265625, 4.76837158203125, 5.960464477539063,
        7.450580596923828, 9.313225746154785
    };

    int resourceType = upgradeType % NUM_RESOURCES;
    if (upgradeType == NUM_RESOURCES * 2) {
        resourceType = -1;
    }
    int newLevel = levels[upgradeType] + 1;
    double baseCost = (3.0 * newLevel * newLevel * newLevel + 1.0) * 100.0;
    if (upgradeType >= NUM_RESOURCES) baseCost *= 2.0;

    double cost[10] = {};
    double productionRates[10] = {};

    switch (resourceType) {
        case 0: cost[1] = baseCost * 10; break;
        case 1: cost[1] = baseCost * 0.8; break;
        case 2: cost[1] = baseCost; break;
        case 3: cost[0] = baseCost; break;
        case 4: cost[0] = baseCost; break;
        case 5: cost[1] = baseCost; cost[2] = baseCost; break;
        case 6: cost[0] = baseCost * 0.7; cost[3] = baseCost * 0.5; break;
        case 7: cost[0] = baseCost; cost[4] = baseCost * 3; break;
        case 8: cost[2] = baseCost * 1.2; cost[5] = baseCost; break;
        case 9: cost[3] = baseCost; cost[4] = baseCost; cost[5] = baseCost; break;
    }

    productionRates[0] = levels[0] * cycleTimeMultiplier[0] * speedMultipliers[levels[10]];
    productionRates[1] = levels[1] * cycleTimeMultiplier[1] * speedMultipliers[levels[11]];
    productionRates[2] = levels[2] * cycleTimeMultiplier[2] * speedMultipliers[levels[12]];
    productionRates[3] = levels[3] * cycleTimeMultiplier[3] * speedMultipliers[levels[13]];
    productionRates[4] = levels[4] * cycleTimeMultiplier[4] * speedMultipliers[levels[14]];
    productionRates[5] = levels[5] * cycleTimeMultiplier[5] * speedMultipliers[levels[15]];
    productionRates[6] = levels[6] * cycleTimeMultiplier[6] * speedMultipliers[levels[16]];
    productionRates[7] = levels[7] * cycleTimeMultiplier[7] * speedMultipliers[levels[17]];
    productionRates[8] = levels[8] * cycleTimeMultiplier[8] * speedMultipliers[levels[18]];
    productionRates[9] = levels[9] * cycleTimeMultiplier[9] * speedMultipliers[levels[19]];

    double timeNeeded = 0;
    double timeElapsed = totalSeconds - remainingTime;
    for (int i = 0; i < NUM_RESOURCES; i++) {
        double neededResources = cost[i] - resources[i];
        if (neededResources <= 0) continue;
        if (productionRates[i] == 0) {
            timeNeeded = INFINITY_VALUE;
            break;
        }
        timeNeeded = max(timeNeeded, neededResources / productionRates[i]);
    }
    int busyLookupIndex = static_cast<int>(timeElapsed + timeNeeded);
    if (0 <= busyLookupIndex && busyLookupIndex < totalSeconds){
        timeNeeded += timeNeededSeconds[busyLookupIndex];
    };

    if (timeNeeded >= remainingTime || upgradeType == (2 * NUM_RESOURCES)) {
        timeNeeded = remainingTime;
        for (int i=0;i<NUM_RESOURCES;i++){
            resources[i] += productionRates[i] * timeNeeded;
        }
        clampEventCurrency(resources);
        return timeNeeded;
    }

    for (int i=0;i<NUM_RESOURCES;i++){
        resources[i] += productionRates[i] * timeNeeded - cost[i];
    }
    levels[upgradeType]++;
    clampEventCurrency(resources);
    return timeNeeded;
}
double simulateUpgradePath(vector<int>& path,
                          vector<int>& levels,
                          vector<double>& resources,
                          bool display = false,
                          vector<string>* upgradeLog = nullptr) {
    thread_local double time;
    time = totalSeconds;
    for (auto upgradeType : path) {
        if (time < 1e-3) return 0;
        if (upgradeType >= NUM_RESOURCES && levels[upgradeType] >= SPEED_LEVEL_CAP) {
            continue; // Skip speed upgrades that are already maxed out
        }
        double timeTaken = performUpgrade(levels, resources, upgradeType, time);
        time -= timeTaken;
        if (display) {
            const int elapsedSeconds = static_cast<int>(totalSeconds - time);
            const string line = formatUpgradeReadout(upgradeType, levels, elapsedSeconds);
            cout << line << "\n";
            if (upgradeLog) {
                upgradeLog->push_back(line);
            }
        } else if (upgradeLog) {
            const int elapsedSeconds = static_cast<int>(totalSeconds - time);
            upgradeLog->push_back(formatUpgradeReadout(upgradeType, levels, elapsedSeconds));
        }
    }
    return time;
}
double calculateScore(vector<double>& resources, bool display = false) {
    double score = 0;
    for (int i = 0; i < NUM_RESOURCES; i++) {
        score += resources[i] * 1e-15;
    }
    const double cappedEventCurrency = min(resources[9], EVENT_CURRENCY_CAP);
    score += (cappedEventCurrency + max(0.0, (resources[9] - EVENT_CURRENCY_CAP)) * 0.01) * (EVENT_CURRENCY_WEIGHT);
    score += resources[7] * (FREE_EXP_WEIGHT);     // Free EXP
    score += resources[8] * (GROWTH_WEIGHT);       // Growth
    score += resources[6] * (PET_STONES_WEIGHT);   // Pet Stones
    return score;
}
double evaluatePath(vector<int>& path, const SearchContext& context){
    if (!pathRespectsSpeedCaps(path, context.levels)) {
        return -numeric_limits<double>::infinity();
    }
    thread_local vector<double> testResources = context.resources;
    thread_local vector<int> testLevels = context.levels;
    testResources = context.resources;
    testLevels = context.levels;
    simulateUpgradePath(path, testLevels, testResources);
    return calculateScore(testResources);
}
void calculateFinalPath(vector<int>& path, Logger* logger = nullptr){
    vector<int>     simulationLevels(currentLevels);
    vector<double>  simulationResources(resourceCounts);
    bool displayUpgrades = true;
    if (logger && !logger->isConsoleEnabled()) {
        displayUpgrades = false;
    }
    vector<string> upgradeLines;
    vector<string>* upgradeLogPtr = nullptr;
    if (logger) {
        if (logger->isFileEnabled() || !logger->isConsoleEnabled()) {
            upgradeLogPtr = &upgradeLines;
        }
    }
    simulateUpgradePath(path, simulationLevels, simulationResources, displayUpgrades, upgradeLogPtr);
    if (logger && upgradeLogPtr) {
        for (const string& line : upgradeLines) {
            const string message = line + "\n";
            if (displayUpgrades && logger->isConsoleEnabled()) {
                logger->logLineToFileOnly(message);
            } else {
                logger->logLine(message);
            }
        }
    }
    double simulationScore = calculateScore(simulationResources);
    string report = formatResultsReport(path, simulationLevels, simulationResources, simulationScore);
    if (!logger) {
        cout << report;
    } else {
        if (!logger->isConsoleEnabled()) {
            cout << report;
        }
        logger->logLine(report);
    }
}

// ------------ Moves ------------
bool tryInsertUpgrade(OptimizationPackage& package, SearchContext& context, Proposal* outProposal = nullptr) {
    int pathLength = (int)package.path.size();
    thread_local vector<int> candidatePath;
    uniform_int_distribution<> positionDist(0, pathLength);
    int startPosition = positionDist(package.randomEngine);
    const int maxTypes = (allowSpeedUpgrades ? NUM_RESOURCES * 2 : NUM_RESOURCES);
    for (int i = 0; i < pathLength; i++) {
        candidatePath = package.path;
        int modulatedInsertPosition = (i + startPosition) % pathLength;
        candidatePath.insert(candidatePath.begin() + modulatedInsertPosition, 0);
        int startingUpgradeType = package.randomEngine() % (allowSpeedUpgrades ? NUM_RESOURCES * 2 : NUM_RESOURCES);
        for (int upgradeType = 0; upgradeType < maxTypes; upgradeType++) {
            int modulatedUpgradeType = (upgradeType + startingUpgradeType) % maxTypes;
            candidatePath[modulatedInsertPosition] = modulatedUpgradeType;
            double testScore = evaluatePath(candidatePath, context);
            if (testScore > package.score) {
                if (outProposal) *outProposal = Proposal::Insert(modulatedInsertPosition, modulatedUpgradeType, testScore);
                package.path.insert(package.path.begin() + modulatedInsertPosition, modulatedUpgradeType);
                package.score = testScore;
                context.logger.logImprovement("Insert", package.path, package.score);
                return true;
            }
        }
    }
    package.deadMoves.insert("Insert");
    return false;
}
bool tryRemoveUpgrade(OptimizationPackage& package, SearchContext& context, Proposal* outProposal = nullptr) {
    int pathLength = (int)package.path.size() - 1;
    thread_local vector<int> candidatePath;
    uniform_int_distribution<> swapDist(0, pathLength - 2);
    int startPos = swapDist(package.randomEngine);
    for (int i = 0; i < pathLength; i++) {
        int removePos = (i + startPos) % (pathLength);
        if (!allowSpeedUpgrades && package.path[removePos] >= NUM_RESOURCES) continue;
        candidatePath = package.path;
        candidatePath.erase(candidatePath.begin() + removePos);
        double testScore = evaluatePath(candidatePath, context);
        if (testScore >= package.score) {
            if (outProposal) *outProposal = Proposal::Remove(removePos, testScore);
            package.score = testScore;
            package.path = candidatePath;
            context.logger.logImprovement("Remove", package.path, package.score);
            return true;
        }
    }
    package.deadMoves.insert("Remove");
    return false;
}
bool trySwapUpgrades(OptimizationPackage& package, SearchContext& context, Proposal* outProposal = nullptr) {
    int pathLength = (int)package.path.size() - 1;
    thread_local vector<int> candidatePath;
    candidatePath = package.path;
    double testScore;
    uniform_int_distribution<> swapDist(0, pathLength - 2);
    int startPos = swapDist(package.randomEngine);
    for (int i2 = 0; i2 < pathLength - 1; i2++) {
        for (int j2 = i2 + 1; j2 < pathLength - 1; j2++) {
            int i = (i2 + startPos) % (pathLength - 1);
            int j = (j2 + startPos) % (pathLength - 1);
            if (candidatePath[i] == candidatePath[j]) continue;
            swap(candidatePath[i], candidatePath[j]);
            testScore = evaluatePath(candidatePath, context);
            if (testScore > package.score) {
                if (outProposal) *outProposal = Proposal::Swap(i, j, testScore);
                package.path = candidatePath;
                package.score = testScore;
                context.logger.logImprovement("Swap", package.path, package.score);
                return true;
            }
            swap(candidatePath[i], candidatePath[j]);
        }
    }
    package.deadMoves.insert("Swap");
    return false;
}
bool tryRotateSubsequences(OptimizationPackage& package, SearchContext& context, Proposal* outProposal = nullptr) {
    int pathLength = (int)package.path.size() - 1;
    thread_local vector<int> candidatePath;
    double testScore;
    uniform_int_distribution<> rotateDist(0, pathLength - 3);
    int i = rotateDist(package.randomEngine);
    uniform_int_distribution<> rotateDist2(i+2, pathLength - 1);
    int j = rotateDist2(package.randomEngine);
    for (int k = 0; k < j-i; k++) {
        candidatePath = package.path;
        int offset = (k + 2) / 2;
        bool isLeft = (k % 2 == 0);
        if(isLeft)  rotate(candidatePath.begin() + i, candidatePath.begin() + i + offset, candidatePath.begin() + j + 1);
        else        rotate(candidatePath.begin() + i, candidatePath.begin() + j - offset + 1, candidatePath.begin() + j + 1);
        testScore = evaluatePath(candidatePath, context);
        int rotationPos = isLeft ? i + offset: j - offset + 1;
        if (testScore > package.score) {
            if (outProposal) *outProposal = Proposal::Rotate(i, j + 1, rotationPos, testScore);
            package.path = candidatePath;
            package.score = testScore;
            context.logger.logImprovement("Rotation", package.path, package.score);
            return true;
        }
    }
    return false;
}
bool exhaustRotateSubsequences(OptimizationPackage& package, SearchContext& context, Proposal* outProposal = nullptr) {
    int pathLength = (int)package.path.size() - 1;
    int maxIndex = pathLength - 1;
    thread_local vector<int> candidatePath;
    double testScore = package.score;
    uniform_int_distribution<> rotateDist(0, maxIndex - 2);
    int i = rotateDist(package.randomEngine);
    for (int i2 = 0; i2 < maxIndex - 1; i2++){
        int i3 = (i + i2) % (maxIndex - 1);
        uniform_int_distribution<> rotateDist2(0, maxIndex-i3);
        int j = rotateDist2(package.randomEngine);
        for (int j2 = 0; j2 < maxIndex - i3 - 1; j2++){
            int j3 = i3 + 2 + ((j + j2) % (maxIndex - i3 - 1));
            for (int k = 0; k < j3-i3; k++) {
                candidatePath = package.path;
                int offset = (k + 2) / 2;
                bool isLeft = (k % 2 == 0);
                if(isLeft)  rotate(candidatePath.begin() + i3, candidatePath.begin() + i3 + offset, candidatePath.begin() + j3 + 1);
                else        rotate(candidatePath.begin() + i3, candidatePath.begin() + j3 - offset + 1, candidatePath.begin() + j3 + 1);
                testScore = evaluatePath(candidatePath, context);
                int rotationPos = isLeft ? i3 + offset: j3 - offset + 1;
                if (testScore > package.score) {
                    if (outProposal) *outProposal = Proposal::Rotate(i3, j3+1, rotationPos, testScore);
                    package.path = candidatePath;
                    package.score = testScore;
                    context.logger.logImprovement("Rotation", package.path, package.score);
                    return true;
                }
            }
        }
    }
    package.deadMoves.insert("Rotate");
    return false;
}
void optimizeUpgradePath(OptimizationPackage& package, SearchContext& context, const int maxIterations = 10000) {
    random_device seed;
    mt19937 randomEngine(seed());
    int iterationCount = 0;
    int noImprovementStreak = 0;
    package.score = evaluatePath(package.path, context);
    while (noImprovementStreak < maxIterations) {
        iterationCount++;
        bool improved = false;
        int strategy = iterationCount % 100;
        if(package.deadMoves.count("Rotation")){
            break;
        }
        else if (package.deadMoves.count("Insert") && package.deadMoves.count("Remove") && package.deadMoves.count("Swap")) {
            improved = tryRotateSubsequences(package, context);
        }
        else if (strategy < 15 && !package.deadMoves.count("Insert")) {
            improved = tryInsertUpgrade(package, context);
        }
        else if (strategy < 30 && !package.deadMoves.count("Remove")) {
            improved = tryRemoveUpgrade(package, context);
        }
        else if (strategy < 32) {
            improved = tryRotateSubsequences(package, context);
        }
        else if (!package.deadMoves.count("Swap")) {
            improved = trySwapUpgrades(package, context);
        }
        if (improved) {
            noImprovementStreak = 0;
            package.deadMoves.clear();
            continue;
        } else {
            noImprovementStreak++;
        }
    }
}

// =================== MAIN ==============================================
int main() {
    // Load config
    AppConfig cfg = loadConfig("config.json");

    // Apply config to runtime globals
    eventDurationDays = max(0, cfg.eventDurationDays);
    eventDurationHours = max(0, cfg.eventDurationHours);
    eventDurationMinutes = max(0, cfg.eventDurationMinutes);
    eventDurationSeconds = max(0, cfg.eventDurationSeconds);
    long long computedSeconds = static_cast<long long>(eventDurationDays) * 24LL * 3600LL
        + static_cast<long long>(eventDurationHours) * 3600LL
        + static_cast<long long>(eventDurationMinutes) * 60LL
        + static_cast<long long>(eventDurationSeconds);
    if (computedSeconds <= 0) {
        computedSeconds = 1;
    }
    totalSeconds = static_cast<int>(min<long long>(computedSeconds, numeric_limits<int>::max()));

    UNLOCKED_PETS = cfg.UNLOCKED_PETS;
    DLs = cfg.DLs;
    outputInterval = cfg.outputInterval;
    EVENT_CURRENCY_WEIGHT = cfg.EVENT_CURRENCY_WEIGHT;
    FREE_EXP_WEIGHT = cfg.FREE_EXP_WEIGHT;
    PET_STONES_WEIGHT = cfg.PET_STONES_WEIGHT;
    GROWTH_WEIGHT = cfg.GROWTH_WEIGHT;
    isFullPath = cfg.isFullPath;
    allowSpeedUpgrades = cfg.allowSpeedUpgrades;
    runOptimization = cfg.runOptimization;
    logToConsoleEnabled = cfg.logToConsole;
    logToFileEnabled = cfg.logToFile;
    appendToLogFile = cfg.appendLogFile;
    if (!cfg.logFilePath.empty()) {
        logFilePath = cfg.logFilePath;
    }
    pauseOnExit = cfg.pauseOnExit;
    maxOptimizationIterations = cfg.maxOptimizationIterations;
    currentLevels = cfg.currentLevels;
    resourceCounts = cfg.resourceCounts;
    clampEventCurrency(resourceCounts);
    upgradePath = cfg.upgradePath;
    busyTimesStart = cfg.busyTimesStart;
    busyTimesEnd = cfg.busyTimesEnd;
    for (int i=0;i<NUM_RESOURCES;i++) resourceNames[i] = cfg.resourceNames[i];

    Logger logger(outputInterval,
                  logToConsoleEnabled,
                  logToFileEnabled ? logFilePath : string(),
                  appendToLogFile);
    Logger* loggerPtr = &logger;
    if (logToFileEnabled) {
        const string logMsg = string("Improvement log file: ") + logFilePath + "\n";
        if (!loggerPtr->isConsoleEnabled()) {
            cout << logMsg;
        }
        loggerPtr->logLine(logMsg);
    }
    ostringstream mapping;
    mapping << "Resource mapping: ";
    for (int i=0;i<NUM_RESOURCES;i++){
        mapping << i << "=" << resourceNames[i];
        if (i+1<NUM_RESOURCES) mapping << ", ";
    }
    mapping << "\n";
    const string mappingStr = mapping.str();
    if (!loggerPtr->isConsoleEnabled()) {
        cout << mappingStr;
    }
    loggerPtr->logLine(mappingStr);

    nameUpgrades();
    preprocessBusyTimes(busyTimesStart, busyTimesEnd);

    if (isFullPath) {
        adjustFullPath(currentLevels);
    }
    if (upgradePath.empty()) {
        upgradePath = generateRandomPath();
    }
    pruneCappedSpeedUpgrades(upgradePath, currentLevels);

    calculateFinalPath(upgradePath, loggerPtr);

    if (runOptimization) {
        random_device seed;
        mt19937 randomEngine(seed());
        SearchContext context{*loggerPtr, resourceCounts, currentLevels};
        OptimizationPackage package = {upgradePath, 0, move(randomEngine)};
        optimizeUpgradePath(package, context, maxOptimizationIterations);
        upgradePath = move(package.path);
    }

    pruneCappedSpeedUpgrades(upgradePath, currentLevels);
    calculateFinalPath(upgradePath, loggerPtr);
    const string doneMessage = string("Done.\n");
    if (loggerPtr) {
        if (!loggerPtr->isConsoleEnabled()) {
            cout << doneMessage;
        }
        loggerPtr->logLine(doneMessage);
    } else {
        cout << doneMessage;
    }
    if (pauseOnExit) {
        cout << "Press Enter to close..." << flush;
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');
    }
    return 0;
}
