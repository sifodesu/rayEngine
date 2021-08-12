#include <typeinfo>
#include <iostream>
#include "gObject.h"

using namespace std;

GObject::GObject(int ID) : id(ID) {

}


void GObject::addComponent(ObjComponent* c) {
    if (!c) {
        cout << "ERROR: Trying to add null component" << endl;
        return;
    }
    components[typeid(*c).name()].push_back(c);
}

void GObject::routine() {
    
}

void GObject::trigger(const char* type) {
    // for (auto c : components) {
    //     c->routine();
    // }
}

GObject::~GObject() {
    // for (auto c : components)
        // delete c;
}