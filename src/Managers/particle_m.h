#pragma once

#include "raylib.h"
#include "rigidBody.h"

class Particle_m {
public:
    struct Params {
        bool enabled = true;
        float density = 1.0f;
        float sizeScale = 1.0f;
        float lifetimeScale = 1.0f;
        float dustSpeedScale = 1.0f;
        float waterSpeedScale = 1.0f;
        float gravityScale = 1.0f;

        float landDustMinFallSpeed = 70.0f;
        float waterSplashMinFallSpeed = 80.0f;
        float stillWaterTouchGrace = 0.06f;
        float waterSplashCooldown = 0.18f;
        float waterfallTouchCooldown = 0.075f;

        int jumpDustCount = 8;
        int landDustBaseCount = 8;
        int landDustStrengthCount = 7;
        int splashBaseCount = 10;
        int splashStrengthCount = 6;
        int waterfallTouchCount = 5;

        Color dustColor = {255, 255, 255, 255};
        Color dustHighlightColor = {255, 255, 255, 255};
        Color waterColor = {164, 232, 255, 255};
        Color foamColor = {220, 250, 255, 255};
    };

    static void update(float dt);
    static void draw();
    static void clear();
    static int activeCount();
    static Params& params();
    static void resetParams();

    static void emitJumpDust(Rectangle source, GravityDirection gravityDir);
    static void emitLandDust(Rectangle source, GravityDirection gravityDir, float strength);
    static void emitWaterSplash(Rectangle source, Rectangle waterRect, GravityDirection gravityDir, float strength);
    static void emitWaterfallTouch(Rectangle source, Rectangle waterfallRect);
};
