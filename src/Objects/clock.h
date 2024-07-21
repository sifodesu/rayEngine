#pragma once

// The Clock class represents a timer for measuring elapsed time.
class Clock
{
public:
    // Starts a new lap and returns the elapsed time since the last lap.
    static double lap();

    // Gets the elapsed time since the last lap without starting a new lap.
    static double getLap();

private:
    static double lastTime; // The time of the last lap.
    static double delta;    // The elapsed time since the last lap.
};