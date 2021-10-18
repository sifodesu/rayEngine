#include "simpleBoss.h"
#include "pattern.h"
#include "object_m.h"
#include "bullet_m.h"

using namespace std;

SimpleBoss::SimpleBoss(nlohmann::json obj) : HObject(obj) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    time_ = 0;
    chara_ = (Character*)Object_m::getObj(t(Character));
}

void SimpleBoss::die() {
    to_delete_ = true;
    Bullet_m::purge();

    RigidBullet* bp = createBasicRB();
    bp->ttl_ = 3;
    bp->sprite_->setTint(VIOLET);
    bp->no_dmg_.insert(chara_);

    bp->setAcceleration(1.5);
    bp->setCurve(1.5);

    bp->setSpeed({ 100, 200 });
    Pattern::circle(bp, 300);

    bp->setSpeed({ 200, 100 });
    Pattern::circle(bp, 300);

    bp->setSpeed({ 200, 50 });
    Pattern::circle(bp, 300);

    bp->setSpeed({ 50, 200 });
    Pattern::circle(bp, 300);

    delete bp;
}

void SimpleBoss::youpi() {
    RigidBullet* bp = createBasicRB();
    bp->setSpeed({ 400, 400 });
    bp->setCoord({ body_->getCenterCoord().x,
                            body_->getCenterCoord().y - 100 });
    bp->ttl_ = 5;
    bp->setAcceleration(-3.0);

    patterns_.push(make_tuple(0.5, std::bind(Pattern::line, bp, bp->body_->getCoord(), Vector2{ 0, 200 }, 0.5, 11)));
    bpq_.push(bp);
}

Vector2 SimpleBoss::getRandomDir(double magnitude) {
    Vector2 dir = getDir();
    dir.x += GetRandomValue(-magnitude, magnitude);
    dir.y += GetRandomValue(-magnitude, magnitude);
    return dir;
}

Vector2 SimpleBoss::getDir() {
    if (chara_ == nullptr) {
        chara_ = (Character*)Object_m::getObj(t(Character));
        if (chara_ == nullptr) {
            return { 0,0 };
        }
    }
    return { chara_->body_->getCoord().x - body_->getCoord().x,
                chara_->body_->getCoord().y - body_->getCoord().y };
}

RigidBullet* SimpleBoss::createBasicRB() {
    RigidBullet* bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->body_->setCurve(0);
    bp->body_->setAcceleration(0);
    bp->no_dmg_ = { this };
    bp->body_->setSpeed({ 50, 50 });
    bp->body_->setCoord({ body_->getCenterCoord().x - bp->body_->getDims().x / 2,
                            body_->getCenterCoord().y - bp->body_->getDims().y / 2 });
    bp->sprite_->setTint(RED);
    return bp;
}

void SimpleBoss::routine() {
    double delta = Clock::getLap();
    time_ += delta;
    if (getHP() <= 0) {
        die();
        return;
    }

    if (patterns_.empty()) {
        if (Bullet_m::waiting_bullets.empty()) {
            auto bp = createBasicRB();
            if (sqrt(pow(getDir().x, 2) + pow(getDir().y, 2)) < 100) {
                bp->body_->setCurve(10);
                patterns_.push(make_tuple(0.1, std::bind(Pattern::circle, bp, 20, 360, getDir(), 0, 30)));
            }
            else {
                patterns_.push(make_tuple(0.1, std::bind(Pattern::circle, bp, 10, 45, getDir(), 0.01, 40)));
            }
            bpq_.push(bp);
        }
    }
    else {
        auto& [time, pat] = patterns_.front();
        time -= delta;
        if (time <= 0) {
            pat();
            patterns_.pop();
            delete bpq_.front();
            bpq_.pop();
        }
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