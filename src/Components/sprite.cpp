#include <iostream>
#include "sprite.h"
#include "texture_m.h"
#include "shader_m.h"
#include "spawn.h"
#include <cmath>
#include <algorithm>

Sprite::Sprite(const std::string& filename, Rectangle rect)
    : filename_(filename), tint_(WHITE) {
    sprite_sheet_ = Texture_m::getTexture(filename);
    // Treat provided rect as single frame
    frameRects_.push_back(rect);
}

Sprite::Sprite(const SpriteDesc& desc)
    : filename_(desc.filename), tint_(desc.tint) {
    sprite_sheet_ = Texture_m::getTexture(filename_);
    flipX_ = desc.flipX;
    flipY_ = desc.flipY;
    glitched_ = desc.glitched;
    // Copy explicit frames (new system only)
    frameRects_ = desc.frameRects;
    frameDurations_ = desc.frameDurations;
    uniformFrameDuration_ = desc.defaultFrameDuration;
    if (frameRects_.empty()) {
        // Fallback: single frame at origin if nothing specified
        frameRects_.push_back({0,0,(float)sprite_sheet_.width,(float)sprite_sheet_.height});
    }
}

SpriteGlitchParams& Sprite::glitchParams() {
    static SpriteGlitchParams params;
    return params;
}

void Sprite::resetGlitchParams() {
    glitchParams() = SpriteGlitchParams{};
}

bool Sprite::beginGlitchShader(Rectangle src) {
    if (!glitched_ || !Shader_m::has("glitch") || sprite_sheet_.width <= 0 || sprite_sheet_.height <= 0) {
        return false;
    }

    Shader shader = Shader_m::get("glitch");
    SpriteGlitchParams& params = glitchParams();

    float srcX2 = src.x + src.width;
    float srcY2 = src.y + src.height;
    float uvMin[2] = {
        std::min(src.x, srcX2) / (float)sprite_sheet_.width,
        std::min(src.y, srcY2) / (float)sprite_sheet_.height
    };
    float uvMax[2] = {
        std::max(src.x, srcX2) / (float)sprite_sheet_.width,
        std::max(src.y, srcY2) / (float)sprite_sheet_.height
    };
    float frameSize[2] = {
        std::max(std::fabs(src.width), 1.0f),
        std::max(std::fabs(src.height), 1.0f)
    };
    float time = (float)GetTime();

    int loc = GetShaderLocation(shader, "time");
    if (loc >= 0) SetShaderValue(shader, loc, &time, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "intensity");
    if (loc >= 0) SetShaderValue(shader, loc, &params.intensity, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "speed");
    if (loc >= 0) SetShaderValue(shader, loc, &params.speed, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "pixelShift");
    if (loc >= 0) SetShaderValue(shader, loc, &params.pixelShift, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "colorShift");
    if (loc >= 0) SetShaderValue(shader, loc, &params.colorShift, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "orientationJitter");
    if (loc >= 0) SetShaderValue(shader, loc, &params.orientationJitter, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "blockFlip");
    if (loc >= 0) SetShaderValue(shader, loc, &params.blockFlip, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "bandFrequency");
    if (loc >= 0) SetShaderValue(shader, loc, &params.bandFrequency, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "seed");
    if (loc >= 0) SetShaderValue(shader, loc, &params.seed, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "frameUvMin");
    if (loc >= 0) SetShaderValue(shader, loc, uvMin, SHADER_UNIFORM_VEC2);
    loc = GetShaderLocation(shader, "frameUvMax");
    if (loc >= 0) SetShaderValue(shader, loc, uvMax, SHADER_UNIFORM_VEC2);
    loc = GetShaderLocation(shader, "frameSizePx");
    if (loc >= 0) SetShaderValue(shader, loc, frameSize, SHADER_UNIFORM_VEC2);

    BeginShaderMode(shader);
    return true;
}

