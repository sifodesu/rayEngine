#include "input.h"

std::map<std::string, int> InputMap::mapping;

void InputMap::init() {
    mapping["left"] = KEY_A;
    mapping["right"] = KEY_D;
    mapping["up"] = KEY_W;
    mapping["down"] = KEY_S;
    mapping["r1"] = KEY_N;
    mapping["r2"] = KEY_J;
    mapping["r3"] = KEY_K;
    mapping["r4"] = KEY_L;
    mapping["dash"] = KEY_LEFT_SHIFT;
}

bool InputMap::checkDown(std::string action) {
    if (IsKeyDown(mapping[action]))
        return true;
    return false;
}
bool InputMap::checkPressed(std::string action) {
    if (IsKeyPressed(mapping[action]))
        return true;
    return false;
}
bool InputMap::checkUp(std::string action) {
    if (IsKeyUp(mapping[action]))
        return true;
    return false;
}