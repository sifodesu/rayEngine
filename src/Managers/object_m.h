#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

#include "gObject.h"
#include "definitions.h"

enum BPE {
    BULLET
};

class Object_m {
public:
    static void loadBlueprints();
    static void loadLevel(std::string level_name);
    static void unload();
    static void routine();
    static GObject* createObj(BPE target);
    static GObject* getObj(std::string type);

    static std::map<int, GObject*> level_ents_;	// ents of the current level
    static std::map<BPE, nlohmann::json> blueprints_;	// ents which cannot be placed on a map
private:
    static GObject* createObjJson(nlohmann::json ojs);
    static int genID();
    static void deleteObj(int id);
    static std::string level_name_;
};