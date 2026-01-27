#pragma once
#include "gObject.h"
#include <functional>

// Component that handles killing characters on collision
class KillComponent {
public:
    KillComponent() = default;
    ~KillComponent() = default;

    // Checks if the colliding object is a character and respawns it
    // Returns true if a character was killed
    bool onCollision(GObject* other);
    
};
