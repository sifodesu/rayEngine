#pragma once
#include <string>
#include <raylib.h>
#include "Quadtree.h"
#include "gObject.h"
#include "spawn.h"

// Json fields:
// int(x y w h) bool(solid)
class CollisionRect {
public:
    CollisionRect(const CollisionDesc& desc, GObject* father);
    virtual ~CollisionRect();
    void setCoord(Vector2 pos);
    void setDims(Vector2 dims);
    void setSurface(Rectangle surf) { surface_ = surf; }
    Vector2 getCoord() { return { surface_.x, surface_.y }; }
    Vector2 getCenterCoord() { return { surface_.x + surface_.width / 2, surface_.y + surface_.height / 2 }; }
    Rectangle& getSurface() { return surface_; }
    Vector2 getDims() { return { surface_.width, surface_.height }; }

    // Single quadtree query: returns all rects intersecting rect; filter by solidity if requested
    static std::vector<CollisionRect*> query(Rectangle rect, bool with_solid = false);
    std::vector<CollisionRect*> getCollisions(bool with_solid) {
        return query(surface_, with_solid);
    }
    void setSolid(bool solid) { solid_ = solid; }
    bool isSolid() { return solid_; }
    void add()
    {
        quad_.add({pool_id_, surface_});
    }
    void remove()
    {
        quad_.remove({pool_id_, surface_});
    }

    GObject* getFather() { return father_; }
    int getId() { return pool_id_; }
protected:
    static Quadtree quad_;
    static std::map<int, CollisionRect*> pool;
    Rectangle surface_;
    bool solid_;
    int pool_id_;

    GObject* father_;
};