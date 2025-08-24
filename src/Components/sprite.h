#pragma once
#include <string>
#include "raylib.h"
#include "clock.h"
#include <vector>

class Sprite {
public:
    Sprite(const std::string& filename, Rectangle rect = Rectangle{0, 0, 32, 32}); // rect becomes first frame
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
    CLITERAL(Color) tint_;

    // Animation
    std::vector<Rectangle> frameRects_; // explicit frames
    std::vector<float> frameDurations_; // seconds per frame (optional)
    float uniformFrameDuration_{0.2f}; // used if frameDurations_ empty and multiple frames
    bool is_frozen_{false};
    float time_acc_{0.0f};
    int current_frame_{0};

    // Flipping
    bool flipX_{false};
    bool flipY_{false};
};