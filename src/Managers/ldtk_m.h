#pragma once

#include "raylib.h"
#include "definitions.h"

#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <map>

/**
 * @brief LDtk Level Manager - Handles loading and parsing LDtk level files
 * 
 * This manager loads LDtk project files (.ldtk) and spawns all tiles and entities
 * into the game world. It handles:
 * - Tile layers (with collision detection via IntGrid)
 * - AutoLayers (decorative tiles)
 * - Entity layers (game objects)
 * - Entity linking system (LDtk IID to engine ID mapping)
 */
class Ldtk_m {
public:
    // ========================================================================
    // LEVEL LOADING
    // ========================================================================
    
    /**
     * @brief Load an LDtk project file and spawn all its contents
     * @param filename Name of the .ldtk file (relative to LDTK_PATH)
     * @param skipCharacters If true, skip LDtk entities tagged "chara" (useful for hot reload)
     */
    static void loadLevel(const std::string& filename, bool skipCharacters = false);

    /**
     * @brief Draw LDtk level backgrounds loaded from bgColor/bgRelPath fields
     * @param viewRect Current camera rectangle in world coordinates, used for culling
     */
    static void drawBackgrounds(const Rectangle& viewRect);

    /**
     * @brief Release LDtk-owned background textures
     */
    static void unload();

    /**
     * @brief Bounds of all currently loaded LDtk levels in world coordinates
     */
    static Rectangle getWorldBounds();
    
    /**
     * @brief Enable/disable hot reload functionality
     * @param v True to enable hot reload
     */
    static void enableHotReload(bool v) { hotReloadEnabled = v; }
    
    // ========================================================================
    // ID MAPPING SYSTEM
    // ========================================================================
    
    /**
     * @brief Get engine object ID from LDtk entity IID
     * @param ldtkId LDtk entity IID string
     * @return Engine object ID, or -1 if not found
     */
    static int getEngineIdFromLdtkId(const std::string& ldtkId);
    
    /**
     * @brief Clear the ID mapping (called before loading new level)
     */
    static void clearIdMapping();
    
    /**
     * @brief Register LDtk ID to Engine ID mapping
     * @param ldtkId LDtk entity IID
     * @param engineId Engine object ID
     */
    static void registerIdMapping(const std::string& ldtkId, int engineId);
    
private:
    static inline bool hotReloadEnabled = true;
    static inline std::filesystem::file_time_type lastWrite{};
    static inline std::string currentProjectFile{"ldtk_test.ldtk"};
    static inline std::map<std::string, int> ldtkIdToEngineId_;
    static inline Rectangle worldBounds_{0.0f, 0.0f, 0.0f, 0.0f};
    static inline bool hasWorldBounds_{false};
};
