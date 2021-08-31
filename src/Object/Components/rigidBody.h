#pragma once
#include <vector>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include <raylib.h>

#include "gObject.h"
#include "Quadtree.h"
#include "clock.h"

class RigidBody : public ObjComponent {
public:
    RigidBody(Rectangle surface, bool solid=true);
    ~RigidBody();
    RigidBody(nlohmann::json obj, GObject* father);

    Vector2 getCoord();
    void setSpeed(Vector2 speed);
    void routine();

    static Quadtree quad;
    static std::vector<RigidBody*> pool;
    static std::vector<RigidBody*> query(Rectangle rect);

    GObject* father_;
private:
    Rectangle surface_;
    Vector2 speed_;
    bool solid_;
    int pool_id_;
    Clock clock_;
};