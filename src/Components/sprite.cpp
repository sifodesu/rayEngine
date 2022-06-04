#include <iostream>
#include <sstream>
#include "sprite.h"
#include "texture_m.h"
using json = nlohmann::json;
using namespace std;

Sprite::Sprite(std::string filename, int nb_frames, int speed)
    : filename_(filename), nb_frames_(nb_frames), speed_(speed), index_(0),
    ttl_frame_(0), tint_(WHITE) {

    sprite_sheet_ = Texture_m::getTexture(filename);
}

Sprite::Sprite(json obj) : index_(0), ttl_frame_(0), speed_(0), nb_frames_(1), tint_(WHITE) {
    if (obj.contains("fieldInstances")) {
        obj = obj["fieldInstances"];
        for (auto& field : obj) {
            if (field["__identifier"] == "sprite_filename") {
                filename_ = (field["__value"]);
                std::stringstream ss(filename_);
                std::string s;
                while (std::getline(ss, s, '/')) {
                    if (s.empty())
                        break;
                    filename_ = s;
                }
            }
            if (field["__identifier"] == "sprite_nb_frames") {
                nb_frames_ = field["__value"];
            }
            if (field["__identifier"] == "sprite_speed") {
                speed_ = field["__value"];
            }
        }
    }
    //legacy
    else if (obj.contains("sprite")) {
        obj = obj["sprite"];
        if (obj.contains("filename")) {
            filename_ = obj["filename"];
            std::stringstream ss(filename_);
            std::string s;
            while (std::getline(ss, s, '/')) {
                if (s.empty())
                    break;
                filename_ = s;
            }
        }

        if (obj.contains("nb_frames"))
            nb_frames_ = obj["nb_frames"];

        if (obj.contains("speed"))
            speed_ = obj["speed"];
    }
    else {
        // cout << "ERROR: no sprite from json" << endl;
        return;
    }


    sprite_sheet_ = Texture_m::getTexture(filename_);
}


void Sprite::routine() {
    if (speed_)
        updateIndex();
}

void Sprite::updateIndex() {
    ttl_frame_ += Clock::getLap() * 1000;
    if (ttl_frame_ > speed_) {
        ttl_frame_ -= speed_;
        index_ = (index_ + 1) % nb_frames_;
    }
}

void Sprite::draw(Vector2 pos) {
    Rectangle source{ (float)index_ * sprite_sheet_.width / nb_frames_, 0.0f,
                        (float)sprite_sheet_.width / nb_frames_, (float)sprite_sheet_.height };
    DrawTextureRec(sprite_sheet_, source, pos, tint_);
}

void Sprite::stop(int frame) {
    index_ = frame;
    speed_ = 0;
}

Vector2 Sprite::getFrameDim() {
    return { (float)sprite_sheet_.width / nb_frames_, (float)sprite_sheet_.height };
}

void Sprite::setTint(CLITERAL(Color) tint) {
    tint_ = tint;
}