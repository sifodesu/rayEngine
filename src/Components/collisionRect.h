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
    void setSurface(Rectangle surf);
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
        if (!in_quadtree_) {
            stored_node_ = {pool_id_, surface_};
            quad_.add(stored_node_);
            in_quadtree_ = true;
        }
    }
    void remove()
    {
        if (in_quadtree_) {
            // Use the exact node that was stored when added
            if (quad_.contains(stored_node_)) {
                quad_.remove(stored_node_);
            }
            in_quadtree_ = false;
        }
    }

    GObject* getFather() { return father_; }
    int getId() { return pool_id_; }
protected:
    static Quadtree quad_;
    static std::map<int, CollisionRect*> pool;
    Rectangle surface_;
    bool solid_;
    int pool_id_;
    bool in_quadtree_;  // Track if this object is currently in the quadtree
    quadNode stored_node_;  // Store the exact node that was added to the quadtree

    GObject* father_;
};