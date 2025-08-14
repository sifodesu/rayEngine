#pragma once
#include <string>
#include "raylib.h"
#include "clock.h"

class Sprite {
public:
    Sprite(const std::string& filename, Rectangle rect = Rectangle{0, 0, 32, 32});
    Sprite(const struct SpriteDesc& desc);
    void draw(Vector2 pos);
    void routine();
    void setTint(Color tint) { tint_ = tint; };
    Color getTint() { return tint_; };
    void setFlipX(bool v) { flipX_ = v; }
    void setFlipY(bool v) { flipY_ = v; }
    bool getFlipX() const { return flipX_; }
    bool getFlipY() const { return flipY_; }
    void freeze(bool value) {
        is_frozen_ = value;
    }

private:
    Texture2D sprite_sheet_;
    std::string filename_;
    Rectangle source_; // portion of the spritesheet
    CLITERAL(Color) tint_;

    // Animation
    int nb_frames_{1};
    int frame_padding_{0};
    float anim_speed_fps_{0.0f};
    bool is_frozen_{false};
    float time_acc_{0.0f};
    int current_frame_{0};

    // Flipping
    bool flipX_{false};
    bool flipY_{false};
};