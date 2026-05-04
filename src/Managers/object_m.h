#pragma once
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <memory>
#include "spawn.h"

#include "gObject.h"
#include "definitions.h"

class Object_m {
public:
    static void unload();
    static void routine();
    static int genID();
    static std::map<int, std::unique_ptr<GObject>> level_ents_; // ents of the current level
    static std::map<int, std::unique_ptr<GObject>> level_tiles_;
    static void clearTiles();

    static GObject* createFromSpawn(const SpawnData& data);

private:
    static void deleteObj(int id);
    static void eraseObj(int id);
    static void updateActiveRoom();
    static void resetRoom(Rectangle room);
    static bool objectIsActive(GObject* obj, Rectangle room);
    static bool spawnOverlapsRoom(const SpawnData& data, Rectangle room);
    static bool isPlayerObject(GObject* obj);
    static Rectangle roomFromPoint(Vector2 point);
    static Rectangle active_room_;
    static bool active_room_initialized_;
    static std::map<int, SpawnData> initial_spawns_;
    static std::string level_name_;
    static int idCounter;
};
