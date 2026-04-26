#include "ldtk_m.h"

// Standard library
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>

// Third party
#include <nlohmann/json.hpp>

// Project includes
#include "object_m.h"
#include "spawn.h"
#include "definitions.h"
#include "sprite_m.h"
#include "portal.h"

using json = nlohmann::json;
using namespace std;

// ============================================================================
// ANONYMOUS NAMESPACE - INTERNAL HELPERS
// ============================================================================

namespace {

// ----------------------------------------------------------------------------
// String & Path Utilities
// ----------------------------------------------------------------------------

bool strEndsWith(const string& s, const string& suf) {
    return s.size() >= suf.size() && equal(suf.rbegin(), suf.rend(), s.rbegin());
}

string basename(const string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == string::npos) ? path : path.substr(pos + 1);
}

// ----------------------------------------------------------------------------
// Tileset Processing
// ----------------------------------------------------------------------------

struct TilesetInfo {
    string filename;
    map<int, string> tileIdToType; // Maps tile ID to EntityType string
};

map<int, TilesetInfo> collectTilesetInfo(const json& root) {
    map<int, TilesetInfo> out;
    
    if (!root.contains("defs") || !root["defs"].contains("tilesets")) 
        return out;
    
    for (auto& ts : root["defs"]["tilesets"]) {
        int uid = ts["uid"];
        if (ts["relPath"].is_null()) continue;
        
        TilesetInfo info;
        info.filename = basename(ts["relPath"].get<string>());
        if (info.filename.empty()) continue;
        
        // Collect enum tags if they exist
        if (ts.contains("enumTags") && ts["enumTags"].is_array()) {
            for (auto& tag : ts["enumTags"]) {
                if (tag.contains("enumValueId") && tag.contains("tileIds")) {
                    string typeStr = tag["enumValueId"].get<string>();
                    for (auto& tileId : tag["tileIds"]) {
                        info.tileIdToType[tileId.get<int>()] = typeStr;
                    }
                }
            }
        }
        
        out[uid] = info;
    }
    
    return out;
}

// ----------------------------------------------------------------------------
// IntGrid Collision System
// ----------------------------------------------------------------------------

struct IntGridInfo { 
    bool has = false; 
    vector<int> csv; 
    int width = 0; 
    int cell = 0; 
};

