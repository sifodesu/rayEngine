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

    const int id_;
    bool to_delete_;
};

class HObject : public GObject {
public:
    HObject(nlohmann::json obj) : GObject(obj["ID"]), hp_(5) {
        if (obj.contains("hp"))
            hp_ = obj["hp"];
    }
    int getHP() { return hp_; };
    void setHP(int hp) { hp_ = hp; }
    void changeHP(int delta) { hp_ += delta; };

private:
    int hp_;
};