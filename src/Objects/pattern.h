#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include "gObject.h"
#include "bullet.h"
#include "rigidBullet.h"

class Pattern {
public:
    static void circle(RigidBullet* bp, int nb_bullets = 15);
    static void line(RigidBullet* bp, Vector2 origin, Vector2 direction, double center_acc = 0, int nb_bullet = 15);
};