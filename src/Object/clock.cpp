#include "clock.h"
#include "raylib.h"

Clock::Clock() {
    lastTime = GetTime();
}

double Clock::getLap() {
    double now = GetTime();
    double ret = now - lastTime;
    lastTime = now;
    return ret;
}