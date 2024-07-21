#include <iostream>
#include "sprite.h"
#include "texture_m.h"
using json = nlohmann::json;
using namespace std;

Sprite::Sprite(std::string filename, Rectangle rect)
    : filename_(filename), source_(rect), tint_(WHITE) {

    sprite_sheet_ = Texture_m::getTexture(filename);
}

Sprite::Sprite(json obj) : tint_(WHITE), source_({0, 0, 32, 32}) {
    if (!obj.contains("sprite")) {
        cout << "ERROR: no sprite from json" << endl;
        return;
    }
    obj = obj["sprite"];
    if (obj.contains("filename"))
        filename_ = obj["filename"];

    if (obj.contains("source"))
        source_ = {obj["source"]["x"], obj["source"]["y"], obj["source"]["w"], obj["source"]["h"]};

    sprite_sheet_ = Texture_m::getTexture(filename_);
}

void Sprite::routine() {
    
}

void Sprite::draw(Vector2 pos) {
    DrawTextureRec(sprite_sheet_, source_, pos, tint_);
}
