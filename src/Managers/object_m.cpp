#include "definitions.h"
#include "object_m.h"
#include "sprite.h"
#include "nlohmann/json.hpp"
#include <fstream>

using json = nlohmann::json;
using namespace std;

std::map<int, GObject*> Object_m::blueprints_;	// ents which cannot be placed on a map
std::map<int, GObject*> Object_m::level_ents_;	// ents of the current level
Quadtree Object_m::quad_ents_;
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
        GObject* cur = new GObject(ojs["ID"]);

        for (auto& [key, value] : ojs.items()) {
            if (key == "sprite") {
                auto sprite = new Sprite(ojs["sprite"]);
                cur->addComponent(sprite);
            }
        }
        container[ojs["ID"]] = cur;
        //put obj in quad.
    }
    file.close();
}

void Object_m::routine() {
    for (auto& [id, obj] : level_ents_) {
        obj->routine();
    }
    //update elements in quad
}

std::vector<GObject*> Object_m::queryQuad(Rectangle rect) {
    std::vector<quadNode> nodes = quad_ents_.query(Box<float>(rect.x, rect.y, rect.width, rect.height));
    std::vector<GObject*> ret;
    for (auto& node : nodes) {
        if (level_ents_.contains(node.id)) {
            ret.push_back(level_ents_[node.id]);
        }
    }
    return ret;
}

void Object_m::unloadAll() {
    for (auto& [id, obj] : blueprints_)
        delete (obj);
    for (auto& [id, obj] : level_ents_)
        delete (obj);
}