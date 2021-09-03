#include <fstream>
#include "definitions.h"
#include "object_m.h"
#include "sprite.h"
#include "nlohmann/json.hpp"
#include "rigidBody.h"
#include "basicEnt.h"
#include "character.h"

using json = nlohmann::json;
using namespace std;

std::map<int, GObject*> Object_m::blueprints_;	// ents which cannot be placed on a map
std::map<int, GObject*> Object_m::level_ents_;	// ents of the current level

std::string Object_m::level_name_;

void Object_m::loadBlueprints() {
    load(BLUEPRINTS_PATH, blueprints_);
}
void Object_m::loadLevel(std::string level_name) {
    load(LEVELS_PATH + level_name, level_ents_);
}


void Object_m::load(string path, map<int, GObject*>& container) {
    std::ifstream file(path.c_str());
    if (!file) {
        cout << "Error: Couldn't load the file at path " << path << endl;
        return;
    }
    container.clear();

    json objArray;
    file >> objArray;

    for (auto& ojs : objArray) {
        if (!ojs.contains("ID")) {
            cout << "Error: object with no ID" << endl;
            continue;
        }
        if (container.contains(ojs["ID"])) {
            cout << "Error: objects with same id. Duplicate not loaded" << endl;
            continue;
        }
        if (!ojs.contains("type")) {
            cout << "Error: object with no type" << endl;
            continue;
        }
        GObject* cur = NULL;
        if (ojs["type"] == "basic")
            cur = new BasicEnt(ojs);
        if (ojs["type"] == "character")
            cur = new Character(ojs);
        if (cur)
            container[ojs["ID"]] = cur;
    }
    file.close();
}

void Object_m::routine() {
    for (auto& [id, obj] : level_ents_) {
        obj->routine();
    }
}


void Object_m::unloadAll() {
    for (auto& [id, obj] : blueprints_)
        delete (obj);
    for (auto& [id, obj] : level_ents_)
        delete (obj);
}