#include "ldtk_m.h"

// Standard library
#include <fstream>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

// Third party
#include <nlohmann/json.hpp>

// Project includes
#include "object_m.h"
#include "spawn.h"
#include "definitions.h"
#include "sprite_m.h"
#include "portal_m.h"

using json = nlohmann::json;
using namespace std;

// ============================================================================
// ANONYMOUS NAMESPACE - INTERNAL HELPERS
// ============================================================================

namespace {

struct LdtkBackground {
    Rectangle levelRect{0, 0, 0, 0};
    std::optional<Color> color;
    Texture2D texture{};
    std::string bgPos{"Unscaled"};
    float pivotX{0.5f};
    float pivotY{0.5f};
};

std::vector<LdtkBackground> backgrounds;

map<int, vector<string>> collectEntityDefTags(const json& root) {
    map<int, vector<string>> tagsByDefUid;
    if (!root.contains("defs") || !root["defs"].contains("entities")) return tagsByDefUid;

    for (const auto& def : root["defs"]["entities"]) {
        int uid = def.value("uid", -1);
        if (uid < 0) continue;

        vector<string> tags;
        if (def.contains("tags") && def["tags"].is_array()) {
            for (const auto& tag : def["tags"]) {
                if (tag.is_string()) tags.push_back(tag.get<string>());
            }
        }
        tagsByDefUid[uid] = std::move(tags);
    }

    return tagsByDefUid;
}

bool entityHasTag(const json& entity, const map<int, vector<string>>& tagsByDefUid, const string& tag) {
    int defUid = entity.value("defUid", -1);
    auto it = tagsByDefUid.find(defUid);
    if (it == tagsByDefUid.end()) return false;
    return std::find(it->second.begin(), it->second.end(), tag) != it->second.end();
}

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

std::filesystem::path projectBasePath(const string& filename) {
    std::filesystem::path projectPath = std::filesystem::path(LDTK_PATH) / filename;
    std::filesystem::path parent = projectPath.parent_path();
    return parent.empty() ? std::filesystem::path(LDTK_PATH) : parent;
}

std::filesystem::path resolveProjectRelativePath(const string& filename, const string& relPath) {
    std::filesystem::path path(relPath);
    if (path.is_absolute()) return path;
    return projectBasePath(filename) / path;
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

Color tintFromLayerOpacity(float opacity) {
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    Color tint = WHITE;
    tint.a = static_cast<unsigned char>(std::round(opacity * 255.0f));
    return tint;
}

Color tintWithAlphaMultiplier(Color tint, float opacity) {
    opacity = std::clamp(opacity, 0.0f, 1.0f);
    tint.a = static_cast<unsigned char>(std::round(static_cast<float>(tint.a) * opacity));
    return tint;
}

void spawnTile(const string& tilesetFile, int tileSize, int sx, int sy, 
               int px, int py, int layer, Color tint = WHITE, const string& typeStr = "") {
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
    sd.tint = tint;
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

void appendIdValue(const json& value, vector<string>& out) {
    if (value.is_null()) return;

    if (value.is_string()) {
        vector<string> ids = parseCommaSeparatedIds(value.get<string>());
        out.insert(out.end(), ids.begin(), ids.end());
        return;
    }

    if (value.is_object()) {
        if (value.contains("entityIid") && value["entityIid"].is_string()) {
            out.push_back(value["entityIid"].get<string>());
        }
        return;
    }

    if (value.is_array()) {
        for (const auto& item : value) {
            appendIdValue(item, out);
        }
    }
}

vector<string> parseIdListValue(const json& value) {
    vector<string> ids;
    appendIdValue(value, ids);
    ids.erase(std::remove(ids.begin(), ids.end(), string{}), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
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

bool rectsOverlap(Rectangle a, Rectangle b) {
    return a.x < b.x + b.width &&
           a.x + a.width > b.x &&
           a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

int drawOrderForLdtkLayer(int displayIndex, int layerCount) {
    return std::max(0, layerCount - 1 - displayIndex);
}

float safePivot(float pivot) {
    if (!std::isfinite(pivot)) return 0.5f;
    return std::clamp(pivot, 0.0f, 1.0f);
}

void drawTextureCover(const Texture2D& texture, Rectangle levelRect, float pivotX, float pivotY) {
    float texW = static_cast<float>(texture.width);
    float texH = static_cast<float>(texture.height);
    if (texW <= 0.0f || texH <= 0.0f || levelRect.width <= 0.0f || levelRect.height <= 0.0f) return;

    float scale = std::max(levelRect.width / texW, levelRect.height / texH);
    float srcW = levelRect.width / scale;
    float srcH = levelRect.height / scale;
    Rectangle source{
        (texW - srcW) * safePivot(pivotX),
        (texH - srcH) * safePivot(pivotY),
        srcW,
        srcH
    };
    DrawTexturePro(texture, source, levelRect, Vector2{0, 0}, 0.0f, WHITE);
}

void drawTextureContain(const Texture2D& texture, Rectangle levelRect, float pivotX, float pivotY) {
    float texW = static_cast<float>(texture.width);
    float texH = static_cast<float>(texture.height);
    if (texW <= 0.0f || texH <= 0.0f || levelRect.width <= 0.0f || levelRect.height <= 0.0f) return;

    float scale = std::min(levelRect.width / texW, levelRect.height / texH);
    float dstW = texW * scale;
    float dstH = texH * scale;
    Rectangle dest{
        levelRect.x + (levelRect.width - dstW) * safePivot(pivotX),
        levelRect.y + (levelRect.height - dstH) * safePivot(pivotY),
        dstW,
        dstH
    };
    Rectangle source{0, 0, texW, texH};
    DrawTexturePro(texture, source, dest, Vector2{0, 0}, 0.0f, WHITE);
}

void drawTextureUnscaled(const Texture2D& texture, Rectangle levelRect, float pivotX, float pivotY) {
    float texW = static_cast<float>(texture.width);
    float texH = static_cast<float>(texture.height);
    if (texW <= 0.0f || texH <= 0.0f) return;

    Vector2 pos{
        levelRect.x + (levelRect.width - texW) * safePivot(pivotX),
        levelRect.y + (levelRect.height - texH) * safePivot(pivotY)
    };
    DrawTextureV(texture, pos, WHITE);
}

void drawTextureRepeat(const Texture2D& texture, Rectangle levelRect, Rectangle viewRect) {
    float texW = static_cast<float>(texture.width);
    float texH = static_cast<float>(texture.height);
    if (texW <= 0.0f || texH <= 0.0f || levelRect.width <= 0.0f || levelRect.height <= 0.0f) return;

    float drawMinX = std::max(levelRect.x, viewRect.x);
    float drawMinY = std::max(levelRect.y, viewRect.y);
    float drawMaxX = std::min(levelRect.x + levelRect.width, viewRect.x + viewRect.width);
    float drawMaxY = std::min(levelRect.y + levelRect.height, viewRect.y + viewRect.height);
    if (drawMinX >= drawMaxX || drawMinY >= drawMaxY) return;

    float firstX = levelRect.x + std::floor((drawMinX - levelRect.x) / texW) * texW;
    float firstY = levelRect.y + std::floor((drawMinY - levelRect.y) / texH) * texH;

    for (float y = firstY; y < drawMaxY; y += texH) {
        for (float x = firstX; x < drawMaxX; x += texW) {
            Rectangle dest{x, y, texW, texH};
            Rectangle src{0, 0, texW, texH};

            if (dest.x < levelRect.x) {
                float delta = levelRect.x - dest.x;
                src.x += delta;
                src.width -= delta;
                dest.x += delta;
                dest.width -= delta;
            }
            if (dest.y < levelRect.y) {
                float delta = levelRect.y - dest.y;
                src.y += delta;
                src.height -= delta;
                dest.y += delta;
                dest.height -= delta;
            }
            float overflowX = (dest.x + dest.width) - (levelRect.x + levelRect.width);
            if (overflowX > 0.0f) {
                src.width -= overflowX;
                dest.width -= overflowX;
            }
            float overflowY = (dest.y + dest.height) - (levelRect.y + levelRect.height);
            if (overflowY > 0.0f) {
                src.height -= overflowY;
                dest.height -= overflowY;
            }

            if (src.width > 0.0f && src.height > 0.0f && dest.width > 0.0f && dest.height > 0.0f) {
                DrawTexturePro(texture, src, dest, Vector2{0, 0}, 0.0f, WHITE);
            }
        }
    }
}

std::optional<LdtkBackground> makeLevelBackground(const json& level, const string& filename) {
    LdtkBackground bg;
    bg.levelRect = Rectangle{
        static_cast<float>(level.value("worldX", 0)),
        static_cast<float>(level.value("worldY", 0)),
        static_cast<float>(level.value("pxWid", 0)),
        static_cast<float>(level.value("pxHei", 0))
    };
    if (level.contains("bgPos") && level["bgPos"].is_string()) {
        bg.bgPos = level["bgPos"].get<string>();
    }
    bg.pivotX = level.value("bgPivotX", 0.5f);
    bg.pivotY = level.value("bgPivotY", 0.5f);

    string colorString;
    if (level.contains("bgColor") && level["bgColor"].is_string()) {
        colorString = level["bgColor"].get<string>();
    } else if (level.contains("__bgColor") && level["__bgColor"].is_string()) {
        colorString = level["__bgColor"].get<string>();
    }
    if (!colorString.empty()) {
        Color parsed{WHITE};
        if (parseHexColor(colorString, parsed)) bg.color = parsed;
    }

    if (level.contains("bgRelPath") && level["bgRelPath"].is_string()) {
        std::filesystem::path bgPath = resolveProjectRelativePath(filename, level["bgRelPath"].get<string>());
        if (std::filesystem::exists(bgPath)) {
            bg.texture = LoadTexture(bgPath.string().c_str());
            if (bg.texture.id > 0) {
                SetTextureFilter(bg.texture, TEXTURE_FILTER_POINT);
            }
        } else {
            cerr << "LDtk: background image not found " << bgPath.string() << '\n';
        }
    }

    if (bg.color.has_value() || bg.texture.id > 0) {
        return bg;
    }
    return std::nullopt;
}

void fillEntityFields(const json& inst, SpawnData& d, int layerGridSize, int worldX, int worldY) {
    if (!inst.contains("fieldInstances")) 
        return;

    auto ensureModelDesc = [&]() -> ModelDesc& {
        if (!d.model.has_value()) d.model = ModelDesc{};
        return *d.model;
    };

    auto ensureLightDesc = [&]() -> LightDesc& {
        if (!d.light.has_value()) d.light = LightDesc{};
        return *d.light;
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
        else if (fid == "staysActivated") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.staysActivated = f["__value"].get<bool>();
            }
        }
        else if (fid == "killOnCol") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                d.interaction.killOnCol = f["__value"].get<bool>();
            }
        }
        else if (fid == "emitLight" || fid == "lightEnabled") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureLightDesc().enabled = f["__value"].get<bool>();
            }
        }
        else if (fid == "lightRadius") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureLightDesc().radius = f["__value"].get<float>();
            }
        }
        else if (fid == "lightIntensity") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureLightDesc().intensity = f["__value"].get<float>();
            }
        }
        else if (fid == "lightColor") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                Color parsed = WHITE;
                if (parseHexColor(f["__value"].get<string>(), parsed)) {
                    ensureLightDesc().color = parsed;
                }
            }
        }
        else if (fid == "lightOffsetX") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureLightDesc().offset.x = f["__value"].get<float>();
            }
        }
        else if (fid == "lightOffsetY") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                ensureLightDesc().offset.y = f["__value"].get<float>();
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
                auto targets = parseIdListValue(f["__value"]);
                if (!targets.empty()) {
                    d.ldtk.targetIds = targets;
                }
            }
        }
        else if (fid == "buttonRefs" || fid == "buttonIds" || fid == "buttons") {
            if (f.contains("__value") && !f["__value"].is_null()) {
                auto targets = parseIdListValue(f["__value"]);
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
                      int worldX, int worldY, int layer, Color tint = WHITE) {
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
        Color tileTint = tintWithAlphaMultiplier(tint, tile.value("a", 1.0f));
        
        // Look up tile type from enum tags
        string tileType = "";
        if (tsInfo && tile.contains("t")) {
            int tileId = tile["t"].get<int>();
            auto typeIt = tsInfo->tileIdToType.find(tileId);
            if (typeIt != tsInfo->tileIdToType.end()) {
                tileType = typeIt->second;
            }
        }
        
        spawnTile(tilesetFile, tileSize, sx, sy, localPx + worldX, localPy + worldY, layer, tileTint, tileType);
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
        std::string identifier = e["__identifier"].get<string>();
        d.entityType = stringToEntityType(identifier);
        d.typeDetail = identifier;
        if (d.entityType == EntityType::Light) {
            d.light = LightDesc{};
            d.light->enabled = true;
        }
    }
    
    // Fill entity data from LDtk fields
    fillEntityFields(e, d, layerGridSize, worldX, worldY);
    SpriteDesc defaultSprite;
    if (!d.sprite.has_value() || (d.sprite->filename == defaultSprite.filename && d.sprite->frameRects.empty()))
        fillEntityTile(e, tilesetInfo, d);
    
    // Setup collision box
    if (!d.physics.collision) 
        d.physics.collision = CollisionDesc{};
    
    float entityWidth = (float)e["width"].get<int>();
    float entityHeight = (float)e["height"].get<int>();
    float pivotX = 0.0f;
    float pivotY = 0.0f;
    if (e.contains("__pivot") && e["__pivot"].is_array() && e["__pivot"].size() >= 2) {
        pivotX = e["__pivot"][0].get<float>();
        pivotY = e["__pivot"][1].get<float>();
    }

    d.physics.collision->rect = Rectangle{
        (float)(e["px"][0].get<int>() + worldX) - entityWidth * pivotX,
        (float)(e["px"][1].get<int>() + worldY) - entityHeight * pivotY,
        entityWidth,
        entityHeight
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

// Public API: import an LDtk project (.ldtk). Optionally skip character-tagged entities when doing hot reload.
void Ldtk_m::loadLevel(const string& filename, bool skipCharacters) {
    if (!strEndsWith(filename, ".ldtk")) { cerr << "LDtk: expected .ldtk file got " << filename << '\n'; return; }

    ifstream file((LDTK_PATH + filename).c_str());
    if (!file) { cerr << "LDtk: can't open project " << filename << '\n'; return; }

    json root; file >> root;
    if (!root.contains("levels") || root["levels"].empty()) { cerr << "LDtk: no levels in " << filename << '\n'; return; }

    unload();

    // Clear previous ID mapping
    if (!skipCharacters) {
        clearIdMapping();
    }
    
    currentProjectFile = filename; // track for hot reload
    auto tilesetInfo = collectTilesetInfo(root);
    auto entityDefTags = collectEntityDefTags(root);

    for (auto& level : root["levels"]) { // Each LDtk level (supports multi-level worlds)
        int worldX = level.value("worldX", 0);
        int worldY = level.value("worldY", 0);
        auto intGrid = extractIntGrid(level);

        if (auto background = makeLevelBackground(level, filename)) {
            backgrounds.push_back(*background);
        }

        const auto& layerInstances = level["layerInstances"];
        int layerIndex = 0;
        const int layerCount = static_cast<int>(layerInstances.size());
        for (auto& layer : layerInstances) {
            // LDtk exports layerInstances in display order: first is top-most,
            // last is behind. The engine draws lower layer numbers first.
            const int drawLayer = drawOrderForLdtkLayer(layerIndex, layerCount);
            string type = layer["__type"].get<string>();
            Color layerTint = tintFromLayerOpacity(layer.value("__opacity", 1.0f));
            if (type == "Tiles") { // Tile layer -> spawn tiles
                int tileSize = layer["__gridSize"].get<int>();
                int tilesetUid = layer.value("__tilesetDefUid", -1);
                string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                processTileArray(layer["gridTiles"], tileSize, tilesetUid, tilesetFile, 
                                tilesetInfo, intGrid, worldX, worldY, drawLayer, layerTint);
            } else if (type == "AutoLayer") { // AutoLayer -> spawn autolayer tiles
                int tileSize = layer["__gridSize"].get<int>();
                int tilesetUid = layer.value("__tilesetDefUid", -1);
                string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                processTileArray(layer["autoLayerTiles"], tileSize, tilesetUid, tilesetFile, 
                                tilesetInfo, intGrid, worldX, worldY, drawLayer, layerTint);
            } else if (type == "IntGrid") { // IntGrid layer -> spawn auto-generated tiles if any
                if (layer.contains("autoLayerTiles") && !layer["autoLayerTiles"].empty()) {
                    int tileSize = layer["__gridSize"].get<int>();
                    int tilesetUid = layer.value("__tilesetDefUid", -1);
                    string tilesetFile = basename(layer.value("__tilesetRelPath", string{}));
                    processTileArray(layer["autoLayerTiles"], tileSize, tilesetUid, tilesetFile, 
                                    tilesetInfo, intGrid, worldX, worldY, drawLayer, layerTint);
                }
            } else if (type == "Entities") { // Entity layer -> spawn entities
                int entityGridSize = layer["__gridSize"].get<int>();
                for (auto& e : layer["entityInstances"]) {
                    if (skipCharacters && entityHasTag(e, entityDefTags, "chara")) {
                        continue;
                    }
                    spawnEntity(e, worldX, worldY, drawLayer, tilesetInfo, entityGridSize);
                }
            }
            ++layerIndex;
        }
    }
    
    // After all entities are loaded, setup portal links
    Portal_m::setupLinks();
}

void Ldtk_m::drawBackgrounds(const Rectangle& viewRect) {
    for (const LdtkBackground& bg : backgrounds) {
        if (!rectsOverlap(bg.levelRect, viewRect)) continue;

        if (bg.color.has_value()) {
            DrawRectangleRec(bg.levelRect, *bg.color);
        }

        if (bg.texture.id <= 0) continue;

        if (bg.bgPos == "Cover") {
            drawTextureCover(bg.texture, bg.levelRect, bg.pivotX, bg.pivotY);
        } else if (bg.bgPos == "Contain") {
            drawTextureContain(bg.texture, bg.levelRect, bg.pivotX, bg.pivotY);
        } else if (bg.bgPos == "Repeat") {
            drawTextureRepeat(bg.texture, bg.levelRect, viewRect);
        } else if (bg.bgPos == "CoverDirty") {
            Rectangle source{0, 0, static_cast<float>(bg.texture.width), static_cast<float>(bg.texture.height)};
            DrawTexturePro(bg.texture, source, bg.levelRect, Vector2{0, 0}, 0.0f, WHITE);
        } else {
            drawTextureUnscaled(bg.texture, bg.levelRect, bg.pivotX, bg.pivotY);
        }
    }
}

void Ldtk_m::unload() {
    for (LdtkBackground& bg : backgrounds) {
        if (bg.texture.id > 0) {
            UnloadTexture(bg.texture);
            bg.texture = Texture2D{};
        }
    }
    backgrounds.clear();
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
