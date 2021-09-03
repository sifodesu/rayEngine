#pragma once
#include <map>
#include <raylib.h>
#include <string>

class InputMap {
public:
    static void init();
    static bool checkDown(std::string);
    static bool checkPressed(std::string);
    static bool checkUp(std::string);
    static std::map<std::string, int> mapping;
};