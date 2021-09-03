#pragma once
#include <map>
#include <raylib.h>
#include <string>

class InputMap {
public:
    static void init();
    static bool checkAction(std::string);
    static std::map<std::string, int> mapping;
};