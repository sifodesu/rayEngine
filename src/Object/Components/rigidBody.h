#pragma once
#include "raylib.h"
#include "objComponent.h"

class RigidBody : public ObjComponent {
private:
    Rectangle surface;
    Vector2 speed;
    bool solid;
    bool contact;
};