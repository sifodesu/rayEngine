#pragma once

#define t(...) typeid(__VA_ARGS__).name()
class ObjComponent {
public:
    virtual void routine() {};
};