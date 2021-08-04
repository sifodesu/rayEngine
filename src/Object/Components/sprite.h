#include "objComponent.h"
#include "clock.h"

class Sprite : ObjComponent {
public:
    void routine();

private:
    Clock clock;
};