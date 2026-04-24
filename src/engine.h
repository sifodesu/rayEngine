#pragma once
#include "object_m.h"
#include "texture_m.h"

class Engine {
public:
    Engine();
    void game_loop();
    ~Engine();

private:
    void loadGameContent();
    void unloadGameContent();
    void reloadGame();
    void render();
};
