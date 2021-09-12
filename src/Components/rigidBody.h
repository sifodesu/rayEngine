#pragma once
#include <vector>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include <raylib.h>
#include <map>

#include "gObject.h"
#include "Quadtree.h"
#include "clock.h"

class RigidBody : public ObjComponent {
public:
    RigidBody(Rectangle surface, bool solid = true);
    ~RigidBody();
    RigidBody(nlohmann::json obj, GObject* father);

    Vector2 getCoord();
    void setCoord(Vector2 pos);
    void setSpeed(Vector2 speed);
    Vector2 getSpeed();
    void setSolid(bool solid);
    void routine();

    static Quadtree quad;
    static std::map<int, RigidBody*> pool;
    static std::vector<RigidBody*> query(Rectangle rect);

    GObject* father_;
private:
    Rectangle surface_;
    Vector2 speed_;
    bool solid_;
    int pool_id_;
    Clock clock_;
    double acceleration_;
    double curve_;
    double angle_;

    void fixSpeed();    //set to 0 if collision
};