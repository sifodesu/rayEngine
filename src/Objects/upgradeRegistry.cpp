#include "upgradeRegistry.h"
#include "character.h"
#include <iostream>

std::unordered_map<std::string, UpgradeRegistry::EffectFn> UpgradeRegistry::effects_;

void UpgradeRegistry::registerEffect(const std::string& name, EffectFn fn) {
    effects_[name] = std::move(fn);
}

bool UpgradeRegistry::apply(const std::string& name, Character& chr) {
    auto it = effects_.find(name);
    if (it == effects_.end()) return false;
    it->second(chr);
    return true;
}

void UpgradeRegistry::initDefaults() {
    if (!effects_.empty()) return;
    registerEffect("upgrade_speed", [](Character& c){ c.addSpeedBoost(0.5f); });
    registerEffect("upgrade_dash",  [](Character& c){ c.addDashBoost(0.25f); });
}
