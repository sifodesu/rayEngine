#include "simpleBoss.h"
#include "pattern.h"
using namespace std;

SimpleBoss::SimpleBoss(nlohmann::json obj) : HObject(obj) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
}

void SimpleBoss::routine() {
    if (patterns_.empty()) {
        clock_.getLap();
        patterns_.push(make_tuple(0.1, std::bind(Pattern::circle, std::unordered_set<GObject*>{ this }, body_->getCoord(), 150, 100)));
        patterns_.push(make_tuple(0.5, std::bind(Pattern::circle, std::unordered_set<GObject*>{ this }, body_->getCoord(), 300, 300)));
    }

    auto& [time, pat] = patterns_.front();
    time -= clock_.getLap();
    if (time <= 0) {
        pat();
        patterns_.pop();
    }

}

void SimpleBoss::draw() {
    sprite_->draw(body_->getCoord());
}
