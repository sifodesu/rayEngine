#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "gObject.h"
#include "bullet.h"


class Pattern {
public:
    static void circle(Vector2 center, int nb_bullet = 15, int speed = 100);
    static void line(Vector2 origin, Vector2 direction, int nb_bullet = 15, int speed = 100);
};