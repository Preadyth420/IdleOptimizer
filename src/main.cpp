#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <vector>
#include <string>
#include <map>
#include <random>
#include <array>
#include <unordered_set>
#include <iomanip>
#include <streambuf>
using namespace std;
typedef long long ll;

// Tee streambuf to mirror stdout -> console + file
class TeeBuf : public std::streambuf {
    std::streambuf* sb1;
    std::streambuf* sb2;
public:
    TeeBuf(std::streambuf* s1, std::streambuf* s2) : sb1(s1), sb2(s2) {}
protected:
    virtual int overflow(int c) override {
        if (c == EOF) return !EOF;
        const int r1 = sb1 ? sb1->sputc(c) : c;
        const int r2 = sb2 ? sb2->sputc(c) : c;
        return (r1 == EOF || r2 == EOF) ? EOF : c;
    }
    virtual int sync() override {
        int r1 = sb1 ? sb1->pubsync() : 0;
        int r2 = sb2 ? sb2->pubsync() : 0;
        return (r1 == 0 && r2 == 0) ? 0 : -1;
    }
};

// USER SETTINGS (defaults) ---------------------------------------------------------------------
const int EVENT_DURATION_DAYS = 11;
const int EVENT_DURATION_HOURS = 1;
const int EVENT_DURATION_MINUTES = 0;
const int EVENT_DURATION_SECONDS = 0;
const int UNLOCKED_PETS = 38;
const int DLs = 1009;

// Starting levels (21 entries; 0..9 level, 10..19 speed, 20 dummy)
vector<int> currentLevels = {
    // Production Levels
    9, 18, 14,
    7, 5, 11,
    8, 4, 10,
    6,            // Event Currency
    // Speed Levels
    8, 10, 10,
    7, 6, 10,
    7, 4, 10,
    6,            // Event Currency
    0             // Dummy placeholder
};
// Starting resources (10 entries)
vector<double> resourceCounts = {
    95000,  850000, 267500,
    72400,  130260, 54750,
    3339,
    1/((500.0 + DLs)/5.0),
    1/(UNLOCKED_PETS/100.0),
    447
};
// Initial path
vector<int> upgradePath = {16,7,8,4,10,8,16,17,6,0,4,8,14,16,8,7,10,5,6,4,2,2,17,6,8,2,14,6,0,13,8,7,2,5,6,4,17,14,0,8,2,6,7,14,8,17,0,5,6,1,6,8,17,3,13,0,7,2,6,2,4,8,7,5,2,6,2,4,17,8,9,5,6,7,19,3,0,13,8,19,5,6,9,7,0,2,6,8,7,3,5,6,3,7,0,8,19,9,6,8,6,0,9,5,7,3,19,2,6,8,7,0,6,2,8,9,9,5,9,9,20};
const bool isFullPath = true;
const bool allowSpeedUpgrades = true;
const bool runOptimization = true;

// ADVANCED SETTINGS ---------------------------------------------------------------------
const int outputInterval = 5000; // ms
const double EVENT_CURRENCY_WEIGHT = 100e-5;
const double FREE_EXP_WEIGHT = 6e-5;
const double PET_STONES_WEIGHT = 4.5e-5;
const double GROWTH_WEIGHT = 7e-5;

const vector<double> busyTimesStart =   {19,43,67,91,115,139,163,187,211,235,259};
const vector<double> busyTimesEnd   =   {27,51,75,99,123,147,171,195,219,243,265};

// PROGRAM SETTINGS -----------------------------------------------------------------------
constexpr array<const char*, 10> resourceNames = {{"Tomb", "Bat", "Ghost", "Witch_Book", "Witch_Soup", "Eye", "PET_STONES", "FREE_EXP", "GROWTH", "Black_Cat"}};
constexpr double INFINITY_VALUE = (1e100);
constexpr int NUM_RESOURCES = resourceNames.size();
constexpr int TOTAL_SECONDS = ((EVENT_DURATION_DAYS)*24*3600+(EVENT_DURATION_HOURS)*3600+(EVENT_DURATION_MINUTES)*60+EVENT_DURATION_SECONDS);

map<int, string> upgradeNames;
array<double, TOTAL_SECONDS> timeNeededSeconds{};

// Forward declare config loader so we can optionally load JSON at start (if provided next to exe)
void loadConfigJson(const std::string& path);

