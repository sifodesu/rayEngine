#pragma once
#include "raylib.h"
#include "definitions.h"
#include <iostream>
#include <filesystem>

class Ldtk_m {
public:
    static void loadLevel(std::string filename, bool skipCharacters = false);
    static void routine(); // hot reload check
    static void enableHotReload(bool v) { hotReloadEnabled = v; }
private:
    static void checkHotReload();
    static inline bool hotReloadEnabled = true;
    static inline std::filesystem::file_time_type lastWrite{};
};


