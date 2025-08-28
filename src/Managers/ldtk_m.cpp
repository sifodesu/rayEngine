#include "ldtk_m.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include "object_m.h"
#include "spawn.h"
#include "definitions.h"
#include "sprite_m.h"

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
    SpriteDesc sd;
    sd.filename = tilesetFile;
    sd.tint = WHITE;
    sd.frameRects.push_back({(float)sx,(float)sy,(float)tileSize,(float)tileSize});
    d.sprite = sd;
    d.collision = CollisionDesc{Rectangle{(float)px,(float)py,(float)tileSize,(float)tileSize}, solid};
    Object_m::createFromSpawn(d);
}

// Pull user-defined fields (type, solid) from LDtk entity instance into SpawnData.
void fillEntityFields(const json& inst, SpawnData& d, int layerGridSize, int worldX, int worldY) {
    if (!inst.contains("fieldInstances")) return;
    for (auto& f : inst["fieldInstances"]) {
        string fid = f["__identifier"];
        if (fid == "Type") d.type = f["__value"].get<string>();
        else if (fid == "solid") {
            if (!d.collision) d.collision = CollisionDesc{};
            d.collision->solid = f["__value"].get<bool>();
        }
        else if (fid == "sprite") {
            string key = f["__value"].get<string>();
            if (!d.sprite) d.sprite = SpriteDesc{};
            auto tryLoad = [&](const std::string& k){ if (auto meta = Sprite_m::get(k)) { *d.sprite = *meta; return true; } return false; };
            bool loaded = tryLoad(key);
            if (!loaded) {
                auto pos = key.find_last_of('.');
                if (pos != string::npos) {
                    string base = key.substr(0, pos);
                    loaded = tryLoad(base);
                }
            }
            if (!loaded) {
                d.sprite->filename = key;
            }
        }
        else if (fid == "flipX") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.sprite) d.sprite = SpriteDesc{};
                d.sprite->flipX = f["__value"].get<bool>();
            }
        }
        else if (fid == "flipY") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.sprite) d.sprite = SpriteDesc{};
                d.sprite->flipY = f["__value"].get<bool>();
            }
        }
        else if (fid == "Dialog") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.dialog = f["__value"].get<string>();
            }
        }
        else if (fid == "Point") {
            if (f.contains("__value") && f["__value"].is_array()) {
                std::vector<Vector2> pts;
                for (auto& p : f["__value"]) {
                    if (p.contains("cx") && p.contains("cy")) {
                        int cx = p["cx"].get<int>();
                        int cy = p["cy"].get<int>();
                        // Convert cell coordinates to absolute pixel coordinates including world offset
                        float absoluteX = (float)(cx * layerGridSize + layerGridSize/2 + worldX);
                        float absoluteY = (float)(cy * layerGridSize + layerGridSize/2 + worldY);
                        pts.push_back({ absoluteX, absoluteY });
                    }
                }
                if (!pts.empty()) d.pathPoints = pts;
            }
        }
        else if (fid == "enabled") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.enabled = f["__value"].get<bool>();
            }
        }
        else if (fid == "trigger") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                // Handle EntityRef type for trigger/targetId field
                if (f["__value"].is_object() && f["__value"].contains("entityIid")) {
                    d.targetId = f["__value"]["entityIid"].get<string>();
                } else if (f["__value"].is_string()) {
                    // Fallback for direct string references
                    d.targetId = f["__value"].get<string>();
                }
            }
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
    Rectangle r{(float)e["__tile"]["x"], (float)e["__tile"]["y"], (float)e["__tile"]["w"], (float)e["__tile"]["h"]};
    if (d.sprite->frameRects.empty()) d.sprite->frameRects.push_back(r); else d.sprite->frameRects[0] = r; // ensure at least one frame
}

// Fully construct and spawn an entity; defaults to type "basic" if unspecified.
void spawnEntity(const json& e, int worldX, int worldY, int layer, const map<int,string>& tilesetNames, int layerGridSize) {
    SpawnData d; // default empty
    fillEntityFields(e, d, layerGridSize, worldX, worldY);
    if (!d.sprite.has_value()) fillEntityTile(e, tilesetNames, d);
    if (d.type.empty()) d.type = "basic";
    if (!d.collision) d.collision = CollisionDesc{};
    d.collision->rect = Rectangle{(float)(e["px"][0].get<int>() + worldX), (float)(e["px"][1].get<int>() + worldY), (float)e["width"].get<int>(), (float)e["height"].get<int>()};
    
    // Extract LDtk entity IID (unique identifier) and store it
    if (e.contains("iid") && e["iid"].is_string()) {
        d.ldtkId = e["iid"].get<string>();
    }
    
    d.id = Object_m::genID();
    d.layer = layer;
    
    // Store mapping from LDtk ID to engine ID
    if (d.ldtkId.has_value()) {
        Ldtk_m::registerIdMapping(*d.ldtkId, d.id);
    }
    
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

    // Clear previous ID mapping
    if (!skipCharacters) {
        clearIdMapping();
    }
    
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
            } else if (type == "AutoLayer") { // AutoLayer -> spawn autolayer tiles
                int tileSize = layer["__gridSize"].get<int>();
                string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                for (auto& tile : layer["autoLayerTiles"]) {
                    int localPx = tile["px"][0];
                    int localPy = tile["px"][1];
                    int sx = tile["src"][0];
                    int sy = tile["src"][1];
                    // AutoLayer tiles are typically decorative, not solid by default
                    // But still check IntGrid for consistency
                    bool solid = isTileSolid(intGrid, localPx, localPy);
                    spawnTile(tilesetFile, tileSize, sx, sy, localPx + worldX, localPy + worldY, solid, layerIndex); 
                }
            } else if (type == "IntGrid") { // IntGrid layer -> spawn auto-generated tiles if any
                if (layer.contains("autoLayerTiles") && !layer["autoLayerTiles"].empty()) {
                    int tileSize = layer["__gridSize"].get<int>();
                    string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                    for (auto& tile : layer["autoLayerTiles"]) {
                        int localPx = tile["px"][0];
                        int localPy = tile["px"][1];
                        int sx = tile["src"][0];
                        int sy = tile["src"][1];
                        // IntGrid tiles with autoLayerTiles are typically solid based on their IntGrid value
                        bool solid = isTileSolid(intGrid, localPx, localPy);
                        spawnTile(tilesetFile, tileSize, sx, sy, localPx + worldX, localPy + worldY, solid, layerIndex);
                    }
                }
            } else if (type == "Entities" && !skipCharacters) { // Entity layer -> spawn entities
                int entityGridSize = layer["__gridSize"].get<int>();
                for (auto& e : layer["entityInstances"]) {
                    spawnEntity(e, worldX, worldY, layerIndex + 100, tilesetNames, entityGridSize);
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

// ID mapping methods
void Ldtk_m::clearIdMapping() {
    ldtkIdToEngineId_.clear();
}

void Ldtk_m::registerIdMapping(const std::string& ldtkId, int engineId) {
    ldtkIdToEngineId_[ldtkId] = engineId;
}

int Ldtk_m::getEngineIdFromLdtkId(const std::string& ldtkId) {
    auto it = ldtkIdToEngineId_.find(ldtkId);
    return (it != ldtkIdToEngineId_.end()) ? it->second : -1;
}
