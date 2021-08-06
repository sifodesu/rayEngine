#include "gObject.h"

GObject::GObject(int ID) : id(ID) {}


void GObject::addComponent(ObjComponent* c) {
    components.push_back(c);
}

void GObject::routine() {
    for (auto c : components) {
        c->routine();
    }
}

GObject::~GObject() {
    for (auto c : components)
        delete c;
}