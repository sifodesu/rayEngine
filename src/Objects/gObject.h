#pragma once
#include <string>
#include <map>
#include <vector>
#include <raylib.h>
#include "objComponent.h"
#include <nlohmann/json.hpp>

class GObject {
public:
    GObject(const int id) : id_(id), to_delete_(false) {}
    virtual ~GObject() {}
    virtual void routine() {}
    virtual void trigger() {}
    virtual void draw() {}
    virtual void onCollision(GObject*) {}

    const int id_;
    bool to_delete_;
};

