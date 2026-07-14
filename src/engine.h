#pragma once

class Engine {
public:
    Engine(int argc = 0, char** argv = nullptr);
    void game_loop();
    int exitCode() const;
    ~Engine();

private:
    void loadGameContent();
    void unloadGameContent();
    void reloadGame();
    void render();
};
