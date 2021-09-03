#include <raylib.h>
#include <nlohmann/json.hpp>
#include "gObject.h"
#include "rigidBody.h"
#include "sprite.h"
#include "runes.h"

class Character : public GObject {
public:
    Character(nlohmann::json obj);
    void routine();
    void draw(Vector2 pos);

private:
    RigidBody* body_;
    Sprite* sprite_;
    // Runes* runes_;
};
