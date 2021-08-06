#pragma once
#include "object_m.h"
#include "texture_m.h"

class Engine {
public:
    Engine(const int screenWidth = 800, const int screenHeight = 450);
    void game_loop();
    ~Engine();

private:
    Object_m* object_m;
    Texture_m* texture_m;
};