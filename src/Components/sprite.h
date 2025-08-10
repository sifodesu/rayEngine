#pragma once
#include <string>
#include "raylib.h"
#include "clock.h"

class Sprite {
public:
    Sprite(const std::string& filename, Rectangle rect = Rectangle{0, 0, 32, 32});
    void draw(Vector2 pos);
    void routine();
    void setTint(Color tint) { tint_ = tint; };
    Color getTint() { return tint_; };

private:
    void updateIndex();
    Texture2D sprite_sheet_;
    std::string filename_;
    Rectangle source_; // portion of the spritesheet
    CLITERAL(Color) tint_;
};