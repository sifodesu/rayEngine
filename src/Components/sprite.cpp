#include <iostream>
#include "sprite.h"
#include "texture_m.h"
#include "spawn.h"
#include <cmath>

Sprite::Sprite(const std::string& filename, Rectangle rect)
    : filename_(filename), source_(rect), tint_(WHITE) {

    sprite_sheet_ = Texture_m::getTexture(filename);
}

Sprite::Sprite(const SpriteDesc& desc)
    : filename_(desc.filename), source_(desc.source), tint_(desc.tint) {
    sprite_sheet_ = Texture_m::getTexture(filename_);
    nb_frames_ = desc.nb_frames;
    frame_padding_ = desc.frame_padding;
    anim_speed_fps_ = desc.animation_speed;
    flipX_ = desc.flipX;
    flipY_ = desc.flipY;
}

void Sprite::routine() {
    if (is_frozen_) return;
    if (nb_frames_ > 1 && anim_speed_fps_ > 0.0f) {
        float dt = static_cast<float>(Clock::getLap());
        time_acc_ += dt;
        float frameTime = 1.0f / anim_speed_fps_;
        while (time_acc_ >= frameTime) {
            time_acc_ -= frameTime;
            current_frame_ = (current_frame_ + 1) % nb_frames_;
        }
    }
}

void Sprite::draw(Vector2 pos) {
    // Snap to integer pixels to avoid subpixel sampling artifacts
    pos.x = std::roundf(pos.x);
    pos.y = std::roundf(pos.y);
    Rectangle src = source_;
    if (nb_frames_ > 1) {
        // Assume horizontal strip; frame_padding_ pixels between frames
        src.x = source_.x + (source_.width + frame_padding_) * current_frame_;
    }
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
