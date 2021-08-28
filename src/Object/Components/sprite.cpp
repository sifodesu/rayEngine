#include "sprite.h"
#include "texture_m.h"
using json = nlohmann::json;

Sprite::Sprite(std::string filename, int nb_frames, int speed)
    : filename_(filename_), nb_frames_(nb_frames), speed_(speed), index_(0),
    ttl_frame_(0) {

    sprite_sheet_ = Texture_m::getTexture(filename);
}

Sprite::Sprite(json obj) : index_(0), ttl_frame_(0) {
    if (obj.contains("filename"))
        filename_ = obj["filename"];

    if (obj.contains("nb_frames"))
        nb_frames_ = obj["nb_frames"];

    if (obj.contains("speed"))
        speed_ = obj["speed"];

    sprite_sheet_ = Texture_m::getTexture(filename_);
}

void Sprite::routine() {
    if (speed_)
        updateIndex();
}

void Sprite::updateIndex() {
    ttl_frame_ += clock_.getLap();
    if (ttl_frame_ > speed_) {
        ttl_frame_ -= speed_;
        index_ = (index_ + 1) % nb_frames_;
    }
}

void Sprite::draw(Vector2 pos) {
    Rectangle source{ (float)index_ * sprite_sheet_.width / nb_frames_, 0.0f,
                        (float)sprite_sheet_.width, (float)sprite_sheet_.height };
    DrawTextureRec(sprite_sheet_, source, pos, WHITE);
}

void Sprite::stop(int frame) {
    index_ = frame;
    speed_ = 0;
}