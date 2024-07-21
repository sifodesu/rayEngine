#pragma once
#include <string>
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "Quadtree.h"
#include "gObject.h"

// Json fields:
// int(x y w h) bool(solid)
class CollisionRect {
public:
    CollisionRect(nlohmann::json obj, GObject* father);
    void setCoord(Vector2 pos);
    void setDims(Vector2 dims);
    void setSurface(Rectangle surf) { surface_ = surf; }
    Vector2 getCoord() { return { surface_.x, surface_.y }; }
    Vector2 getCenterCoord() { return { surface_.x + surface_.width / 2, surface_.y + surface_.height / 2 }; }
    Rectangle& getSurface() { return surface_; }
    Vector2 getDims() { return { surface_.width, surface_.height }; }

    static std::vector<CollisionRect*> query(Rectangle rect, bool _is_static, bool with_solid);
    std::vector<CollisionRect*> getCollisions(bool with_static, bool with_solid) {
        return query(surface_, with_static, with_solid);
    }
    void setSolid(bool solid) { solid_ = solid; }
    bool isSolid() { return solid_; }
    void add()
    {
        if (is_static)
        {
            quad_static.add({pool_id_, surface_});
        }
        else
        {
            quad_dynamic.add({pool_id_, surface_});
        }
    }
    void remove()
    {
        if (is_static)
        {
            quad_static.remove({pool_id_, surface_});
        }
        else
        {
            quad_dynamic.remove({pool_id_, surface_});
        }
    }

    GObject* getFather() { return father_; }
    int getId() { return pool_id_; }

    bool is_static;
protected:
    static Quadtree quad_static;
    static Quadtree quad_dynamic;
    static std::map<int, CollisionRect*> pool;
    Rectangle surface_;
    bool solid_;
    int pool_id_;

    GObject* father_;
};