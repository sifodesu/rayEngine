#pragma once

class Fps3D_m {
public:
    static void initFromCurrentLevel();
    static void unload();
    static void toggle();
    static bool isEnabled();
    static bool isVisible();
    static bool isFully3D();
    static void update(float dt);
    static void draw();
};
