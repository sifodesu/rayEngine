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
#include "fog.h"
#include "playerClone.h"
#include "cloneTrigger.h"
#include "particle_m.h"
#include "light_m.h"
#include <algorithm>
#include <cmath>
#include <set>

using namespace std;

std::map<int, std::unique_ptr<GObject>> Object_m::level_ents_;  // ents of the current level
std::map<int, std::unique_ptr<GObject>> Object_m::level_tiles_; // won't call routines on them
int Object_m::idCounter = 0;
Rectangle Object_m::active_room_{0.0f, 0.0f, (float)NATIVE_RES_WIDTH, (float)NATIVE_RES_HEIGHT};
bool Object_m::active_room_initialized_ = false;
std::map<int, SpawnData> Object_m::initial_spawns_;

std::string Object_m::level_name_;

namespace {

bool rectsOverlap(Rectangle a, Rectangle b)
{
    return a.x < b.x + b.width &&
           a.x + a.width > b.x &&
           a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

bool sameRoom(Rectangle a, Rectangle b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

bool isAuthoredSpawn(const SpawnData& data)
{
    return data.isTileInstance || data.ldtk.iid.has_value();
}

Rectangle spawnRect(const SpawnData& data)
{
    if (data.physics.collision.has_value()) {
        return data.physics.collision->rect;
    }
    return Rectangle{0.0f, 0.0f, 0.0f, 0.0f};
}

} // namespace

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
        case EntityType::PlayerClone:
            obj = std::make_unique<PlayerClone>(data);
            break;
        case EntityType::CloneTrigger:
            obj = std::make_unique<CloneTrigger>(data);
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
        case EntityType::Fog:
            obj = std::make_unique<Fog>(data);
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
    obj->configureLight(data);
    GObject* raw = obj.get();
    if (isAuthoredSpawn(data)) {
        initial_spawns_[data.id] = data;
    }
    if (data.entityType == EntityType::Tile || data.isTileInstance)
        level_tiles_.emplace(data.id, std::move(obj));
    else
        level_ents_.emplace(data.id, std::move(obj));
    return raw;
}

Rectangle Object_m::roomFromPoint(Vector2 point)
{
    const float roomW = static_cast<float>(NATIVE_RES_WIDTH);
    const float roomH = static_cast<float>(NATIVE_RES_HEIGHT);
    float gx = std::floor(point.x / roomW);
    float gy = std::floor(point.y / roomH);
    return Rectangle{gx * roomW, gy * roomH, roomW, roomH};
}

bool Object_m::isPlayerObject(GObject* obj)
{
    if (!obj) return false;
    if (RigidBody* target = Raycam_m::getTarget()) {
        return target->getFather() == obj;
    }
    return dynamic_cast<Character*>(obj) != nullptr;
}

bool Object_m::spawnOverlapsRoom(const SpawnData& data, Rectangle room)
{
    Rectangle rect = spawnRect(data);
    if (rect.width <= 0.0f || rect.height <= 0.0f) return false;
    return rectsOverlap(rect, room);
}

bool Object_m::objectIsActive(GObject* obj, Rectangle room)
{
    if (!obj) return false;
    if (isPlayerObject(obj)) return true;

    CollisionRect* body = obj->getCollisionBody();
    if (body) return rectsOverlap(body->getSurface(), room);

    Rectangle rect = obj->getRect();
    if (rect.width <= 0.0f || rect.height <= 0.0f) return false;
    return rectsOverlap(rect, room);
}

void Object_m::eraseObj(int id)
{
    auto entIt = level_ents_.find(id);
    if (entIt != level_ents_.end()) {
        if (isPlayerObject(entIt->second.get())) return;
        Portal::cancelTransit(entIt->second.get());
        level_ents_.erase(entIt);
        return;
    }

    auto tileIt = level_tiles_.find(id);
    if (tileIt != level_tiles_.end()) {
        Portal::cancelTransit(tileIt->second.get());
        level_tiles_.erase(tileIt);
    }
}

void Object_m::resetRoom(Rectangle room)
{
    Portal::releaseAllDisabledTiles();

    std::vector<int> runtimeToDelete;
    for (auto& [id, obj] : level_ents_) {
        if (!obj || isPlayerObject(obj.get())) continue;
        if (initial_spawns_.find(id) != initial_spawns_.end()) continue;
        if (objectIsActive(obj.get(), room)) runtimeToDelete.push_back(id);
    }
    for (int id : runtimeToDelete) {
        eraseObj(id);
    }

    std::vector<SpawnData> toRespawn;
    for (auto& [id, spawn] : initial_spawns_) {
        if (spawn.entityType == EntityType::Character) continue;
        if (spawn.entityType == EntityType::Fog) continue;
        if (spawnOverlapsRoom(spawn, room)) {
            toRespawn.push_back(spawn);
        }
    }

    for (const SpawnData& spawn : toRespawn) {
        eraseObj(spawn.id);
    }
    for (const SpawnData& spawn : toRespawn) {
        createFromSpawn(spawn);
    }

    Portal::setupPortalLinks();
}

void Object_m::updateActiveRoom()
{
    GObject* player = nullptr;
    if (RigidBody* target = Raycam_m::getTarget()) {
        player = target->getFather();
    }
    if (!player) {
        for (auto& [id, obj] : level_ents_) {
            if (isPlayerObject(obj.get())) {
                player = obj.get();
                break;
            }
        }
    }

    Rectangle nextRoom = Raycam_m::getRayCam().getRect();
    if (player) {
        if (active_room_initialized_ && Portal::isEntityInTransit(player)) {
            nextRoom = active_room_;
        } else if (CollisionRect* body = player->getCollisionBody()) {
            nextRoom = roomFromPoint(body->getCenterCoord());
        }
    } else {
        Camera2D& cam = Raycam_m::getCam();
        nextRoom = roomFromPoint(cam.target);
    }

    if (!active_room_initialized_) {
        active_room_ = nextRoom;
        active_room_initialized_ = true;
        return;
    }

    if (!sameRoom(active_room_, nextRoom)) {
        active_room_ = nextRoom;
        resetRoom(active_room_);
    }
}

void Object_m::deleteObj(int id)
{
    eraseObj(id);
}

void Object_m::routine()
{
    int nb_ents = level_ents_.size() + level_tiles_.size();

    // DrawText(std::to_string(nb_ents).c_str(), 200, 10, 20, BLACK);

    vector<int> toDelete;
    updateActiveRoom();
    const Rectangle activeRoom = active_room_;

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

    std::set<int> activeIds;
    std::vector<int> routineIds;
    for (auto& [id, obj] : level_ents_) {
        if (!objectIsActive(obj.get(), activeRoom)) continue;
        activeIds.insert(id);
        routineIds.push_back(id);
    }

    for (int id : routineIds) {
        auto it = level_ents_.find(id);
        if (it == level_ents_.end()) continue;
        GObject* obj = it->second.get();
        if (!obj || !objectIsActive(obj, activeRoom)) continue;
        obj->routine();
        if (obj->to_delete_)
        {
            toDelete.push_back(id);
        }
    }

    Portal::updateTransits();

    // Apply kill rules from rigidbody swept contacts (pre-correction physics contacts)
    for (auto& [id, obj] : level_ents_) {
        if (activeIds.find(id) == activeIds.end()) continue;
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
        std::vector<CollisionRect*> bodies = CollisionRect::query(activeRoom); // include non-solid
        const size_t n = bodies.size();
        for (size_t i = 0; i < n; ++i) {
            CollisionRect* aBody = bodies[i];
            if (!aBody) continue;
            GObject* a = aBody->getFather();
            if (!objectIsActive(a, activeRoom)) continue;
            Rectangle aRect = aBody->getSurface();
            for (size_t j = i + 1; j < n; ++j) {
                CollisionRect* bBody = bodies[j];
                if (!bBody) continue;
                GObject* b = bBody->getFather();
                if (!objectIsActive(b, activeRoom)) continue;
                Rectangle bRect = bBody->getSurface();
                if (!CheckCollisionRecs(aRect, bRect)) continue;
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
    Fog::clear();
    Particle_m::clear();
    level_ents_.clear();
    level_tiles_.clear();
    initial_spawns_.clear();
    active_room_initialized_ = false;
    Light_m::clear();
}

void Object_m::clearTiles()
{
    Portal::releaseAllDisabledTiles();
    // Safely destroy tiles and let CollisionRect dtor remove from quadtree
    level_tiles_.clear();
}