IntGridInfo extractIntGrid(const json& level) {
    IntGridInfo info;
    
    if (!level.contains("layerInstances")) 
        return info;
    
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

// ----------------------------------------------------------------------------
// Tile Spawning
// ----------------------------------------------------------------------------

void spawnTile(const string& tilesetFile, int tileSize, int sx, int sy, 
               int px, int py, int layer, const string& typeStr = "") {
    SpawnData d;
    d.id = Object_m::genID();
    d.isTileInstance = true;
    
    // Use provided type or default to Tile
    if (!typeStr.empty()) {
        d.entityType = stringToEntityType(typeStr);
        d.typeDetail = typeStr;
    } else {
        d.entityType = EntityType::Tile;
    }

    bool solid = d.entityType == EntityType::Basic ? true : false;

    d.layer = layer;
    
    // Setup sprite
    SpriteDesc sd;
    sd.filename = tilesetFile;
    sd.tint = WHITE;
    sd.frameRects.push_back({(float)sx, (float)sy, (float)tileSize, (float)tileSize});
    d.sprite = sd;
    
    // Setup collision
    Rectangle collisionRect = {(float)px, (float)py, (float)tileSize, (float)tileSize};
    d.physics.collision = CollisionDesc{collisionRect, solid};
    
    Object_m::createFromSpawn(d);
}

// ----------------------------------------------------------------------------
// Entity Field Processing
// ----------------------------------------------------------------------------

// Parse comma-separated string into vector of trimmed strings
vector<string> parseCommaSeparatedIds(const string& input) {
    vector<string> result;
    if (input.empty()) return result;
    
    size_t start = 0;
    size_t end = input.find(',');
    
    while (end != string::npos) {
        string item = input.substr(start, end - start);
        // Trim whitespace
        item.erase(0, item.find_first_not_of(" \t"));
        item.erase(item.find_last_not_of(" \t") + 1);
        if (!item.empty()) result.push_back(item);
        
        start = end + 1;
        end = input.find(',', start);
    }
    
    // Add last item
    string item = input.substr(start);
    item.erase(0, item.find_first_not_of(" \t"));
    item.erase(item.find_last_not_of(" \t") + 1);
    if (!item.empty()) result.push_back(item);
    
    return result;
}

bool parseHexColor(const string& hex, Color& out) {
    if (hex.size() != 7 && hex.size() != 9) return false;
    if (hex[0] != '#') return false;

    auto fromHex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    auto parseByte = [&](size_t i, unsigned char& dst) -> bool {
        int hi = fromHex(hex[i]);
        int lo = fromHex(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        dst = static_cast<unsigned char>((hi << 4) | lo);
        return true;
    };

    Color c{WHITE};
    if (!parseByte(1, c.r)) return false;
    if (!parseByte(3, c.g)) return false;
    if (!parseByte(5, c.b)) return false;
    c.a = 255;
    if (hex.size() == 9 && !parseByte(7, c.a)) return false;
    out = c;
    return true;
}

void fillEntityFields(const json& inst, SpawnData& d, int layerGridSize, int worldX, int worldY) {
    if (!inst.contains("fieldInstances")) 
        return;

    auto ensureModelDesc = [&]() -> ModelDesc& {
        if (!d.model.has_value()) d.model = ModelDesc{};
        return *d.model;
    };

    for (auto& f : inst["fieldInstances"]) {
        string fid = f["__identifier"];
        if (fid == "Type") {
            string val = f["__value"].get<string>();
            d.entityType = stringToEntityType(val);
            
            // Check for specific behaviors/flags in the Type enum
             d.typeDetail = val; // fallback
        }
        else if (fid == "solid") {
            if (!d.physics.collision) d.physics.collision = CollisionDesc{};
            d.physics.collision->solid = f["__value"].get<bool>();
        }
        else if (fid == "loop") {
             if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.isLoop = f["__value"].get<bool>();
            }
        }
        else if (fid == "sprite") {
            string key = f["__value"].get<string>();
            if (!d.sprite) d.sprite = SpriteDesc{};
            bool wasGlitched = d.sprite->glitched;
            
            auto tryLoad = [&](const std::string& k) { 
                if (auto meta = Sprite_m::get(k)) { 
                    *d.sprite = *meta; 
                    d.sprite->glitched = wasGlitched;
                    return true; 
                } 
                return false; 
            };
            
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
        else if (fid == "model" || fid == "modelPath") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().modelFile = f["__value"].get<string>();
            }
        }
        else if (fid == "primitive") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().primitive = stringToModelPrimitive(f["__value"].get<string>());
            }
        }
        else if (fid == "modelRotX") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().rotation.x = f["__value"].get<float>();
            }
        }
        else if (fid == "modelRotY") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().rotation.y = f["__value"].get<float>();
            }
        }
        else if (fid == "modelRotZ") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().rotation.z = f["__value"].get<float>();
            }
        }
        else if (fid == "modelScaleX") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().scale.x = f["__value"].get<float>();
            }
        }
        else if (fid == "modelScaleY") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().scale.y = f["__value"].get<float>();
            }
        }
        else if (fid == "modelScaleZ") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().scale.z = f["__value"].get<float>();
            }
        }
        else if (fid == "modelTint") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                Color parsed = WHITE;
                if (parseHexColor(f["__value"].get<string>(), parsed)) {
                    ensureModelDesc().tint = parsed;
                }
            }
        }
        else if (fid == "spinX" || fid == "spinSpeedX") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().spin.x = f["__value"].get<float>();
            }
        }
        else if (fid == "spinY" || fid == "spinSpeedY") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().spin.y = f["__value"].get<float>();
            }
        }
        else if (fid == "spinZ" || fid == "spinSpeedZ") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureModelDesc().spin.z = f["__value"].get<float>();
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
        else if (fid == "glitched") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                bool glitched = f["__value"].get<bool>();
                if (glitched) {
                    if (!d.sprite) d.sprite = SpriteDesc{};
                    d.sprite->glitched = true;
                } else if (d.sprite) {
                    d.sprite->glitched = false;
                }
            }
        }
        else if (fid == "Dialog") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.dialog = f["__value"].get<string>();
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
                if (!pts.empty()) d.interaction.pathPoints = pts;
            }
        }
        else if (fid == "enabled") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.enabled = f["__value"].get<bool>();
            }
        }
        else if (fid == "killOnCol") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.killOnCol = f["__value"].get<bool>();
            }
        }
        else if (fid == "breakTime") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.breakTime = f["__value"].get<float>();
            }
        }
        else if (fid == "fireInterval") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.fireInterval = f["__value"].get<float>();
            }
        }
        else if (fid == "projectileSpeed") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.projectileSpeed = f["__value"].get<float>();
            }
        }
        else if (fid == "projectileSprite") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.projectileSprite = f["__value"].get<string>();
            }
        }
        else if (fid == "maxRipple") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.maxRipple = f["__value"].get<int>();
            }
        }
        else if (fid == "linkId") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (f["__value"].is_object() && f["__value"].contains("entityIid")) {
                    d.ldtk.linkId = f["__value"]["entityIid"].get<string>();
                } else if (f["__value"].is_string()) {
                    d.ldtk.linkId = f["__value"].get<string>();
                }
            }
        }
        else if (fid == "targetIds") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                auto targets = parseCommaSeparatedIds(f["__value"].get<string>());
                if (!targets.empty()) {
                    d.ldtk.targetIds = targets;
                }
            }
        }
        else if (fid == "trigger") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.interaction.adiComponent) d.interaction.adiComponent = AdiComponentDesc{};
                
                std::string targetId;
                if (f["__value"].is_object() && f["__value"].contains("entityIid")) {
                    targetId = f["__value"]["entityIid"].get<string>();
                } else if (f["__value"].is_string()) {
                    targetId = f["__value"].get<string>();
                }
                
                if (!targetId.empty()) {
                    d.interaction.adiComponent->targetIds.push_back(targetId);
                    d.interaction.adiComponent->canReceiveAdi = true;
                }
            }
        }
        else if (fid == "canReceiveAdi") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.interaction.adiComponent) d.interaction.adiComponent = AdiComponentDesc{};
                d.interaction.adiComponent->canReceiveAdi = f["__value"].get<bool>();
            }
        }
        else if (fid == "canBeTriggered") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.interaction.adiComponent) d.interaction.adiComponent = AdiComponentDesc{};
                d.interaction.adiComponent->canBeTriggered = f["__value"].get<bool>();
            }
        }
        else if (fid == "adiCapacity") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.interaction.adiComponent) d.interaction.adiComponent = AdiComponentDesc{};
                d.interaction.adiComponent->maxCapacity = f["__value"].get<int>();
            }
        }
        else if (fid == "adiThreshold") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.interaction.adiComponent) d.interaction.adiComponent = AdiComponentDesc{};
                d.interaction.adiComponent->activationThreshold = f["__value"].get<int>();
            }
        }
        else if (fid == "adiTargets") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                if (!d.interaction.adiComponent) d.interaction.adiComponent = AdiComponentDesc{};
                
                auto targets = parseCommaSeparatedIds(f["__value"].get<string>());
                if (!targets.empty()) {
                    d.interaction.adiComponent->targetIds = targets;
                }
            }
        }
        else if (fid == "Gw_dir") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                std::string dirStr = f["__value"].get<string>();
                if (dirStr == "UP") {
                    d.interaction.direction = PortalDirection::UP;
                } else if (dirStr == "DOWN") {
                    d.interaction.direction = PortalDirection::DOWN;
                } else if (dirStr == "LEFT") {
                    d.interaction.direction = PortalDirection::LEFT;
                } else if (dirStr == "RIGHT") {
                    d.interaction.direction = PortalDirection::RIGHT;
                }
            }
        }
        else if (fid == "Force_gravity") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.forceGravity = f["__value"].get<bool>();
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Tile Processing Helper
// ----------------------------------------------------------------------------

