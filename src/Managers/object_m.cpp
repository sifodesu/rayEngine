#include <fstream>
#include "definitions.h"
#include "object_m.h"
#include "sprite.h"
#include "rigidBody.h"
#include "basicEnt.h"
#include "character.h"
#include "bullet.h"
#include "simpleBoss.h"
#include "bullet_m.h"

using json = nlohmann::json;
using namespace std;

std::map<BPE, json> Object_m::blueprints_;      // ents which cannot be placed on a map
std::map<int, GObject*> Object_m::level_ents_; // ents of the current level

std::string Object_m::level_name_;

GObject* Object_m::createObj(BPE target) {
    if (blueprints_.contains(target)) {
        return createObjJson(blueprints_[target]);
    }
    cout << "Error: no blueprint with target " << target << endl;
    return NULL;
}
int Object_m::genID() {
    if (level_ents_.size()) {
        return (--level_ents_.end())->first + 1;
    }
    return 0;
}

GObject* Object_m::getObj(string type) {
    for (auto [id, obj] : level_ents_) {
        if (t(*obj) == type) {
            return obj;
        }
    }
    return nullptr;
}

GObject* Object_m::createObjJson(json ojs) {
    if (!ojs.contains("ID") || level_ents_.contains(ojs["ID"])) {
        ojs["ID"] = genID();
    }

    if (!ojs.contains("type")) {
        cout << "Error: object with no type" << endl;
        return NULL;
    }
    GObject* cur = NULL;
    if (ojs["type"] == "basic")
        cur = new BasicEnt(ojs);
    if (ojs["type"] == "bullet")
        cur = new Bullet(ojs);
    if (ojs["type"] == "character")
        cur = new Character(ojs);
    if (ojs["type"] == "simpleBoss")
        cur = new SimpleBoss(ojs);
    if (cur)
        level_ents_[ojs["ID"]] = cur;

    return cur;
}

void Object_m::loadBlueprints() {
    std::ifstream file(BLUEPRINTS_PATH);
    if (!file) {
        cout << "Error: Couldn't load the blueprints file" << endl;
        return;
    }
    blueprints_.clear();

    json objArray;
    file >> objArray;

    for (auto& ojs : objArray) {
        blueprints_[ojs["target"]] = ojs;
    }
    file.close();
}

void Object_m::loadLevel(string level_name) {
    std::ifstream file((LEVELS_PATH + level_name).c_str());
    if (!file) {
        cout << "Error: Couldn't load the level " << level_name << endl;
        return;
    }
    level_ents_.clear();

    json objArray;
    file >> objArray;
    if (objArray.contains("bgRelPath")) {
        json newBasic;
        newBasic["ID"] = genID();
        newBasic["type"] = "basic";
        newBasic["collisionRect"] = { {"x", objArray["__bgPos"]["topLeftPx"][0]},
                                        {"y", objArray["__bgPos"]["topLeftPx"][1]}
        };
        newBasic["sprite"] = { {"filename", objArray["bgRelPath"]} };
        createObjJson(newBasic);
    }
    objArray = objArray["layerInstances"];
    for (auto& ojs : objArray) {
        if (ojs["__type"] == "IntGrid") {
            int gridSize = ojs["__gridSize"];
            int width = ojs["__cWid"];
            int height = ojs["__cHei"];
            for (int h = 0; h < height; h++) {
                for (int w = 0; w < width; w++) {
                    int content = ojs["intGridCsv"][w + width * h];
                    if (!content)
                        continue;
                    json newBasic;
                    newBasic["ID"] = genID();
                    newBasic["type"] = "basic";
                    newBasic["collisionRect"] = { {"x", w * gridSize},
                                                    {"y", h * gridSize},
                                                    {"w", gridSize},
                                                    {"h", gridSize},
                                                    {"solid", content == 1}
                    };
                    createObjJson(newBasic);
                }
            }
        }
        if (ojs["__type"] == "Entities") {
            ojs = ojs["entityInstances"];
            for (auto& obj : ojs) {
                obj["ID"] = genID();
                for (auto& field : obj["fieldInstances"]) {
                    if (field["__identifier"] == "type") {
                        obj["type"] = field["__value"];
                        break;
                    }
                }
                if (obj.contains("type")) {
                    createObjJson(obj);
                }
                else
                    std::cout << "ERROR: Ent with no type" << std::endl;
            }
        }
    }
    json newBasic;
    newBasic["ID"] = genID();
    newBasic["type"] = "character";
    newBasic["collisionRect"] = { {"x", 100},
                                    {"y", 100},
                                    {"w", 5},
                                    {"h", 5},
                                    {"solid", true}
    };
    createObjJson(newBasic);

    file.close();
}

void Object_m::deleteObj(int id) {
    if (level_ents_.contains(id)) {
        delete (level_ents_[id]);
        level_ents_.erase(id);
    }
}

void Object_m::routine() {
    int nb_ents = level_ents_.size();
    for (auto& [type, set] : Bullet_m::active_bullets)
        nb_ents += set.size();
    DrawText(std::to_string(nb_ents).c_str(), 200, 10, 20, BLACK);

    vector<int> toDelete;
    for (auto& [id, obj] : level_ents_) {
        obj->routine();
        if (obj->to_delete_) {
            toDelete.push_back(id);
        }
    }

    for (int id : toDelete) {
        deleteObj(id);
    }
}

void Object_m::unload() {
    for (auto& [id, obj] : level_ents_)
        delete (obj);
    level_ents_.clear();
}