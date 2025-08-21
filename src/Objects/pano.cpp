#include "pano.h"
#include "character.h"
#include "raycam_m.h"
#include "clock.h"
#include <raylib.h>
#include <string>
#include <vector>
#include <cstdlib>
#include <cmath>
#include "input.h"
#include "definitions.h"

// Internal helpers to avoid code duplication and keep logic consistent
namespace {
    struct UiBox {
        Rectangle box{};            // World-space rectangle where dialog renders
        int pad = 6;
        int fontSize = 9;
        int x = 0;
        int y = 0;
        int maxW = 0;
        int bottom = 0;             // Last baseline allowed
    };

    inline UiBox computeUiBox() {
        UiBox ui{};
        // Map a fixed screen-space box to world-space so it follows the camera drawing pass
        Camera2D cam = Raycam_m::getCam();
        float zoom = (fabsf(cam.zoom) < 1e-6f) ? 1.0f : cam.zoom; // avoid div by zero
        Vector2 worldTL{ cam.target.x - cam.offset.x / zoom, cam.target.y - cam.offset.y / zoom };

        const int sw = NATIVE_RES_WIDTH;
        const int sh = NATIVE_RES_HEIGHT;
        const int margin = 20;
        const int boxH = sh / 4; // quarter screen height

        ui.box = {
            worldTL.x + margin / zoom,
            worldTL.y + (sh - boxH - margin) / zoom,
            (float)(sw - 2 * margin) / zoom,
            (float)boxH / zoom
        };

        ui.x = (int)ui.box.x + ui.pad;
        ui.y = (int)ui.box.y + ui.pad;
        ui.maxW = (int)ui.box.width - 2 * ui.pad;
        ui.bottom = (int)(ui.box.y + ui.box.height) - ui.pad - ui.fontSize;
        return ui;
    }

    struct Line { std::string text; int x; int y; };
    struct Wrapped { std::vector<Line> lines; size_t endIdx = 0; };

    // Supported inline controls: [p], [w:ms], [s:cps]
    enum class Control { None, PageBreak, Wait, Speed };
    struct CtrlResult { Control type = Control::None; size_t end = 0; float value = 0.0f; };

    inline CtrlResult parseControl(const std::string& s, size_t idx) {
        CtrlResult r{};
        if (idx >= s.size() || s[idx] != '[') return r;
        size_t close = s.find(']', idx);
        if (close == std::string::npos) return r;
        std::string inside = s.substr(idx + 1, close - idx - 1);
        if (inside == "p") { r.type = Control::PageBreak; r.end = close + 1; return r; }

        size_t colon = inside.find(':');
        if (colon == std::string::npos) return r;
        std::string key = inside.substr(0, colon);
        std::string val = inside.substr(colon + 1);
        if (key == "w") {
            // milliseconds -> seconds
            double ms = strtod(val.c_str(), nullptr);
            r.type = Control::Wait; r.value = (float)(ms / 1000.0); r.end = close + 1; return r;
        }
        if (key == "s") {
            double cps = strtod(val.c_str(), nullptr);
            r.type = Control::Speed; r.value = (float)cps; r.end = close + 1; return r;
        }
        return r;
    }

    // Collect the last seen speed/wait controls in [from, to)
    inline void accumulateEffects(const std::string& s, size_t from, size_t to, float defaultSpeed,
                                  bool& hasSpeed, float& speed, bool& hasWait, float& wait) {
        hasSpeed = false; hasWait = false;
        if (from >= to || from >= s.size()) return;
        size_t i = from;
        to = to > s.size() ? s.size() : to;
        while (i < to) {
            if (s[i] == '[') {
                CtrlResult cr = parseControl(s, i);
                if (cr.end > i) {
                    if (cr.type == Control::Speed) { hasSpeed = true; speed = (cr.value > 0.0f) ? cr.value : defaultSpeed; }
                    else if (cr.type == Control::Wait) { hasWait = true; wait = cr.value; }
                    i = cr.end; continue;
                }
            }
            ++i;
        }
    }

    // Skip consecutive page breaks; keep waits/speed for next page start
    inline size_t skipPageBreaksFrom(const std::string& s, size_t idx) {
        size_t i = idx;
        for (;;) {
            if (i >= s.size() || s[i] != '[') break;
            CtrlResult cr = parseControl(s, i);
            if (cr.end > i && cr.type == Control::PageBreak) { i = cr.end; continue; }
            break;
        }
        return i;
    }

