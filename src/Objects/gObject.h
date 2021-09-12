#pragma once
#include <string>
#include <map>
#include <vector>
#include <raylib.h>
#include "objComponent.h"


class GObject {
public:
    GObject(const int id): id_(id) {}
    virtual ~GObject() {}
    virtual void routine() {}
    virtual void trigger() {}
    virtual void draw() {}
    
    const int id_;
};