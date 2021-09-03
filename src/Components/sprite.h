#pragma once
#include <string>
#include "objComponent.h"
#include "clock.h"
#include "raylib.h"
#include "nlohmann/json.hpp"

//Arbitrary design: each Sprite is ONE animation.
class Sprite : public ObjComponent {
public:
    Sprite(std::string filename, int nb_frames = 1, int speed = 0);
    Sprite(nlohmann::json obj);
    void draw(Vector2 pos);
    void routine();
    void stop(int frame = 0); //freeze the animation on a specific frame
    Vector2 getFrameDim();

private:
    void updateIndex();
    Clock clock_;
    Texture2D sprite_sheet_;
    std::string filename_;
    int nb_frames_;
    int speed_;
    int index_;
    double ttl_frame_;
};