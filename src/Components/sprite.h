#pragma once
#include <string>
#include "clock.h"
#include "raylib.h"
#include "nlohmann/json.hpp"

// Json fields:
// filename
class Sprite {
public:
    Sprite(std::string filename, Rectangle rect = {0, 0, 32, 32});
    Sprite(nlohmann::json obj);
    void draw(Vector2 pos);
    void routine();
    void setTint(Color tint) { tint_ = tint; };
    Color getTint() { return tint_; };

private:
    void updateIndex();
    Texture2D sprite_sheet_;
    std::string filename_;
    CLITERAL(Color) tint_;
    Rectangle source_; // portion of the spritesheet
};