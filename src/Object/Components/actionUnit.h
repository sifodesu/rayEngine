#include "objComponent.h"

class ActionUnit : public ObjComponent {
public:
    virtual void trigger() = 0;
};