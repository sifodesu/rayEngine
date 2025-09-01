#pragma once
#include "raylib.h"
#include "definitions.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <map>

class Ldtk_m {
public:
    static void loadLevel(const std::string& filename, bool skipCharacters = false);
    static void enableHotReload(bool v) { hotReloadEnabled = v; }
    static int getEngineIdFromLdtkId(const std::string& ldtkId); // Get engine object ID from LDtk entity IID
    static void clearIdMapping(); // Clear the ID mapping (called before loading new level)
    static void registerIdMapping(const std::string& ldtkId, int engineId); // Register LDtk ID -> Engine ID mapping
private:
    static inline bool hotReloadEnabled = true;
    static inline std::filesystem::file_time_type lastWrite{};
    static inline std::string currentProjectFile{"ldtk_test.ldtk"};
    static inline std::map<std::string, int> ldtkIdToEngineId_; // Map LDtk entity IID to engine object ID
};


