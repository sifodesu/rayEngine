#include "definitions.h"
#include "object_m.h"
#include "sprite.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

void Object_m::load(string path, map<int, GObject*>& container) {
    char* file = LoadFileText(path.c_str());
    if (file == NULL) {
        cout << "Error: Couldn't load the file at path " << path << endl;
        return;
    }
    container.clear();

    json objArray = file;

    for (auto& ojs : objArray) {
        int tat = ojs["ID"];
        if (!ojs.contains("ID")) {
            cout << "Error: object with no ID" << endl;
            continue;
        }
        GObject* cur = new GObject(ojs["ID"]);

        for (auto& [key, value] : ojs.items()) {
            // if (key == "target")
                // cur->target = value;

            // if (key == "targetnames") {
            //     for (auto tn : ojs["targetnames"])
            //         cur->targetnames.push_back(tn);
            // }
            
            // if (key == "texture") {
            //     auto sprite = new Sprite(value);
            //     cur->addComponent(sprite);
            // }


            // if (field.key() == "flags") {
            //     for (auto fn : ojs["flags"])
            //         cur.flags.push_back(fn);
            // }

            // if (field.key() == "content")
            //     cur.meta = ojs["content"];

            // if (field.key() == "x") {
            //     cur.movingUnit.hitBox.x = ojs["x"];
            //     std::get<0>(cur.movingUnit.savedCoord) = ojs["x"];
            // }

            // if (field.key() == "y") {
            //     cur.movingUnit.hitBox.y = ojs["y"];
            //     std::get<1>(cur.movingUnit.savedCoord) = ojs["y"];
            // }

            // if (field.key() == "useMUnit")
            //     cur.useMUnit = ojs["useMUnit"];

            // if (field.key() == "enabled") {
            //     cur.default_enabled = ojs["enabled"];
            //     cur.enabled = cur.default_enabled;
            // }
        }
        container[cur->id] = cur;

    }

    UnloadFileText((unsigned char*)file);
}

Object_m::Object_m() {
    load(BLUEPRINTS_PATH, blueprints);
}

Object_m::~Object_m() {
    for(auto& [id, obj] : blueprints)
        delete(obj);
    for (auto& [id, obj] : levelEnts)
        delete(obj);
}