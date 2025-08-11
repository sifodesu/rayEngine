#pragma once
#include "raylib.h"
#include "definitions.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

class Ldtk_m {
public:
    // Load an LDtk project (.ldtk). If skipCharacters is true, entity layers are ignored.
    static void loadLevel(const std::string& filename, bool skipCharacters = false);
    static void routine(); // hot reload check
    static void enableHotReload(bool v) { hotReloadEnabled = v; }
private:
    static void checkHotReload();
    static inline bool hotReloadEnabled = true;
    static inline std::filesystem::file_time_type lastWrite{};
    static inline std::string currentProjectFile{"ldtk_test.ldtk"};
};


