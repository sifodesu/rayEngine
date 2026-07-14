#include "clock.h"
#include "raylib.h"

double Clock::lastTime = GetTime();
double Clock::delta = 0;
bool Clock::paused = false;

double Clock::lap() {
    double now = GetTime();
    delta = paused ? 0.0 : now - lastTime;
    lastTime = now;
    return delta;
}

double Clock::getLap() {
    return delta;
}

void Clock::setSimulationPaused(bool shouldPause) {
    paused = shouldPause;
    delta = 0.0;
    lastTime = GetTime();
}

bool Clock::isSimulationPaused() {
    return paused;
}
