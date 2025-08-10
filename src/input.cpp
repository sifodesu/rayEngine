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

static inline int getKeyFor(const std::string& action) {
    auto it = InputMap::mapping.find(action);
    if (it == InputMap::mapping.end()) return 0;
    return it->second;
}

bool InputMap::checkDown(std::string action) {
    int key = getKeyFor(action);
    return key != 0 && IsKeyDown(key);
}
bool InputMap::checkPressed(std::string action) {
    int key = getKeyFor(action);
    return key != 0 && IsKeyPressed(key);
}
bool InputMap::checkUp(std::string action) {
    int key = getKeyFor(action);
    return key != 0 && IsKeyUp(key);
}