// UTILITY FUNCTIONS --------------------------------------------------------------------
template <typename T>
void printVector(vector<T>& x, ostream& out = cout) { for (auto item : x) out << item << ","; }

class NullBuffer : public std::streambuf { public: int overflow(int c) override { return c; } };
class NullStream : public std::ostream { public: NullStream() : std::ostream(&m_sb) {} private: NullBuffer m_sb; };

class Logger {
    int interval;
    mutable chrono::steady_clock::time_point lastLogTime;
    ostream& out;
public:
    Logger(int outputInterval, ostream& output = cout)
        : interval(outputInterval), lastLogTime(chrono::steady_clock::now()), out(output) {}
    void logImprovement(const string& type, vector<int>& path, const double score) const {
        auto now = chrono::steady_clock::now();
        auto elapsed = chrono::duration_cast<chrono::milliseconds>(now - lastLogTime).count();
        if (elapsed >= interval) {
            out << "[Improvement/" << type << "] score=" << fixed << setprecision(8) << score << "\n{";
            printVector(path, out);
            out << "}\n";
            lastLogTime = now;
        }
    }
};
struct SearchContext {
    Logger logger;
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
    static Proposal Insert(int index, int upgradeType, double score) { return Proposal{"Insert", score, index, 0, 0, upgradeType}; }
    static Proposal Remove(int index,  double score) { return Proposal{"Remove", score, index, 0, 0, 0}; }
    static Proposal Swap(int indexA, int indexB, double score) { return Proposal{"Swap", score, indexA, indexB, 0, 0}; }
    static Proposal Rotate(int indexA, int indexB, int rotateIndex, double score) { return Proposal{"Rotate", score, indexA, indexB, rotateIndex, 0}; }
};
void nameUpgrades() {
    for (int i = 0; i < NUM_RESOURCES; i++) {
        upgradeNames[i] = string(resourceNames[i]) + "_Level";
        upgradeNames[i + NUM_RESOURCES] = string(resourceNames[i]) + "_Speed";
    }
    upgradeNames[20] = "Complete";
}
vector <int> adjustFullPath(vector<int>& levels){ // not thread-safe
    levels[1]--; // Adjust first level because it always starts at 1
    auto it = upgradePath.begin();
    while (it != upgradePath.end()) {
        if (levels[*it] > 0) {
            levels[*it]--;
            it = upgradePath.erase(it);
        } else {
            ++it;
        }
    }
    return levels;
}
void printFormattedResults(vector<int>& path, vector<int>& simulationLevels, vector<double>& simulationResources, double finalScore) {
    cout << "Upgrade Path: \n{";
    printVector(path);
    cout << "}\n";
    cout << "Final Resource Counts: ";
    printVector(simulationResources);
    cout << "\n";
    cout << "Final Upgrade Levels: ";
    printVector(simulationLevels);
    cout << "\n";
    cout << "Event Currency: " << simulationResources[9] << "\n";
    cout << "Free Exp (" << DLs << " DLs): "  << simulationResources[7] * (500.0 + DLs) / 5.0 << " (" << simulationResources[7] << " levels * cycles)" << "\n";
    cout << "Pet Stones: " << simulationResources[6] << "\n";
    cout << "Growth (" << UNLOCKED_PETS << " pets): "  << simulationResources[8] * UNLOCKED_PETS / 100.0 << " (" << simulationResources[8] << " levels * cycles)" << "\n";
    cout << "Score: " << fixed << setprecision(8) << finalScore << "\n\n";
}
void preprocessBusyTimes(const vector<double>& startHours, const vector<double>& endHours) {
    for (size_t i = 0; i < startHours.size(); ++i) {
        int startSec = static_cast<int>(startHours[i] * 3600.0);
        int endSec = static_cast<int>(endHours[i] * 3600.0);
        startSec = min(startSec, TOTAL_SECONDS - 1);
        endSec = min(endSec, TOTAL_SECONDS - 1);
        for (int s = startSec; s <= endSec; ++s) {
            timeNeededSeconds[s] = endSec - s;
        }
    }
}
inline double additionalTimeNeeded(double expectedTimeSeconds) {
    int idx = static_cast<int>(expectedTimeSeconds);
    if (idx >= TOTAL_SECONDS) return 0.0;
    return timeNeededSeconds[idx];
}
vector <int> generateRandomPath(int length = TOTAL_SECONDS/3600) {
    vector<int> randomPath = {};
    for (int i = 0; i < length; i++) {
        randomPath.push_back(rand() % NUM_RESOURCES + 10 * (rand() % 2));
    }
    randomPath.push_back(NUM_RESOURCES * 2);
    return randomPath;
}
void readoutUpgrade(int upgradeType, vector<int>& levels, int elapsedSeconds) {
    cout << upgradeNames[upgradeType] << " " << levels[upgradeType]
        << " " << (int)elapsedSeconds/24/3600 << "d "
        << (int)elapsedSeconds/3600%24 << "h "
        << (int)elapsedSeconds/60%60 << "m\n";
}

