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
#include "plateforme.h"
#include "oneWayPlatform.h"
#include "friablePlatform.h"
#include "shooter.h"
#include "portal.h"
#include "projectile.h"
#include "modelEnt.h"
#include "water.h"

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
    switch (data.entityType) {
        case EntityType::Tile:
        case EntityType::Basic:
            obj = std::make_unique<BasicEnt>(data);
            break;
        case EntityType::Character:
            obj = std::make_unique<Character>(data);
            break;
        case EntityType::Model3D:
            obj = std::make_unique<ModelEnt>(data);
            break;
        case EntityType::UpgradePickup:
            obj = std::make_unique<UpgradePickup>(data);
            break;
        case EntityType::Checkpoint:
            obj = std::make_unique<Checkpoint>(data);
            break;
        case EntityType::Kill:
            obj = std::make_unique<Kill>(data);
            break;
        case EntityType::Projectile:
            obj = std::make_unique<Projectile>(data);
            break;
        case EntityType::Pano:
            obj = std::make_unique<Pano>(data);
            break;
        case EntityType::Receptacle:
            obj = std::make_unique<Receptacle>(data);
            break;
        case EntityType::Plateforme:
            obj = std::make_unique<Plateforme>(data);
            break;
        case EntityType::OneWayPlatform:
            obj = std::make_unique<OneWayPlatform>(data);
            break;
        case EntityType::FriablePlatform:
            obj = std::make_unique<FriablePlatform>(data);
            break;
        case EntityType::Water:
            obj = std::make_unique<Water>(data);
            break;
        case EntityType::Shooter:
            obj = std::make_unique<Shooter>(data);
            break;
        case EntityType::Portal:
            obj = std::make_unique<Portal>(data);
            break;
        default:
            return nullptr;
    }
    obj->layer_ = data.layer;
    obj->setKillOnCollision(data.interaction.killOnCol.value_or(false));
    GObject* raw = obj.get();
    if (data.entityType == EntityType::Tile || data.isTileInstance)
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

    // std::vector<CollisionRect*> to_routine = CollisionRect::query(Raycam_m::getRayCam().getRect());
    // for (CollisionRect* obj_rec : to_routine)
    // {
    //     GObject* obj = obj_rec->getFather();
    //     obj->routine();
        
    //     if (obj->to_delete_)
    //     {
    //         toDelete.push_back(obj->id_);
    //     }
    // }

    for (auto &[id, obj] : level_ents_)
    {
        obj->routine();
        if (obj->to_delete_)
        {
            toDelete.push_back(id);
        }
    }

    Portal::updateTransits();

    // Apply kill rules from rigidbody swept contacts (pre-correction physics contacts)
    for (auto& [id, obj] : level_ents_) {
        CollisionRect* body = obj->getCollisionBody();
        if (!body) continue;
        RigidBody* rigid = dynamic_cast<RigidBody*>(body);
        if (!rigid) continue;

        for (GObject* other : rigid->getSweepContactOwners()) {
            if (!other || other == obj.get()) continue;

            obj->applyKillOnCollision(other);
            other->applyKillOnCollision(obj.get());
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
                if (!aBody->isRenderProxy() && Portal::isEntityInTransit(a)) continue;
                if (!bBody->isRenderProxy() && Portal::isEntityInTransit(b)) continue;
                a->onCollision(b);
                a->applyKillOnCollision(b);
                b->onCollision(a);
                b->applyKillOnCollision(a);
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
    Portal::clearTransits();
    Portal::releaseAllDisabledTiles();
    Water::clear();
    level_ents_.clear();
    level_tiles_.clear();
}

void Object_m::clearTiles()
{
    Portal::releaseAllDisabledTiles();
    // Safely destroy tiles and let CollisionRect dtor remove from quadtree
    level_tiles_.clear();
}
