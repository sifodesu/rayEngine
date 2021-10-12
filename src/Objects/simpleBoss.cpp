#include "simpleBoss.h"
#include "pattern.h"
#include "object_m.h"
using namespace std;

SimpleBoss::SimpleBoss(nlohmann::json obj) : HObject(obj) {
    sprite_ = new Sprite(obj);
    // sprite_->setTint(ORANGE);
    body_ = new RigidBody(obj, this);
    time_ = 0;
}
void SimpleBoss::die() {
    to_delete_ = true;
    RigidBullet* bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->ttl_ = 3;
    bp->setSpeed({ 300, 300 });
    bp->setCoord({ body_->getCenterCoord().x - bp->body_->getDims().x / 2,
                            body_->getCenterCoord().y - bp->body_->getDims().y / 2 });
    bp->setCurve(1);
    bp->sprite_->setTint(RED);

    Pattern::circle(bp, 300);

    bp->setSpeed({ 100, 200 });
    bp->setCurve(1.5);
    Pattern::circle(bp, 300);

    bp->setSpeed({ 200, 100 });
    bp->setCurve(1.5);
    Pattern::circle(bp, 300);

    delete bp;
}

void SimpleBoss::youpi() {
    RigidBullet* bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->no_dmg_ = { this };
    bp->setSpeed({ 400, 400 });
    bp->setCoord({ body_->getCenterCoord().x,
                            body_->getCenterCoord().y - 100 });
    bp->ttl_ = 5;
    bp->sprite_->setTint(RED);
    bp->setAcceleration(-3.0);
    bp->setCurve(0);
    patterns_.push(make_tuple(0.5, std::bind(Pattern::line, bp, bp->body_->getCoord(), Vector2{ 0, 200 }, 0.5, 11)));
    bpq_.push(bp);

    bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->no_dmg_ = { this };
    bp->setSpeed({ 400, 400 });
    bp->setCoord({ body_->getCenterCoord().x - 100,
                            body_->getCenterCoord().y });
    bp->ttl_ = 5;
    bp->sprite_->setTint(RED);
    bp->setAcceleration(-3.0);
    bp->setCurve(0);
    patterns_.push(make_tuple(0.5, std::bind(Pattern::line, bp, bp->body_->getCoord(), Vector2{ 200, 0 }, 0.5, 11)));
    bpq_.push(bp);

    bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->no_dmg_ = { this };
    bp->setSpeed({ 400, 400 });
    bp->setCoord({ body_->getCenterCoord().x + 100,
                            body_->getCenterCoord().y });
    bp->ttl_ = 5;
    bp->sprite_->setTint(RED);
    bp->setAcceleration(-3.0);
    bp->setCurve(0);
    patterns_.push(make_tuple(0.5, std::bind(Pattern::line, bp, bp->body_->getCoord(), Vector2{ -200, 0 }, 0.5, 11)));
    bpq_.push(bp);

    bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->no_dmg_ = { this };
    bp->setSpeed({ 400, 400 });
    bp->setCoord({ body_->getCenterCoord().x,
                            body_->getCenterCoord().y + 100 });
    bp->ttl_ = 5;
    bp->sprite_->setTint(RED);
    bp->setAcceleration(-3.0);
    bp->setCurve(0);
    patterns_.push(make_tuple(0.5, std::bind(Pattern::line, bp, bp->body_->getCoord(), Vector2{ 0, -200 }, 0.5, 11)));
    bpq_.push(bp);

    bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->no_dmg_ = { this };
    bp->setSpeed({ 200, 200 });
    bp->setCoord({ body_->getCenterCoord().x - bp->body_->getDims().x / 2,
                            body_->getCenterCoord().y - bp->body_->getDims().y / 2 });
    bp->body_->setCurve(0);
    bp->sprite_->setTint(RED);
    patterns_.push(make_tuple(0.5, std::bind(Pattern::circle, bp, 15)));
    bpq_.push(bp);
}

void SimpleBoss::routine() {
    double delta = clock_.getLap();
    time_ += delta;
    if (getHP() <= 0) {
        die();
        return;
    }

    if (patterns_.empty()) {
        // RigidBullet* bp = new RigidBullet(Object_m::blueprints_[BULLET]);
        // bp->no_dmg_ = { this };
        // bp->body_->setSpeed({ 200, 200 });
        // bp->body_->setCoord({ body_->getCenterCoord().x - bp->body_->getDims().x / 2,
        //                         body_->getCenterCoord().y - bp->body_->getDims().y / 2 });
        // bp->body_->setCurve(6);
        // bp->sprite_->setTint(RED);
        // patterns_.push(make_tuple(0.1, std::bind(Pattern::circle, bp, 15)));
        // bpq_.push(bp);

        youpi();
    }

    auto& [time, pat] = patterns_.front();
    time -= delta;
    if (time <= 0) {
        pat();
        patterns_.pop();
        delete bpq_.front();
        bpq_.pop();
    }
    sprite_->routine();
}

void SimpleBoss::draw() {
    sprite_->draw(body_->getCoord());
}

SimpleBoss::~SimpleBoss() {
    if (sprite_) {
        delete sprite_;
    }
    if (body_) {
        delete body_;
    }
    while (!bpq_.empty()) {
        delete bpq_.front();
        bpq_.pop();
    }
}