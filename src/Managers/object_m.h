#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <nlohmann/json.hpp>

#include "gObject.h"
#include "definitions.h"

class Object_m {
public:
    static void loadLevel(std::string level_name);
    static void unload();
    static void routine();
    static int genID();
    static std::map<int, GObject*> level_ents_;	// ents of the current level
    static std::map<int, GObject*> level_tiles_;

    static GObject* createObjJson(nlohmann::json ojs);

private:
    static void deleteObj(int id);
    static std::string level_name_;
    static int idCounter;
};