void processTileArray(const json& tileArray, int tileSize, int tilesetUid, const string& tilesetFile,
                      const map<int, TilesetInfo>& tilesetInfo, const IntGridInfo& intGrid,
                      int worldX, int worldY, int layer) {
    // Get tileset info for type lookups
    const TilesetInfo* tsInfo = nullptr;
    auto tsIt = tilesetInfo.find(tilesetUid);
    if (tsIt != tilesetInfo.end()) {
        tsInfo = &tsIt->second;
    }
    
    for (auto& tile : tileArray) {
        int localPx = tile["px"][0];
        int localPy = tile["px"][1];
        int sx = tile["src"][0];
        int sy = tile["src"][1];
        
        // Look up tile type from enum tags
        string tileType = "";
        if (tsInfo && tile.contains("t")) {
            int tileId = tile["t"].get<int>();
            auto typeIt = tsInfo->tileIdToType.find(tileId);
            if (typeIt != tsInfo->tileIdToType.end()) {
                tileType = typeIt->second;
            }
        }
        
        spawnTile(tilesetFile, tileSize, sx, sy, localPx + worldX, localPy + worldY, layer, tileType);
    }
}

// ----------------------------------------------------------------------------
// Entity Spawning
// ----------------------------------------------------------------------------

void fillEntityTile(const json& e, const map<int, TilesetInfo>& tilesetInfo, SpawnData& d) {
    if (!e.contains("__tile") || !e["__tile"].is_object()) 
        return;
    
    int uid = e["__tile"]["tilesetUid"];
    if (!d.sprite) d.sprite = SpriteDesc{};
    
    auto it = tilesetInfo.find(uid);
    if (it != tilesetInfo.end()) 
        d.sprite->filename = it->second.filename;
    
    Rectangle r{
        (float)e["__tile"]["x"], 
        (float)e["__tile"]["y"], 
        (float)e["__tile"]["w"], 
        (float)e["__tile"]["h"]
    };
    
    if (d.sprite->frameRects.empty()) 
        d.sprite->frameRects.push_back(r);
    else 
        d.sprite->frameRects[0] = r;
}

