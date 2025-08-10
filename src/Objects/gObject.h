#pragma once
#include <string>
#include <map>
#include <vector>
#include <raylib.h>
#include <nlohmann/json.hpp>

class GObject {
public:
    explicit GObject(const int id) : id_(id), to_delete_(false), layer_(0) {}
    virtual ~GObject() {}
    virtual void routine() {}
    virtual void trigger() {}
    virtual void draw() {}
    virtual void onCollision(GObject*) {}
    virtual Rectangle getRect() { return Rectangle{0.0f, 0.0f, 1.0f, 1.0f}; }

    const int id_;
    // int x, y;
    bool to_delete_;
    int layer_;
};
