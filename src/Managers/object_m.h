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
    static std::string level_name_;
    static int idCounter;
};