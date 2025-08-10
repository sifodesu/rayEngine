#include <iostream>
#include "sprite.h"
#include "texture_m.h"

Sprite::Sprite(const std::string& filename, Rectangle rect)
    : filename_(filename), source_(rect), tint_(WHITE) {

    sprite_sheet_ = Texture_m::getTexture(filename);
}

void Sprite::routine() {
    
}

void Sprite::draw(Vector2 pos) {
    DrawTextureRec(sprite_sheet_, source_, pos, tint_);
}
