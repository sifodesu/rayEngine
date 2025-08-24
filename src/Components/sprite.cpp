#include <iostream>
#include "sprite.h"
#include "texture_m.h"
#include "spawn.h"
#include <cmath>

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
    // Copy explicit frames (new system only)
    frameRects_ = desc.frameRects;
    frameDurations_ = desc.frameDurations;
    uniformFrameDuration_ = desc.defaultFrameDuration;
    if (frameRects_.empty()) {
        // Fallback: single frame at origin if nothing specified
        frameRects_.push_back({0,0,(float)sprite_sheet_.width,(float)sprite_sheet_.height});
    }
}

void Sprite::routine() {
    if (is_frozen_) return;
    float dt = static_cast<float>(Clock::getLap());
    size_t frameCount = frameRects_.size();
    if (frameCount > 1) {
        if (!frameDurations_.empty() && frameDurations_.size() == frameCount) {
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
    DrawTextureRec(sprite_sheet_, src, pos, tint_);
}
