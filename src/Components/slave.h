#pragma once
#include "objComponent.h"
#include "gObject.h"

class Slave : public ObjComponent {
public:
    Slave(GObject* s) : slave(s);
    void trigger() {
        s->trigger();
    }


private:
    GObject* slave;
};