#include "input.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <unordered_map>

using json = nlohmann::json;

namespace {

constexpr const char* INPUT_CONFIG_PATH = "input.json";

using BindingMap = std::map<std::string, std::vector<int>>;

BindingMap defaultBindings() {
    return {
        {"left", {KEY_A}},
        {"right", {KEY_D}},
        {"up", {KEY_W}},
        {"down", {KEY_S}},
        {"r1", {KEY_N}},
        {"r2", {KEY_J}},
        {"r3", {KEY_K}},
        {"r4", {KEY_L}},
        {"dash", {KEY_LEFT_SHIFT}},
        {"reload", {KEY_R}},
        {"screenshot", {KEY_PRINT_SCREEN}},
        {"toggle_collision", {KEY_C}},
    };
}

std::string normaliseKeyName(std::string name) {
    name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char c) {
        return std::isspace(c);
    }), name.end());
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    std::replace(name.begin(), name.end(), '-', '_');
    if (name.rfind("KEY_", 0) == 0) name.erase(0, 4);
    return name;
}

const std::unordered_map<std::string, int>& keysByName() {
    static const std::unordered_map<std::string, int> keys = {
        {"APOSTROPHE", KEY_APOSTROPHE}, {"COMMA", KEY_COMMA}, {"MINUS", KEY_MINUS},
        {"PERIOD", KEY_PERIOD}, {"SLASH", KEY_SLASH}, {"SEMICOLON", KEY_SEMICOLON},
        {"EQUAL", KEY_EQUAL}, {"LEFT_BRACKET", KEY_LEFT_BRACKET},
        {"BACKSLASH", KEY_BACKSLASH}, {"RIGHT_BRACKET", KEY_RIGHT_BRACKET},
        {"GRAVE", KEY_GRAVE}, {"SPACE", KEY_SPACE}, {"ESCAPE", KEY_ESCAPE},
        {"ESC", KEY_ESCAPE}, {"ENTER", KEY_ENTER}, {"RETURN", KEY_ENTER},
        {"TAB", KEY_TAB}, {"BACKSPACE", KEY_BACKSPACE}, {"INSERT", KEY_INSERT},
        {"DELETE", KEY_DELETE}, {"RIGHT", KEY_RIGHT}, {"LEFT", KEY_LEFT},
        {"DOWN", KEY_DOWN}, {"UP", KEY_UP}, {"PAGE_UP", KEY_PAGE_UP},
        {"PAGE_DOWN", KEY_PAGE_DOWN}, {"HOME", KEY_HOME}, {"END", KEY_END},
        {"CAPS_LOCK", KEY_CAPS_LOCK}, {"SCROLL_LOCK", KEY_SCROLL_LOCK},
        {"NUM_LOCK", KEY_NUM_LOCK}, {"PRINT_SCREEN", KEY_PRINT_SCREEN},
        {"PAUSE", KEY_PAUSE}, {"LEFT_SHIFT", KEY_LEFT_SHIFT},
        {"LEFT_CONTROL", KEY_LEFT_CONTROL}, {"LEFT_CTRL", KEY_LEFT_CONTROL},
        {"LEFT_ALT", KEY_LEFT_ALT}, {"LEFT_SUPER", KEY_LEFT_SUPER},
        {"RIGHT_SHIFT", KEY_RIGHT_SHIFT}, {"RIGHT_CONTROL", KEY_RIGHT_CONTROL},
        {"RIGHT_CTRL", KEY_RIGHT_CONTROL}, {"RIGHT_ALT", KEY_RIGHT_ALT},
        {"RIGHT_SUPER", KEY_RIGHT_SUPER}, {"KB_MENU", KEY_KB_MENU},
        {"MENU", KEY_MENU}, {"BACK", KEY_BACK}, {"VOLUME_UP", KEY_VOLUME_UP},
        {"VOLUME_DOWN", KEY_VOLUME_DOWN}, {"KP_DECIMAL", KEY_KP_DECIMAL},
        {"KP_DIVIDE", KEY_KP_DIVIDE}, {"KP_MULTIPLY", KEY_KP_MULTIPLY},
        {"KP_SUBTRACT", KEY_KP_SUBTRACT}, {"KP_ADD", KEY_KP_ADD},
        {"KP_ENTER", KEY_KP_ENTER}, {"KP_EQUAL", KEY_KP_EQUAL},
    };
    return keys;
}