void Sprite::routine() {
    if (is_frozen_) return;
    float dt = static_cast<float>(Clock::getLap());
    size_t frameCount = frameRects_.size();
    if (frameCount > 1) {
        if (forceUniformDuration_) {
            time_acc_ += dt;
            while (time_acc_ >= forcedUniformDuration_) {
                time_acc_ -= forcedUniformDuration_;
                current_frame_ = (current_frame_ + 1) % (int)frameCount;
            }
        } else if (!frameDurations_.empty() && frameDurations_.size() == frameCount) {
            time_acc_ += dt;
            while (time_acc_ > frameDurations_[current_frame_]) {
                time_acc_ -= frameDurations_[current_frame_];
                current_frame_ = (current_frame_ + 1) % (int)frameCount;
            }
        } else { // uniform duration
            time_acc_ += dt;
            while (time_acc_ >= uniformFrameDuration_) {
                time_acc_ -= uniformFrameDuration_;
                current_frame_ = (current_frame_ + 1) % (int)frameCount;
            }
        }
    }
}

void Sprite::resetAnimation() {
    time_acc_ = 0.0f;
    current_frame_ = 0;
}

void Sprite::setForcedUniformFrameDuration(float seconds) {
    forceUniformDuration_ = true;
    forcedUniformDuration_ = std::max(seconds, 0.001f);
}

void Sprite::clearForcedUniformFrameDuration() {
    forceUniformDuration_ = false;
}

void Sprite::draw(Vector2 pos) {
    // Snap to integer pixels to avoid subpixel sampling artifacts
    pos.x = std::roundf(pos.x);
    pos.y = std::roundf(pos.y);
    Rectangle src = frameRects_.empty()? Rectangle{0,0,0,0} : frameRects_[current_frame_ % frameRects_.size()];
    
    // Slightly inset the source to avoid sampling neighboring texels due to float precision
    const float inset = 0.01f;
    bool willFlipX = flipX_;
    bool willFlipY = flipY_;
    if (!willFlipX) { src.x += inset; src.width -= 2*inset; }
    if (!willFlipY) { src.y += inset; src.height -= 2*inset; }
    // Apply flipping by negating width/height and adjusting origin
    if (flipX_) {
        src.x += src.width; // shift to the right edge
        src.width = -src.width;
    }
    if (flipY_) {
        src.y += src.height; // shift to the bottom edge
        src.height = -src.height;
    }

    bool shaderActive = beginGlitchShader(src);
    DrawTextureRec(sprite_sheet_, src, pos, tint_);
    if (shaderActive) EndShaderMode();
}

void Sprite::draw(Rectangle targetRect) {
    // Snap target rectangle to integer pixels to avoid subpixel sampling artifacts
    targetRect.x = std::roundf(targetRect.x);
    targetRect.y = std::roundf(targetRect.y);
    targetRect.width = std::roundf(targetRect.width);
    targetRect.height = std::roundf(targetRect.height);
    
    Rectangle src = frameRects_.empty()? Rectangle{0,0,0,0} : frameRects_[current_frame_ % frameRects_.size()];
    
    // Slightly inset the source to avoid sampling neighboring texels due to float precision
    const float inset = 0.01f;
    bool willFlipX = flipX_;
    bool willFlipY = flipY_;
    if (!willFlipX) { src.x += inset; src.width -= 2*inset; }
    if (!willFlipY) { src.y += inset; src.height -= 2*inset; }
    // Apply flipping by negating width/height and adjusting origin
    if (flipX_) {
        src.x += src.width; // shift to the right edge
        src.width = -src.width;
    }
    if (flipY_) {
        src.y += src.height; // shift to the bottom edge
        src.height = -src.height;
    }
    
    // Use DrawTexturePro to stretch the sprite to fit the target rectangle
    // Set origin to center of sprite for proper rotation
    Vector2 origin = {targetRect.width / 2.0f, targetRect.height / 2.0f};
    Rectangle rotatedTarget = targetRect;
    rotatedTarget.x += origin.x; // Offset to center
    rotatedTarget.y += origin.y;
    bool shaderActive = beginGlitchShader(src);
    DrawTexturePro(sprite_sheet_, src, rotatedTarget, origin, rotation_, tint_);
    if (shaderActive) EndShaderMode();
}
