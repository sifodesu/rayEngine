#include "object_m.h"
#include "sprite.h"
#include "rigidBody.h"
#include "basicEnt.h"
#include "character.h"
#include "upgradePickup.h"
#include "collisionRect.h"
#include "raycam_m.h"
#include <raylib.h>
#include "kill.h"
#include "checkpoint.h"
#include "pano.h"
#include "receptacle.h"

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
    } else if (data.type == "Character") {
        obj = std::make_unique<Character>(data);
    } else if (data.type.rfind("upgrade_", 0) == 0) { // any upgrade_* type
        obj = std::make_unique<UpgradePickup>(data);
    } else if (data.type == "Checkpoint") {
        obj = std::make_unique<Checkpoint>(data);
    } else if (data.type == "Kill") {
        obj = std::make_unique<Kill>(data);
    } else if (data.type == "Pano") {
        obj = std::make_unique<Pano>(data);
    } else if (data.type == "Receptacle") {
        obj = std::make_unique<Receptacle>(data);
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

    // DrawText(std::to_string(nb_ents).c_str(), 200, 10, 20, BLACK);

    vector<int> toDelete;
    for (auto &[id, obj] : level_ents_)
    {
        obj->routine();
        if (obj->to_delete_)
        {
            toDelete.push_back(id);
        }
    }

    // Dispatch collisions for objects within camera view
    {
        auto camRect = Raycam_m::getRayCam().getRect();
        std::vector<CollisionRect*> bodies = CollisionRect::query(camRect); // include non-solid
        const size_t n = bodies.size();
        for (size_t i = 0; i < n; ++i) {
            CollisionRect* aBody = bodies[i];
            if (!aBody) continue;
            Rectangle aRect = aBody->getSurface();
            for (size_t j = i + 1; j < n; ++j) {
                CollisionRect* bBody = bodies[j];
                if (!bBody) continue;
                Rectangle bRect = bBody->getSurface();
                if (!CheckCollisionRecs(aRect, bRect)) continue;
                GObject* a = aBody->getFather();
                GObject* b = bBody->getFather();
                if (!a || !b || a == b) continue;
                a->onCollision(b);
                b->onCollision(a);
            }
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