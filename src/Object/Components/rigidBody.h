#pragma once
#include <map>
#include <string>
#include <functional>
#include "raylib.h"
#include "gObject.h"

class RigidBody : public ObjComponent {
public:


private:
    GObject* father;
    Rectangle surface;
    Vector2 speed;
    bool solid;

};