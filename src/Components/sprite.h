#pragma once
#include <string>
#include "raylib.h"
#include "clock.h"
#include <vector>

struct SpriteGlitchParams {
    float intensity{0.85f};
    float speed{1.0f};
    float pixelShift{2.0f};
    float colorShift{1.25f};
    float orientationJitter{0.45f};
    float blockFlip{0.65f};
    float bandFrequency{14.0f};
    float seed{0.0f};
};

class Sprite {
public:
    Sprite(const std::string& filename, Rectangle rect = Rectangle{0, 0, 32, 32}); // rect becomes first frame
    Sprite(const struct SpriteDesc& desc);
    void draw(Vector2 pos);
    void draw(Rectangle targetRect);  // Draw sprite stretched to fit target rectangle
    void routine();
    void setTint(Color tint) { tint_ = tint; };
    Color getTint() { return tint_; };
    void setFlipX(bool v) { flipX_ = v; }
    void setFlipY(bool v) { flipY_ = v; }
    bool getFlipX() const { return flipX_; }
    bool getFlipY() const { return flipY_; }
    void setGlitched(bool value) { glitched_ = value; }
    bool isGlitched() const { return glitched_; }
    void setRotation(float rotation) { rotation_ = rotation; } // rotation in degrees
    float getRotation() const { return rotation_; }
    int getFrameCount() const { return (int)frameRects_.size(); }
    void resetAnimation();
    void setForcedUniformFrameDuration(float seconds);
    void clearForcedUniformFrameDuration();
    void freeze(bool value) {
        is_frozen_ = value;
    }

    static SpriteGlitchParams& glitchParams();
    static void resetGlitchParams();

private:
    bool beginGlitchShader(Rectangle src);

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
    bool forceUniformDuration_{false};
    float forcedUniformDuration_{0.1f};

    // Flipping
    bool flipX_{false};
    bool flipY_{false};
    bool glitched_{false};
    
    // Rotation
    float rotation_{0.0f}; // rotation in degrees
};
