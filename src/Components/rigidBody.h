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
    void setCurve(double curve);
    Vector2 getSpeed();
    void setSolid(bool solid);
    bool isSolid();
    void routine();
    std::vector<RigidBody*> getCollisions(bool with_solid = true);


    static std::vector<RigidBody*> query(Rectangle rect, bool force_solid = false);

    GObject* father_;

private:
    static Quadtree quad;
    static std::map<int, RigidBody*> pool;
    Rectangle surface_;
    Vector2 speed_;
    bool solid_;
    int pool_id_;
    Clock clock_;
    double acceleration_;
    double curve_;

    void fixSpeed();    //set to 0 if collision
};