    // Wrap printable text (controls are skipped) from startIdx up to limitIdx (exclusive)
    inline Wrapped wrapPage(const std::string& str, size_t startIdx, size_t limitIdx, const UiBox& ui) {
        Wrapped out{};
        if (startIdx >= str.size()) { out.endIdx = startIdx; return out; }
        if (limitIdx > str.size()) limitIdx = str.size();

        std::string line, word;
        int y = ui.y;
        size_t i = startIdx;

        auto commitLine = [&]() {
            if (!line.empty() && y <= ui.bottom) out.lines.push_back({line, ui.x, y});
            if (!line.empty()) y += ui.fontSize + 4;
            line.clear();
        };

        while (i < limitIdx) {
            if (str[i] == '[') {
                CtrlResult cr = parseControl(str, i);
                if (cr.end > i) { i = cr.end; continue; }
            }
            char c = str[i++];
            if (c == '\n') {
                if (!word.empty()) { line += word; word.clear(); }
                commitLine();
                if (y > ui.bottom) break;
                continue;
            }
            if (c == ' ') {
                if (!word.empty()) {
                    std::string candidate = line + word + ' ';
                    int w = MeasureText(candidate.c_str(), ui.fontSize);
                    if (w > ui.maxW && !line.empty()) {
                        commitLine();
                        if (y > ui.bottom) break;
                        line = word + ' ';
                    } else {
                        line = candidate;
                    }
                    word.clear();
                }
                continue;
            }
            word.push_back(c);
        }

        if (!word.empty()) {
            std::string candidate = line + word;
            int w = MeasureText(candidate.c_str(), ui.fontSize);
            if (w > ui.maxW && !line.empty()) {
                commitLine();
                if (y <= ui.bottom) { line = word; commitLine(); }
            } else {
                line = candidate; commitLine();
            }
        } else {
            commitLine();
        }

        out.endIdx = std::min(std::max(startIdx + (size_t)1, i), str.size());
        return out;
    }
}

Pano::Pano(const SpawnData& data) : BasicEnt(data) {
    if (data.dialog.has_value()) text_ = *data.dialog;
}

Pano::~Pano() = default;

void Pano::onCollision(GObject* other) {
    if (active_ || ended_) return;
    if (dynamic_cast<Character*>(other)) {
        if (InputMap::checkPressed("r2")) {
            // Activate dialog on interaction
            if (text_.empty()) return; // No text to show
            active_ = true;
            shown_ = 0;
            pageStart_ = 0;
            pageEnd_ = 0;
            charAccumulator_ = 0.0f; // Reset character accumulator
            speedCps_ = defaultSpeedCps_;
        }
    }
}