std::optional<int> keyFromName(const std::string& configuredName) {
    const std::string name = normaliseKeyName(configuredName);
    if (name.size() == 1) {
        const char key = name.front();
        if (key >= 'A' && key <= 'Z') return KEY_A + (key - 'A');
        if (key >= '0' && key <= '9') return KEY_ZERO + (key - '0');
    }

    if (name.size() == 2 && name[0] == 'F' && name[1] >= '1' && name[1] <= '9') {
        return KEY_F1 + (name[1] - '1');
    }
    if (name.size() == 3 && name[0] == 'F' && name[1] == '1' && name[2] >= '0' && name[2] <= '2') {
        return KEY_F10 + (name[2] - '0');
    }
    if (name.rfind("KP_", 0) == 0 && name.size() == 4 && name[3] >= '0' && name[3] <= '9') {
        return KEY_KP_0 + (name[3] - '0');
    }

    const auto it = keysByName().find(name);
    if (it != keysByName().end()) return it->second;
    return std::nullopt;
}

std::optional<std::vector<int>> parseBindings(const json& value, const std::string& action) {
    std::vector<std::string> configuredKeys;
    if (value.is_string()) {
        configuredKeys.push_back(value.get<std::string>());
    } else if (value.is_array()) {
        for (const json& key : value) {
            if (!key.is_string()) {
                TraceLog(LOG_WARNING, "Input: action '%s' contains a non-string key", action.c_str());
                return std::nullopt;
            }
            configuredKeys.push_back(key.get<std::string>());
        }
    } else {
        TraceLog(LOG_WARNING, "Input: action '%s' must be a string or an array of strings", action.c_str());
        return std::nullopt;
    }

    std::vector<int> parsedKeys;
    for (const std::string& configuredKey : configuredKeys) {
        const std::optional<int> key = keyFromName(configuredKey);
        if (!key) {
            TraceLog(LOG_WARNING, "Input: unknown key '%s' for action '%s'", configuredKey.c_str(), action.c_str());
            return std::nullopt;
        }
        if (std::find(parsedKeys.begin(), parsedKeys.end(), *key) == parsedKeys.end()) {
            parsedKeys.push_back(*key);
        }
    }
    return parsedKeys;
}

const std::vector<int>* getKeysFor(const std::string& action) {
    const auto it = InputMap::mapping.find(action);
    return it == InputMap::mapping.end() ? nullptr : &it->second;
}

template <typename KeyCheck>
bool anyKeyMatches(const std::string& action, KeyCheck check) {
    const std::vector<int>* keys = getKeysFor(action);
    return keys && std::any_of(keys->begin(), keys->end(), check);
}

} // namespace

std::map<std::string, std::vector<int>> InputMap::mapping;

void InputMap::init() {
    mapping = defaultBindings();

    std::ifstream file(INPUT_CONFIG_PATH);
    if (!file.is_open()) return;

    try {
        json config;
        file >> config;
        if (!config.is_object() || !config.contains("bindings") || !config["bindings"].is_object()) {
            TraceLog(LOG_WARNING, "Input: %s must contain a 'bindings' object; using defaults", INPUT_CONFIG_PATH);
            return;
        }

        for (auto& [action, value] : config["bindings"].items()) {
            const std::optional<std::vector<int>> bindings = parseBindings(value, action);
            if (bindings) mapping[action] = *bindings;
        }
    } catch (const json::exception& error) {
        TraceLog(LOG_WARNING, "Input: could not parse %s (%s); using defaults", INPUT_CONFIG_PATH, error.what());
    }
}

bool InputMap::checkDown(const std::string& action) {
    return anyKeyMatches(action, [](int key) { return IsKeyDown(key); });
}
bool InputMap::checkPressed(const std::string& action) {
    return anyKeyMatches(action, [](int key) { return IsKeyPressed(key); });
}
bool InputMap::checkUp(const std::string& action) {
    const std::vector<int>* keys = getKeysFor(action);
    return keys && !keys->empty()
        && std::all_of(keys->begin(), keys->end(), [](int key) { return IsKeyUp(key); });
}
bool InputMap::checkReleased(const std::string& action) {
    return anyKeyMatches(action, [](int key) { return IsKeyReleased(key); });
}
