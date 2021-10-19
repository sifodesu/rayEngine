#pragma once
#include <string>
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "objComponent.h"
#include "Quadtree.h"
#include "gObject.h"


class collisionRect : public ObjComponent {
public:
    collisionRect(nlohmann::json obj, GObject* father);
    void setCoord(Vector2 pos);
    void setSurface(Rectangle surf) { surface_ = surf; }
    Vector2 getCoord() { return { surface_.x, surface_.y }; }
    Vector2 getCenterCoord() { return { surface_.x + surface_.width / 2, surface_.y + surface_.height / 2 }; }
    Rectangle getSurface() { return surface_; }
    Vector2 getDims() { return { surface_.width, surface_.height }; }

    static std::vector<collisionRect*> query(Rectangle rect, bool force_solid = true);
    std::vector<collisionRect*> getCollisions(bool with_solid = true) {
        return query(surface_, with_solid);
    }
    void setSolid(bool solid) { solid_ = solid; }
    bool isSolid() { return solid_; }
    void add() {
        if (in_quad_) {
            quad.add({ pool_id_, surface_ });
        }
    }
    void remove() {
        if (in_quad_) {
            quad.remove({ pool_id_, surface_ });
        }
    };
    ~collisionRect() {
        remove();
        pool.erase(pool_id_);
    }
    GObject* getFather() { return father_; }
    int getId() { return pool_id_; }

protected:
    bool in_quad_;
    static Quadtree quad;
    static std::map<int, collisionRect*> pool;
    Rectangle surface_;
    bool solid_;
    int pool_id_;

    GObject* father_;
};