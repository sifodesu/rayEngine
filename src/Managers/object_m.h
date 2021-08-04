#include <iostream>
#include <string>
#include <map>
#include "Quadtree.h"
#include "gObject.h"

class Object_m {
public:
    Object_m();

private:
    std::map<int, GObject> blueprints;	// ents which cannot be placed on a map
    std::map<int, GObject> levelEnts;	// ents of the current level
   
    quadtree::Quadtree quadEnts;
    
    std::string levelName;
};