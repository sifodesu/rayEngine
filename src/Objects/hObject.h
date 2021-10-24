#pragma once
#include <string>
#include <map>
#include <vector>
#include <raylib.h>
#include <nlohmann/json.hpp>

#include "bullet.h"

class HObject : public GObject {
public:
    HObject(nlohmann::json obj) : GObject(obj["ID"]), hp_(5) {
        if (obj.contains("hp")){
            hp_ = obj["hp"];
        }
    }
    int getHP() { return hp_; };
    void setHP(int hp) { hp_ = hp; }
    void changeHP(int delta) { hp_ += delta; };
    virtual void onCollision(GObject* obj) {
        Bullet* bullet = dynamic_cast<Bullet*>(obj);
        if (bullet != nullptr) {
            changeHP(-bullet->dmg_);
        }
    }

protected:
    int hp_;
};