// ALGORITHM FUNCTIONS ------------------------------------------------------------------
double performUpgrade(vector<int>& levels, vector<double>& resources, int upgradeType, double& remainingTime) {
    constexpr double cycleTimeMultiplier[10] = { 1.0/3.0, 1.0, 1.0/3.0, 1.0/3.0, 1.0/3.0, 1.0/3.0, 1.0/1200.0, 1.0/2500.0, 1.0/1800.0, 1.0/5000.0 };
    constexpr double speedMultipliers[11] = {
        1.0, 1.25, 1.5625, 1.953125, 2.44140625, 3.0517578125, 3.814697265625,
        4.76837158203125, 5.960464477539063, 7.450580596923828, 9.313225746154785
    };
    int resourceType = upgradeType % NUM_RESOURCES;
    if (upgradeType == NUM_RESOURCES * 2) { resourceType = -1; }
    int newLevel = levels[upgradeType] + 1;
    double baseCost = (3.0 * newLevel * newLevel * newLevel + 1.0) * 100.0;
    if (upgradeType >= NUM_RESOURCES) baseCost *= 2.0;
    double cost[10] = {}; double productionRates[10] = {};
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
    double timeElapsed = TOTAL_SECONDS - remainingTime;
    for (int i = 0; i < NUM_RESOURCES; i++) {
        double neededResources = cost[i] - resources[i];
        if (neededResources <= 0) continue;
        if (productionRates[i] == 0) { timeNeeded = INFINITY_VALUE; break; }
        timeNeeded = max(timeNeeded, neededResources / productionRates[i]);
    }
    int busyLookupIndex = static_cast<int>(timeElapsed + timeNeeded);
    if (0 <= busyLookupIndex && busyLookupIndex < TOTAL_SECONDS){ timeNeeded += timeNeededSeconds[busyLookupIndex]; };
    if (timeNeeded >= remainingTime || upgradeType == (2 * NUM_RESOURCES)) {
        timeNeeded = remainingTime;
        for (int i = 0; i < NUM_RESOURCES; ++i) resources[i] += productionRates[i] * timeNeeded;
        return timeNeeded;
    }
    for (int i = 0; i < NUM_RESOURCES; ++i) resources[i] += productionRates[i] * timeNeeded - cost[i];
    levels[upgradeType]++;
    return timeNeeded;
}
double simulateUpgradePath(vector<int>& path, vector<int>& levels, vector<double>& resources, bool display = false) {
    thread_local double time;
    time = TOTAL_SECONDS;
    for (auto upgradeType : path) {
        if (time < 1e-3) return 0;
        if (upgradeType >= NUM_RESOURCES && levels[upgradeType] >= 10) continue; // maxed speed
        double timeTaken = performUpgrade(levels, resources, upgradeType, time);
        time -= timeTaken;
        if (display) {
            int elapsedSeconds = TOTAL_SECONDS - time;
            readoutUpgrade(upgradeType, levels, elapsedSeconds);
        }
    }
    return time;
}
double calculateScore(vector<double>& resources, bool display = false) {
    double score = 0;
    for (int i = 0; i < NUM_RESOURCES; i++) { score += resources[i] * 1e-15; }
    score += (min(resources[9], 10000.0) + max(0.0, (resources[9] - 10000)) * 0.01) * (EVENT_CURRENCY_WEIGHT);
    score += resources[7] * (FREE_EXP_WEIGHT);
    score += resources[8] * (GROWTH_WEIGHT);
    score += resources[6] * (PET_STONES_WEIGHT);
    return score;
}
double evaluatePath(vector<int>& path, const SearchContext& context){
    thread_local vector<double> testResources = context.resources;
    thread_local vector<int> testLevels = context.levels;
    testResources = context.resources;
    testLevels = context.levels;
    simulateUpgradePath(path, testLevels, testResources);
    return calculateScore(testResources);
}
void calculateFinalPath(vector<int>& path){
    vector<int>     simulationLevels(currentLevels);
    vector<double>  simulationResources(resourceCounts);
    cout << "\n=== Final Path Simulation ===\n";
    simulateUpgradePath(path, simulationLevels, simulationResources, true);
    double simulationScore = calculateScore(simulationResources);
    printFormattedResults(path, simulationLevels, simulationResources, simulationScore);
}
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
    double testScore;
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
                double testScore = evaluatePath(candidatePath, context);
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
    package.randomEngine = randomEngine;

    int iterationCount = 0;
    int noImprovementStreak = 0;
    int InsertCount = 0, RemoveCount = 0, SwapCount = 0, RotationCount = 0;

    cout << "Optimization started (maxIterations=" << maxIterations << ")...\n";
    while (noImprovementStreak < maxIterations) {
        iterationCount++;
        bool improved = false;
        int strategy = iterationCount % 100;
        if(package.deadMoves.count("Rotation")){
            break;
        }
        else if (package.deadMoves.count("Insert") && package.deadMoves.count("Remove") && package.deadMoves.count("Swap")){
            improved = tryRotateSubsequences(package, context); if (improved) RotationCount++;
        }
        else if (strategy < 15 && !package.deadMoves.count("Insert")){
            improved = tryInsertUpgrade(package, context); if (improved) InsertCount++;
        }
        else if (strategy < 30 && !package.deadMoves.count("Remove")){
            improved = tryRemoveUpgrade(package, context); if (improved) RemoveCount++;
        }
        else if (strategy < 32){
            improved = tryRotateSubsequences(package, context); if (improved) RotationCount++;
        }
        else if (!package.deadMoves.count("Swap")){
            improved = trySwapUpgrades(package, context); if (improved) SwapCount++;
        }

        if (improved) {
            noImprovementStreak = 0;
            package.deadMoves.clear();
        } else {
            noImprovementStreak++;
        }

        if (iterationCount % 500 == 0) {
            cout << "[iter " << iterationCount << "] score=" << fixed << setprecision(8) << package.score
                 << "  noImp=" << noImprovementStreak << "\n";
        }
    }
    cout << "Optimization finished. Iters=" << iterationCount
         << "  lastNoImpStreak=" << noImprovementStreak << "\n";
}

