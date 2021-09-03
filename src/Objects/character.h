#include <raylib.h>
#include "gObject.h"
#include "rigidBody.h"
#include "sprite.h"
#include <nlohmann/json.hpp>

class Character : public GObject {
public:
    Character(nlohmann::json  obj);
    void routine();
    void draw(Vector2 pos);

private:
    RigidBody* body_;
    Sprite* sprite_;
};
