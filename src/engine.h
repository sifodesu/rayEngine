#pragma once
#include "object_m.h"
#include "texture_m.h"
#include "raycam.h"

class Engine {
public:
    Engine();
    void game_loop();
    ~Engine();

private:
    void render();
    Raycam camera_;
    int screenWidth_;
    int screenHeight_;
};