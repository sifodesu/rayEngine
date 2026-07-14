#pragma once

// The Clock class represents a timer for measuring elapsed time.
class Clock
{
public:
    // Starts a new lap and returns the elapsed time since the last lap.
    static double lap();

    // Gets the elapsed time since the last lap without starting a new lap.
    static double getLap();

    // Used by deterministic capture suites: rendering and CRT wall-clock
    // state continue, while gameplay simulation remains on one exact frame.
    static void setSimulationPaused(bool paused);
    static bool isSimulationPaused();

private:
    static double lastTime; // The time of the last lap.
    static double delta;    // The elapsed time since the last lap.
    static bool paused;
};
