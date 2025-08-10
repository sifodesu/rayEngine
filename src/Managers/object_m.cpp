#include "object_m.h"
#include "sprite.h"
#include "rigidBody.h"
#include "basicEnt.h"
#include "character.h"

using namespace std;

std::map<int, std::unique_ptr<GObject>> Object_m::level_ents_;  // ents of the current level
std::map<int, std::unique_ptr<GObject>> Object_m::level_tiles_; // won't call routines on them
int Object_m::idCounter = 0;

std::string Object_m::level_name_;

int Object_m::genID()
{
    return ++idCounter;
}

GObject* Object_m::createFromSpawn(const SpawnData& data)
{
    std::unique_ptr<GObject> obj;
    if (data.type == "tile" || data.type == "basic") {
        obj = std::make_unique<BasicEnt>(data);
    } else if (data.type == "character") {
        obj = std::make_unique<Character>(data);
    } else {
        return nullptr;
    }
    obj->layer_ = data.layer;
    GObject* raw = obj.get();
    if (data.type == "tile")
        level_tiles_.emplace(data.id, std::move(obj));
    else
        level_ents_.emplace(data.id, std::move(obj));
    return raw;
}

 

void Object_m::deleteObj(int id)
{
    level_ents_.erase(id);
}

void Object_m::routine()
{
    int nb_ents = level_ents_.size() + level_tiles_.size();

    DrawText(std::to_string(nb_ents).c_str(), 200, 10, 20, BLACK);

    vector<int> toDelete;
    for (auto &[id, obj] : level_ents_)
    {
        obj->routine();
        if (obj->to_delete_)
        {
            toDelete.push_back(id);
        }
    }

    for (int id : toDelete)
    {
        deleteObj(id);
    }
}

void Object_m::unload()
{
    level_ents_.clear();
    level_tiles_.clear();
}

void Object_m::clearTiles()
{
    // Safely destroy tiles and let CollisionRect dtor remove from quadtree
    level_tiles_.clear();
}