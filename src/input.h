#pragma once
#include <map>
#include <raylib.h>
#include <string>
#include <vector>

class InputMap {
public:
    static void init();
    static bool checkDown(const std::string& action);
    static bool checkPressed(const std::string& action);
    static bool checkUp(const std::string& action);
    static bool checkReleased(const std::string& action);

    // Actions can have more than one keyboard binding.  The configuration is
    // read from input.json each time init() is called (including a game reload).
    static std::map<std::string, std::vector<int>> mapping;
};
