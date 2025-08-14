#include "pano.h"
#include "character.h"
#include "raycam_m.h"
#include "clock.h"
#include <raylib.h>
#include <string>

Pano::Pano(const SpawnData& data) : BasicEnt(data) {
    if (data.dialog.has_value()) text_ = *data.dialog;
}

Pano::~Pano() {
    // No texture cleanup needed anymore
}

void Pano::onCollision(GObject* other) {
    if (active_) return;
    if (dynamic_cast<Character*>(other)) {
        active_ = true;
        shown_ = 0;
        charAccumulator_ = 0.0f; // Reset character accumulator
    }
}

void Pano::routine() {
    BasicEnt::routine();
    if (!active_ || text_.empty()) return;
    
    // Allow skipping to end with space/enter
    if (shown_ < text_.size() && (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON))) {
        shown_ = text_.size();
        return;
    }
    
    // advance characters by time
    double dt = Clock::lap();
    charAccumulator_ += speedCps_ * dt;
    
    // Debug: uncomment to see timing values
    // if (shown_ < 5) printf("dt: %f, accumulator: %f, speedCps: %f\n", dt, charAccumulator_, speedCps_);
    
    size_t target = shown_;
    if (charAccumulator_ >= 1.0f) {
        size_t charsToAdd = static_cast<size_t>(charAccumulator_);
        target = shown_ + charsToAdd;
        charAccumulator_ -= charsToAdd; // Keep the fractional part
    }
    
    // Fallback: if no advancement after reasonable time, force at least 1 character every 10 frames
    static int frameCounter = 0;
    if (target == shown_) {
        frameCounter++;
        if (frameCounter >= 10 && shown_ < text_.size()) {
            target = shown_ + 1;
            frameCounter = 0;
        }
    } else {
        frameCounter = 0;
    }
    
    if (target > text_.size()) target = text_.size();
    
    // Ensure we show at least one character initially
    if (shown_ == 0 && target == 0) target = 1;
    
    shown_ = target;
    
    // close on input if finished
    if (shown_ >= text_.size()) {
        if (IsKeyPressed(KEY_SPACE) || IsKeyPressed(KEY_ENTER) || IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            active_ = false;
            // Optionally delete after reading:
            // to_delete_ = true;
        }
    }
}

void Pano::draw() {
    BasicEnt::draw();
    if (!active_ || text_.empty()) return;

    // Compute world position that maps to a fixed screen-space overlay.
    Camera2D cam = Raycam_m::getCam();
    Vector2 worldTL = { cam.target.x - cam.offset.x / cam.zoom, cam.target.y - cam.offset.y / cam.zoom };

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();
    const int margin = 20;
    const int boxH = sh / 4; // quarter screen height
    Rectangle boxWorld = { worldTL.x + (float)margin / cam.zoom, worldTL.y + (float)(sh - boxH - margin) / cam.zoom, (float)(sw - 2*margin) / cam.zoom, (float)boxH / cam.zoom };

    // Build dialog image once-per update with ImageDrawText (software), then draw texture
    const int pad = 12;
    const int imgW = (int)boxWorld.width;
    const int imgH = (int)boxWorld.height;
    const int fontSize = 24;

    // Ensure minimum size
    if (imgW <= 0 || imgH <= 0) return;

    // Draw dialog box directly without texture to avoid blinking
    DrawRectangleRec(boxWorld, Color{0,0,0,200});
    DrawRectangleLinesEx(boxWorld, 2, WHITE);
    
    // Render text directly
    std::string displayText = text_.substr(0, shown_);
    if (!displayText.empty()) {
        int x = (int)boxWorld.x + pad;
        int y = (int)boxWorld.y + pad;
        int maxW = (int)boxWorld.width - 2*pad;
        int bottomLimit = (int)boxWorld.y + (int)boxWorld.height - pad - fontSize; // Reserve space for one line
        
        // Simple word wrapping
        std::string line;
        std::string word;
        auto drawLine = [&]() {
            if (!line.empty() && y <= bottomLimit) {
                DrawText(line.c_str(), x, y, fontSize, WHITE);
                y += fontSize + 4;
                line.clear();
            }
        };
        
        for (size_t i = 0; i < displayText.size(); ++i) {
            char c = displayText[i];
            
            if (c == '\n') {
                if (!word.empty()) { 
                    line += word; 
                    word.clear(); 
                }
                drawLine();
                if (y > bottomLimit) break; // Stop if we're out of space
                continue;
            }
            if (c == ' ') {
                if (!word.empty()) {
                    std::string candidate = line + word + ' ';
                    int w = MeasureText(candidate.c_str(), fontSize);
                    if (w > maxW && !line.empty()) { 
                        drawLine(); 
                        if (y > bottomLimit) break; // Stop if we're out of space
                        line = word + ' '; 
                    } else {
                        line = candidate;
                    }
                    word.clear();
                }
            } else {
                word.push_back(c);
            }
        }
        
        // Handle remaining word
        if (!word.empty()) {
            std::string candidate = line + word;
            int w = MeasureText(candidate.c_str(), fontSize);
            if (w > maxW && !line.empty()) { 
                drawLine(); 
                if (y <= bottomLimit) {
                    line = word; 
                    drawLine();
                }
            } else {
                line = candidate;
                drawLine();
            }
        } else {
            // Draw the final line if no remaining word
            drawLine();
        }
    }

    // Hint
    const char* hint = (shown_ < text_.size()) ? "..." : "[SPACE]";
    Vector2 hintPos = { boxWorld.x + boxWorld.width - 80, boxWorld.y + boxWorld.height - 28 };
    DrawText(hint, (int)hintPos.x, (int)hintPos.y, 20, LIGHTGRAY);
}
