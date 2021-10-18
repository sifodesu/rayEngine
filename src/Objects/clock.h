#pragma once

class Clock {
public:
    static double lap();
    static double getLap();
    
private:
    static double lastTime;
    static double delta;
};