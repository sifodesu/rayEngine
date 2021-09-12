#pragma once
#include <raylib.h>
#include <deque>
#include <vector>
#include "sprite.h"

class Runes {
public:
    static void init();
    static void newRune();
    static void routine();
    static void draw(Vector2 pos);
    static bool getBullet();

private:
    static std::deque<int> queue_;
    static std::vector<Sprite*> sprites_;
    static int to_shoot_;
};