#pragma once
#include <string>
#include <map>
#include <vector>
#include <raylib.h>
#include "objComponent.h"


class GObject {
public:
    GObject(const int id): id_(id) {}
    
    virtual void routine() {}
    virtual void trigger() {}
    virtual void draw(Vector2 pos) {}
    
    const int id_;
};