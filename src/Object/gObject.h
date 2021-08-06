#pragma once
#include <string>
#include <vector>
#include "objComponent.h"

class GObject {
public:
    GObject(int ID);
    ~GObject();
    void addComponent(ObjComponent* c);
    
    void routine();
    const int id;
    std::string target;
    std::vector<std::string> targetnames;

private:
    std::vector<ObjComponent*> components;
};