#include "runes.h"
#include "input.h"

std::deque<int> Runes::queue_;
std::vector<Sprite*> Runes::sprites_;
int Runes::to_shoot_ = 0;
bool pianoMode = true;

void Runes::init() {
    for (int rune = 0; rune < 4; rune++) {
        sprites_.push_back(new Sprite(std::to_string(rune + 1 + (pianoMode ? 1 : 0)) + ".png", 1));
        newRune();
    }
}

void Runes::newRune() {
    if (!queue_.size()) {
        queue_.push_back(GetRandomValue(1, 4));
        return;
    }
    int last = queue_.back();
    int back = last;
    while (back == last)
        back = GetRandomValue(1, 4);
    queue_.push_back(back);
}

void Runes::routine() {
    if (InputMap::checkPressed("r" + std::to_string(queue_.front()))) {
        queue_.pop_front();
        newRune();
        to_shoot_++;
    }
}

void Runes::draw(Vector2 pos) {
    int offset = 0;

    for (int rune = 0; rune < 4; rune++) {
        pos.x += offset;
        sprites_[queue_[rune] - 1]->draw(pos);
        offset = sprites_[queue_[rune] - 1]->getFrameDim().x;
    }
}

bool Runes::getBullet() {
    if (to_shoot_ > 0)
        return to_shoot_--;
    return false;
}