#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>

#include "gObject.h"
#include "definitions.h"

class Object_m {
public:
    static void loadBlueprints();
    static void loadLevel(std::string level_name);
    static void unloadAll();

    static void routine();

private:
    static void load(std::string path, std::map<int, GObject*>& container);
    static std::map<int, GObject*> blueprints_;	// ents which cannot be placed on a map
    static std::map<int, GObject*> level_ents_;	// ents of the current level
    
    static std::string level_name_;
};