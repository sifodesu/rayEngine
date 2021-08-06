#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include "Quadtree.h"
#include "gObject.h"

class Object_m {
public:
    Object_m();
    void load(std::string path, std::map<int, GObject*>& container);
    ~Object_m();

private:
    std::map<int, GObject*> blueprints;	// ents which cannot be placed on a map
    std::map<int, GObject*> levelEnts;	// ents of the current level
   
    Quadtree quadEnts;
    
    std::string levelName;
};