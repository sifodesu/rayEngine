#pragma once
#include <map>
#include <string>
#include <functional>
#include "raylib.h"
#include "gObject.h"

class RigidBody : public ObjComponent {
public:

    void onCollide(RigidBody* collided);
    //get type of collided
    //if entry in map, execute on collided father
    // if (typeid(arg) == typeid(const int)) {
        //ID of instance
        //map[id]()
    // }
    // if (typeid(arg) == typeid(string)) {
        //type of object
        //map[type]()
// }
    // }
    //map <type/id, func>
    

private:
    GObject* father;
    Rectangle surface;
    Vector2 speed;
    bool solid;
    bool contact;

    // std::map<int, std::function<void(GObject*)>> collideSignals;

};