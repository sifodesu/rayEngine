#include "gObject.h"

void GObject::routine() {
    for (auto c : components) {
        c->routine();
    }
}