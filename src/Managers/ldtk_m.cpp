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

namespace {

bool strEndsWith(const string& s, const string& suf) {
    return s.size() >= suf.size() && equal(suf.rbegin(), suf.rend(), s.rbegin());
}

// Extract filename from a path (handles / or \). Returns path if no separators.
string basename(const string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == string::npos) ? path : path.substr(pos + 1);
}

// Build map tilesetUid -> tileset filename (basename only) for quick lookup.
map<int,string> collectTilesetNames(const json& root) {
    map<int,string> out;
    if (!root.contains("defs") || !root["defs"].contains("tilesets")) return out;
    for (auto& ts : root["defs"]["tilesets"]) {
        int uid = ts["uid"];
        out[uid] = basename(ts["relPath"].get<string>());
    }
    return out;
}

// Minimal IntGrid capture: presence flag, flattened csv, width, and cell size.
struct IntGridInfo { bool has=false; vector<int> csv; int width=0; int cell=0; };
// Locate the first IntGrid layer (if any) and capture its CSV & dimensions.
IntGridInfo extractIntGrid(const json& level) {
    IntGridInfo info;
    if (!level.contains("layerInstances")) return info;
    for (auto& layer : level["layerInstances"]) {
        if (layer["__type"] == "IntGrid") {
            info.has = true;
            info.csv = layer["intGridCsv"].get<vector<int>>();
            info.width = layer["__cWid"];
            info.cell = layer["__gridSize"];
            break;
        }
    }
    return info;
}

// Convert a tile's local pixel coordinate into an IntGrid cell index and test solidity.
bool isTileSolid(const IntGridInfo& g, int localPx, int localPy) {
    if (!g.has || g.cell <= 0) return false;
    int i = localPx / g.cell;
    int j = localPy / g.cell;
    int idx = j * g.width + i;
    return idx >= 0 && idx < (int)g.csv.size() && g.csv[idx] > 0;
}

// Spawn a tile object: sprite source rectangle + collision box (static if solid/not solid)
void spawnTile(const string& tilesetFile, int tileSize, int sx, int sy, int px, int py, bool solid, int layer) {
    SpawnData d;
    d.id = Object_m::genID();
    d.type = "tile";
    d.layer = layer;
    d.sprite = SpriteDesc{tilesetFile, Rectangle{(float)sx,(float)sy,(float)tileSize,(float)tileSize}, WHITE};
    d.collision = CollisionDesc{Rectangle{(float)px,(float)py,(float)tileSize,(float)tileSize}, solid, true};
    Object_m::createFromSpawn(d);
}

// Pull user-defined fields (type, solid) from LDtk entity instance into SpawnData.
void fillEntityFields(const json& inst, SpawnData& d) {
    if (!inst.contains("fieldInstances")) return;
    for (auto& f : inst["fieldInstances"]) {
        string fid = f["__identifier"];
        if (fid == "type") d.type = f["__value"].get<string>();
        else if (fid == "solid") {
            if (!d.collision) d.collision = CollisionDesc{};
            d.collision->solid = f["__value"].get<bool>();
        }
    }
}

// If the entity uses a tileset tile, set its sprite filename + source rectangle.
void fillEntityTile(const json& e, const map<int,string>& tilesetNames, SpawnData& d) {
    if (!e.contains("__tile") || !e["__tile"].is_object()) return;
    int uid = e["__tile"]["tilesetUid"];
    if (!d.sprite) d.sprite = SpriteDesc{};
    auto it = tilesetNames.find(uid);
    if (it != tilesetNames.end()) d.sprite->filename = it->second;
    d.sprite->source = Rectangle{(float)e["__tile"]["x"], (float)e["__tile"]["y"], (float)e["__tile"]["w"], (float)e["__tile"]["h"]};
}

// Fully construct and spawn an entity; defaults to type "basic" if unspecified.
void spawnEntity(const json& e, int worldX, int worldY, int layer, const map<int,string>& tilesetNames) {
    SpawnData d; // default empty
    fillEntityFields(e, d);
    if (d.type.empty()) d.type = "basic";
    fillEntityTile(e, tilesetNames, d);
    if (!d.collision) d.collision = CollisionDesc{};
    d.collision->rect = Rectangle{(float)(e["px"][0].get<int>() + worldX), (float)(e["px"][1].get<int>() + worldY), (float)e["width"].get<int>(), (float)e["height"].get<int>()};
    d.collision->isStatic = false;
    d.id = Object_m::genID();
    d.layer = layer;
    Object_m::createFromSpawn(d);
}
} // namespace

// Public API: import an LDtk project (.ldtk). Optionally skip entities (characters) when doing hot reload.
void Ldtk_m::loadLevel(const string& filename, bool skipCharacters) {
    if (!strEndsWith(filename, ".ldtk")) { cerr << "LDtk: expected .ldtk file got " << filename << '\n'; return; }

    ifstream file((LDTK_PATH + filename).c_str());
    if (!file) { cerr << "LDtk: can't open project " << filename << '\n'; return; }

    json root; file >> root;
    if (!root.contains("levels") || root["levels"].empty()) { cerr << "LDtk: no levels in " << filename << '\n'; return; }

    currentProjectFile = filename; // track for hot reload
    auto tilesetNames = collectTilesetNames(root);

    for (auto& level : root["levels"]) { // Each LDtk level (supports multi-level worlds)
        int worldX = level.value("worldX", 0);
        int worldY = level.value("worldY", 0);
        auto intGrid = extractIntGrid(level);

        int layerIndex = 0;
    for (auto& layer : level["layerInstances"]) { // Iterate draw order as provided
            string type = layer["__type"].get<string>();
            if (type == "Tiles") { // Tile layer -> spawn tiles
                int tileSize = layer["__gridSize"].get<int>();
                string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                for (auto& tile : layer["gridTiles"]) {
                    int localPx = tile["px"][0];
                    int localPy = tile["px"][1];
                    int sx = tile["src"][0];
                    int sy = tile["src"][1];
                    bool solid = isTileSolid(intGrid, localPx, localPy);
                    spawnTile(tilesetFile, tileSize, sx, sy, localPx + worldX, localPy + worldY, solid, layerIndex);
                }
            } else if (type == "Entities" && !skipCharacters) { // Entity layer -> spawn entities
                for (auto& e : layer["entityInstances"]) {
                    // Keep large offset from tile layers but simpler constant
                    spawnEntity(e, worldX, worldY, layerIndex + 100, tilesetNames);
                }
            }
            ++layerIndex;
        }
    }
}

// Hot reload: if project timestamp changed, clear only tiles (preserve dynamic ents) and re-import (entities skipped).
void Ldtk_m::checkHotReload() {
    if (!hotReloadEnabled || currentProjectFile.empty()) return;
    error_code ec; auto path = filesystem::path(LDTK_PATH) / currentProjectFile; auto cur = filesystem::last_write_time(path, ec); if (ec) return;
    if (lastWrite.time_since_epoch().count() == 0) { lastWrite = cur; return; }
    if (cur == lastWrite) return;
    lastWrite = cur;
    try { Object_m::clearTiles(); loadLevel(currentProjectFile, true); } catch (...) { /* swallow */ }
}

void Ldtk_m::routine() { checkHotReload(); }
