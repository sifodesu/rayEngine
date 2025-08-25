#include "save_manager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include "object_m.h"
#include "character.h"
#include "definitions.h"

using json = nlohmann::json;

std::string SaveManager::saveFilePath() {
    return SAVE_FILE_PATH; // central path constant
}

void SaveManager::load() {
    if (loaded_) return; // only load once
    loaded_ = true;
    std::ifstream f(saveFilePath());
    if (!f.good()) return; // no save yet
    try {
        json j; f >> j;
        if (j.contains("lastCheckpoint") && j["lastCheckpoint"].is_object()) {
            auto& lc = j["lastCheckpoint"];
            if (lc.contains("x") && lc.contains("y")) {
                data_.lastCheckpoint = { lc["x"].get<float>(), lc["y"].get<float>() };
                data_.hasCheckpoint = true;
            }
        }
    } catch (...) {
        // ignore corrupt file
    }
}

void SaveManager::save() {
    json j;
    if (data_.hasCheckpoint) {
        j["lastCheckpoint"] = { {"x", data_.lastCheckpoint.x}, {"y", data_.lastCheckpoint.y} };
    }
    std::ofstream f(saveFilePath(), std::ios::trunc);
    if (!f.good()) return;
    f << j.dump(2);
}

void SaveManager::registerCheckpoint(Vector2 pos) {
    data_.lastCheckpoint = pos;
    data_.hasCheckpoint = true;
    save();
}

void SaveManager::applyToWorld() {
    if (!data_.hasCheckpoint) return;
    // Find the (first) Character, set respawn and move it immediately before gameplay starts.
    for (auto& [id, ptr] : Object_m::level_ents_) {
        if (auto* chr = dynamic_cast<Character*>(ptr.get())) {
            chr->setRespawn(data_.lastCheckpoint);
            // also place character body at respawn position
            if (chr->body_) chr->body_->setCoord(data_.lastCheckpoint);
            break; // only one player character assumed
        }
    }
}

const SaveManager::Data& SaveManager::get() { return data_; }
