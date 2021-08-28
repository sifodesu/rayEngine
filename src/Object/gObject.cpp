#include <typeinfo>
#include <iostream>
#include "gObject.h"
#include "definitions.h"

using namespace std;

GObject::GObject(int ID) : id(ID) {

}


void GObject::addComponent(ObjComponent* c) {
    if (!c) {
        cout << "ERROR: Trying to add null component" << endl;
        return;
    }
    components[t(*c)].push_back(c);
}

void GObject::routine() {
    for (auto& [type, vec] : components) {
        for (auto& comp : vec){
            comp->routine();
        }
    }
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