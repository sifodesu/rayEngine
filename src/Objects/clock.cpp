#include "clock.h"
#include "raylib.h"

double Clock::lastTime = GetTime();
double Clock::delta = 0;

double Clock::lap() {
    double now = GetTime();
    delta = now - lastTime;
    lastTime = now;
    return delta;
}

double Clock::getLap() {
    return delta;
}