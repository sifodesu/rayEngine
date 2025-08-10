#include "ldtk_m.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include "object_m.h"
#include "spawn.h"

using json = nlohmann::json;
using namespace std;

static inline bool strEndsWith(const std::string &str, const std::string &suffix) {
    if (suffix.size() > str.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin());
}

static inline std::string pathBasename(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    if (pos == std::string::npos) return path;
    return path.substr(pos + 1);
}

void Ldtk_m::loadLevel(std::string filename, bool skipCharacters) {
    if (!strEndsWith(filename, ".ldtk")) {
        cout << "Error: Ldtk_m expects an .ldtk project file, got: " << filename << endl;
        return;
    }

    std::ifstream file((LDTK_PATH + filename).c_str());
    if (!file)
    {
        cout << "Error: Couldn't load the LDtk project " << filename << endl;
        return;
    }

    json ldtk;
    file >> ldtk;
    file.close();

    if (!ldtk.contains("levels") || ldtk["levels"].empty())
    {
        cout << "Error: LDtk project has no levels: " << filename << endl;
        return;
    }

    map<int, std::string> tilesetFilenames;
    if (ldtk.contains("defs") && ldtk["defs"].contains("tilesets"))
    {
        for (auto &ts : ldtk["defs"]["tilesets"]) {
            int uid = ts["uid"];
            std::string rel = ts["relPath"];
            tilesetFilenames[uid] = pathBasename(rel);
        }
    }

    json level = ldtk["levels"][0];

    bool hasIntGrid = false;
    std::vector<int> intGridCsv;
    int intGridWidth = 0;
    int intGridSize = 0;
    if (level.contains("layerInstances"))
    {
        for (auto &layer : level["layerInstances"]) {
            if (layer["__type"] == "IntGrid") {
                hasIntGrid = true;
                intGridCsv = layer["intGridCsv"].get<std::vector<int>>();
                intGridWidth = layer["__cWid"];
                intGridSize = layer["__gridSize"];
                break;
            }
        }
    }

    int layer_num = 0;
        for (auto &layer : level["layerInstances"]) {
        std::string ltype = layer["__type"];
        if (ltype == "Tiles") {
            int tileSize = layer["__gridSize"];
            std::string tilesetRel = layer.contains("__tilesetRelPath") ? (std::string)layer["__tilesetRelPath"] : std::string("");
            std::string tilesetFile = pathBasename(tilesetRel);
            for (auto &tile : layer["gridTiles"]) {
                int px = tile["px"][0];
                int py = tile["px"][1];
                int sx = tile["src"][0];
                int sy = tile["src"][1];

                bool solid = false;
                if (hasIntGrid && intGridSize > 0) {
                    int i = px / intGridSize;
                    int j = py / intGridSize;
                    int idx = j * intGridWidth + i;
                    if (idx >= 0 && idx < (int)intGridCsv.size())
                        solid = intGridCsv[idx] > 0;
                }

                SpawnData d;
                d.id = Object_m::genID();
                d.type = "tile";
                d.layer = layer_num;
                d.sprite = SpriteDesc{tilesetFile, Rectangle{(float)sx, (float)sy, (float)tileSize, (float)tileSize}, WHITE};
                d.collision = CollisionDesc{Rectangle{(float)px, (float)py, (float)tileSize, (float)tileSize}, solid, true};
                Object_m::createFromSpawn(d);
            }
        } else if (ltype == "Entities") {
            if (skipCharacters) { continue; }
            for (auto &e : layer["entityInstances"]) {
                SpawnData d;

                if (e.contains("fieldInstances")) {
                    for (auto &f : e["fieldInstances"]) {
                        std::string fid = f["__identifier"];
                        if (fid == "type") {
                            d.type = f["__value"];
                        } else if (fid == "solid") {
                            if (!d.collision) d.collision = CollisionDesc{};
                            d.collision->solid = f["__value"];
                        }
                    }
                }
                if (d.type.empty()) d.type = "basic";

                if (e.contains("__tile") && e["__tile"].is_object()) {
                    int uid = e["__tile"]["tilesetUid"];
                    if (tilesetFilenames.contains(uid)) {
                        if (!d.sprite) d.sprite = SpriteDesc{};
                        d.sprite->filename = tilesetFilenames[uid];
                    }
                    if (!d.sprite) d.sprite = SpriteDesc{};
                    d.sprite->source = Rectangle{(float)e["__tile"]["x"], (float)e["__tile"]["y"], (float)e["__tile"]["w"], (float)e["__tile"]["h"]};
                }

                if (!d.collision) d.collision = CollisionDesc{};
                d.collision->rect = Rectangle{(float)e["px"][0], (float)e["px"][1], (float)e["width"], (float)e["height"]};
                d.collision->isStatic = false;
                d.id = Object_m::genID();
                d.layer = layer_num + 100;
                Object_m::createFromSpawn(d);
            }
        }
        layer_num++;
    }
}