void Pano::routine() {
    BasicEnt::routine();
    ended_ = false;
    if (!active_ || text_.empty()) return;

    // Always compute page end for current layout (handles resizes)
    pageEnd_ = computePageEnd(pageStart_);
    if (pageEnd_ <= pageStart_) pageEnd_ = std::min(pageStart_ + 1, text_.size());

    const bool advanceInput = InputMap::checkPressed("r2");
    if (advanceInput) {
        if (shown_ < pageEnd_) {
            // If user skips mid-effect, apply effects from the skipped region to the next text
            bool hasSpeed = false, hasWait = false; float newSpeed = speedCps_, newWait = 0.0f;
            accumulateEffects(text_, shown_, pageEnd_, defaultSpeedCps_, hasSpeed, newSpeed, hasWait, newWait);
            shown_ = pageEnd_;
            if (hasSpeed) speedCps_ = newSpeed;
            if (hasWait) { waitTimer_ = newWait; /* next frame will wait before revealing */ }
            return;
        }
        if (pageEnd_ >= text_.size()) { active_ = false; ended_ = true; return; }
        // Next page: skip page-break tags so we start on content (but keep waits/speed tags)
        const size_t prevEnd = pageEnd_;
        pageStart_ = skipPageBreaksFrom(text_, pageEnd_);
        shown_ = pageStart_;
        charAccumulator_ = 0.0f;
        // pageEnd_ will be recomputed next frame (or now):
        pageEnd_ = computePageEnd(pageStart_);
        // Apply effects encountered strictly between previous end and new start (not inside new page)
        {
            bool hasSpeed = false, hasWait = false; float newSpeed = speedCps_, newWait = 0.0f;
            accumulateEffects(text_, prevEnd, pageStart_, defaultSpeedCps_, hasSpeed, newSpeed, hasWait, newWait);
            if (hasSpeed) speedCps_ = newSpeed;
            if (hasWait) waitTimer_ = newWait;
        }
        return;
    }
    
    // advance characters by time
    // Use per-frame delta (engine calls Clock::lap() once per frame)
    double dt = Clock::getLap();
    // Apply leading controls at current position immediately
    if (shown_ < pageEnd_) {
        while (shown_ < pageEnd_ && text_[shown_] == '[') {
            CtrlResult cr = parseControl(text_, shown_);
            if (cr.end <= shown_) break;
            if (cr.type == Control::PageBreak) { pageEnd_ = shown_; break; }
            if (cr.type == Control::Wait) { waitTimer_ = cr.value; shown_ = cr.end; return; }
            if (cr.type == Control::Speed) { speedCps_ = (cr.value > 0.0f) ? cr.value : defaultSpeedCps_; shown_ = cr.end; continue; }
            shown_ = cr.end; // Unknown: skip
        }
    }
    // Apply wait timer (could have been set by a control above)
    if (waitTimer_ > 0.0f) {
        waitTimer_ -= (float)dt;
        if (waitTimer_ > 0.0f) return; // still waiting
        // fallthrough once wait ends
    }
    charAccumulator_ += speedCps_ * dt;
    
    // Consume accumulator one character at a time to respect CPS exactly
    while (charAccumulator_ >= 1.0f && shown_ < pageEnd_) {
        // Handle control tags inline
        if (text_[shown_] == '[') {
            auto cr = parseControl(text_, shown_);
            if (cr.end > shown_) {
                if (cr.type == Control::PageBreak) {
                    // End current page at this position
                    pageEnd_ = shown_;
                    break;
                } else if (cr.type == Control::Wait) {
                    // Start wait immediately and stop revealing further this frame
                    waitTimer_ = cr.value;
                    shown_ = cr.end; // skip control tag
                    return;
                } else if (cr.type == Control::Speed) {
                    // Adjust cps and continue without consuming a printable char
                    speedCps_ = (cr.value > 0.0f) ? cr.value : defaultSpeedCps_;
                    shown_ = cr.end; // skip control tag
                    continue;
                } else {
                    // Unknown/other control: skip defensively
                    shown_ = cr.end;
                    continue;
                }
            }
        }
        // Normal printable char
        ++shown_;
        charAccumulator_ -= 1.0f;
    }
    
    // No auto-close here; handled by input above when on last page
}

void Pano::draw() {
    BasicEnt::draw();
    if (!active_ || text_.empty()) return;

    UiBox ui = computeUiBox();
    if (ui.box.width <= 0 || ui.box.height <= 0) return;

    // Box
    DrawRectangleRec(ui.box, Color{0,0,0,200});
    DrawRectangleLinesEx(ui.box, 2, WHITE);

    // Render only current page portion [pageStart_, shown_), skipping controls
    if (shown_ > pageStart_) {
        Wrapped wrapped = wrapPage(text_, pageStart_, shown_, ui);
        for (const auto& ln : wrapped.lines) {
            DrawText(ln.text.c_str(), ln.x, ln.y, ui.fontSize, WHITE);
        }
    }

    // Hint
    const bool moreToReveal = shown_ < pageEnd_;
    const bool morePages = pageEnd_ < text_.size();
    if (moreToReveal || morePages) {
        Vector2 hintPos = { ui.box.x + ui.box.width - 30, ui.box.y + ui.box.height - 24 };
        DrawText("...", (int)hintPos.x, (int)hintPos.y, 20, LIGHTGRAY);
    }
}

// Compute the exclusive end index of the page starting at startIdx by simulating wrapping with current box
size_t Pano::computePageEnd(size_t startIdx) const {
    UiBox ui = computeUiBox();
    if (ui.box.width <= 0 || ui.box.height <= 0) return std::min(startIdx + 1, text_.size());

    // Skip only leading page-break controls to avoid empty-page loops; keep waits/speed at start
    size_t effectiveStart = startIdx;
    while (effectiveStart < text_.size() && text_[effectiveStart] == '[') {
        CtrlResult cr = parseControl(text_, effectiveStart);
        if (cr.end > effectiveStart && cr.type == Control::PageBreak) { effectiveStart = cr.end; continue; }
        break;
    }

    // Cut the page at the first explicit page break if any
    size_t limit = text_.size();
    for (size_t i = effectiveStart; i < text_.size();) {
        if (text_[i] == '[') {
            CtrlResult cr = parseControl(text_, i);
            if (cr.end > i) {
                if (cr.type == Control::PageBreak) { limit = i; break; }
                i = cr.end; continue;
            }
        }
        ++i;
    }

    Wrapped wrapped = wrapPage(text_, startIdx, limit, ui);
    return std::max(wrapped.endIdx, effectiveStart);
}
