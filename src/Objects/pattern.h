#pragma once
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "gObject.h"
#include "bullet.h"


class Pattern {
    public:
    static void circle(Vector2 pos);
    static void line(Vector2 origin, Vector2 direction);
};