void spawnEntity(const json& e, int worldX, int worldY, int layer, 
                 const map<int, TilesetInfo>& tilesetInfo, int layerGridSize) {
    SpawnData d;
    if (e.contains("__identifier")) {
        d.entityType = stringToEntityType(e["__identifier"].get<string>());
    }
    
    // Fill entity data from LDtk fields
    fillEntityFields(e, d, layerGridSize, worldX, worldY);
    SpriteDesc defaultSprite;
    if (!d.sprite.has_value() || (d.sprite->filename == defaultSprite.filename && d.sprite->frameRects.empty()))
        fillEntityTile(e, tilesetInfo, d);
    
    // Setup collision box
    if (!d.physics.collision) 
        d.physics.collision = CollisionDesc{};
    
    d.physics.collision->rect = Rectangle{
        (float)(e["px"][0].get<int>() + worldX), 
        (float)(e["px"][1].get<int>() + worldY), 
        (float)e["width"].get<int>(), 
        (float)e["height"].get<int>()
    };
    
    // Store LDtk IID for linking
    if (e.contains("iid") && e["iid"].is_string()) {
        d.ldtk.iid = e["iid"].get<string>();
    }
    
    d.id = Object_m::genID();
    d.layer = layer;
    
    // Register ID mapping
    if (d.ldtk.iid.has_value()) {
        Ldtk_m::registerIdMapping(*d.ldtk.iid, d.id);
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
    auto tilesetInfo = collectTilesetInfo(root);

    for (auto& level : root["levels"]) { // Each LDtk level (supports multi-level worlds)
        int worldX = level.value("worldX", 0);
        int worldY = level.value("worldY", 0);
        auto intGrid = extractIntGrid(level);

        int layerIndex = 0;
        for (auto& layer : level["layerInstances"]) { // Iterate draw order as provided
            string type = layer["__type"].get<string>();
            if (type == "Tiles") { // Tile layer -> spawn tiles
                int tileSize = layer["__gridSize"].get<int>();
                int tilesetUid = layer.value("__tilesetDefUid", -1);
                string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                processTileArray(layer["gridTiles"], tileSize, tilesetUid, tilesetFile, 
                                tilesetInfo, intGrid, worldX, worldY, layerIndex);
            } else if (type == "AutoLayer") { // AutoLayer -> spawn autolayer tiles
                int tileSize = layer["__gridSize"].get<int>();
                int tilesetUid = layer.value("__tilesetDefUid", -1);
                string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                processTileArray(layer["autoLayerTiles"], tileSize, tilesetUid, tilesetFile, 
                                tilesetInfo, intGrid, worldX, worldY, layerIndex);
            } else if (type == "IntGrid") { // IntGrid layer -> spawn auto-generated tiles if any
                if (layer.contains("autoLayerTiles") && !layer["autoLayerTiles"].empty()) {
                    int tileSize = layer["__gridSize"].get<int>();
                    int tilesetUid = layer.value("__tilesetDefUid", -1);
                    string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                    processTileArray(layer["autoLayerTiles"], tileSize, tilesetUid, tilesetFile, 
                                    tilesetInfo, intGrid, worldX, worldY, layerIndex);
                }
            } else if (type == "Entities" && !skipCharacters) { // Entity layer -> spawn entities
                int entityGridSize = layer["__gridSize"].get<int>();
                for (auto& e : layer["entityInstances"]) {
                    spawnEntity(e, worldX, worldY, layerIndex + 100, tilesetInfo, entityGridSize);
                }
            }
            ++layerIndex;
        }
    }
    
    // After all entities are loaded, setup portal links
    Portal::setupPortalLinks();
}


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
