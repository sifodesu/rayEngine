#pragma once
#include <functional>
#include <string>
#include <unordered_map>
class Character;

class UpgradeRegistry {
public:
    using EffectFn = std::function<void(Character&)>;
    static void registerEffect(const std::string& name, EffectFn fn);
    static bool apply(const std::string& name, Character& chr);
    static void initDefaults();
private:
    static std::unordered_map<std::string, EffectFn> effects_;
};