// Mini JSON loader forwarder
void loadConfigJson(const std::string& path);

int main() {
    // Open tee file
    ofstream logfile("run_log.txt", ios::out | ios::trunc);
    TeeBuf teebuf(cout.rdbuf(), logfile.rdbuf());
    cout.rdbuf(&teebuf); // mirror std::cout into file

    cout << "IdleOptimizer starting..." << endl;
    cout << "TOTAL_SECONDS=" << TOTAL_SECONDS << " (" << (TOTAL_SECONDS/3600.0) << " hours)" << endl;

    nameUpgrades();
    preprocessBusyTimes(busyTimesStart, busyTimesEnd);

    if (upgradePath.empty()) { upgradePath = generateRandomPath(); }
    vector<int> levelsCopy(currentLevels);

    if (isFullPath) { levelsCopy = adjustFullPath(levelsCopy); }

    // Initial score printout
    cout << "Initial path length: " << upgradePath.size() << endl;
    calculateFinalPath(upgradePath);

    if (runOptimization) {
        Logger logger(outputInterval, cout);
        SearchContext context{logger, resourceCounts, currentLevels};
        OptimizationPackage package = {upgradePath, 0.0, mt19937()};
        package.score = evaluatePath(package.path, context); // seed with current score
        cout << "Seed score=" << fixed << setprecision(8) << package.score << "\n";

        optimizeUpgradePath(package, context, 10000); // tweak this if you want longer/shorter runs
        upgradePath = move(package.path);
    }

    calculateFinalPath(upgradePath);

#ifdef _WIN32
    system("pause");
#endif
    return 0;
}
