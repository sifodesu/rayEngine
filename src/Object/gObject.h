#pragma once
#include <string>
#include <map>
#include <vector>
#include "objComponent.h"

class GObject {
public:
    GObject(int ID);
    ~GObject();
    void addComponent(ObjComponent* c);

    void routine();
    void trigger(const char* type = "");
    const int id;

private:
    std::map<const char*, std::vector<ObjComponent*>> components;   //<typeid(), components matching>
};