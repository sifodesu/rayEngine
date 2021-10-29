#include "simpleBoss.h"
#include "pattern.h"
#include "object_m.h"
#include "bullet_m.h"
#include "raymath.h"

using namespace std;

#define p(delay, func, ...) patterns_.push(make_tuple(delay, bind(Pattern::func, __VA_ARGS__)))

SimpleBoss::SimpleBoss(nlohmann::json obj) : HObject(obj) {
    sprite_ = new Sprite(obj);
    body_ = new RigidBody(obj, this);
    time_ = 0;
    patDelay_ = 0;
    subphase_ = 0;
    phase_ = 0;
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

    p(0.5, line, bp, bp->getCoord(), Vector2{ 0, 200 }, 0.5, 11);
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

Vector2 SimpleBoss::getCenterForBullet(RigidBullet* bp) {
    return { body_->getCenterCoord().x - bp->surface_->getDims().x / 2,
                                body_->getCenterCoord().y - bp->surface_->getDims().y / 2 };
}

RigidBullet* SimpleBoss::createBasicRB() {
    RigidBullet* bp = new RigidBullet(Object_m::blueprints_[BULLET]);
    bp->setCurve(0);
    bp->setAcceleration(0);
    bp->no_dmg_ = { this };
    bp->setSpeed({ 75, 75 });
    bp->setCoord(getCenterForBullet(bp));
    bp->sprite_->setTint(RED);
    bp->ttl_ = 10;
    return bp;
}

void SimpleBoss::execPat(bool update_pos) {
    double delta = Clock::getLap();
    auto& [delay, pat] = patterns_.front();
    delay -= delta;
    if (delay <= 0) {
        if (update_pos) {
            bpq_.front()->setCoord(getCenterForBullet((RigidBullet*)bpq_.front()));
        }
        pat();
        patterns_.pop();
        delete bpq_.front();
        bpq_.pop();
    }
}

void SimpleBoss::genSelfDefense() {
    auto bp = createBasicRB();
    bp->setCurve(10);
    bp->ttl_ = 0.5;
    p(0.1, circle, bp, 20, 360, getDir(), 0, 30);
    bpq_.push(bp);
}

void SimpleBoss::patternDelay(double delay) {
    auto bp = createBasicRB();
    p(delay, nullPattern, bp);
    bpq_.push(bp);
}

void SimpleBoss::phase1() {
    double delta = Clock::getLap();
    if (sqrt(pow(getDir().x, 2) + pow(getDir().y, 2)) < 80) {
        genSelfDefense();
    }
    else if (subphase_ == 0) {
        auto bp = createBasicRB();
        bp->sprite_->setTint(GREEN);
        bp->setSpeed({ 100, 100 });
        p(0.05, circle, bp, 1, 0, getRandomDir(80), 0, 20);
        bpq_.push(bp);

        time_ -= delta + 0.1;
        if (time_ <= 0) {
            patternDelay(0.5);
            subphase_ = 1;
            // body_->setCoord({ body_->getCoord().x - 50, body_->getCoord().y });
            time_ = 2;
        }
    }
    else if (subphase_ == 1) {
        if (time_ <= 2) {
            Vector2 dir = getDir();
            int angle = 15;
            for (int i = 0; i < 20; i++, dir = Vector2Rotate(dir, angle)) {
                auto bp = createBasicRB();
                bp->sprite_->setTint(GREEN);
                bp->setSpeed({ 60, 60 });
                p(0.2, circle, bp, 6, 360, dir, 0, 15);
                bpq_.push(bp);
            }
        }
        time_ -= delta + 0.1;
        if (time_ <= 0) {
            time_ = 2;
            patternDelay(1);
            subphase_ = 0;
            // body_->setCoord({ body_->getCoord().x + 50, body_->getCoord().y });
        }
    }
}

void SimpleBoss::phase2() {
    double delta = Clock::getLap();
    if (sqrt(pow(getDir().x, 2) + pow(getDir().y, 2)) < 100) {
        genSelfDefense();
    }
    else if (time_ > 0) {
        auto bp = createBasicRB();
        bp->sprite_->setTint(RED);
        bp->setSpeed({ 100, 100 });
        p(0.05, circle, bp, 1, 0, getRandomDir(50), 0, 30);
        bpq_.push(bp);
        time_ -= delta + 0.1;
        if (time_ <= 0) {
            patternDelay(0.5);
        }
    }
    else {
        for (int i = 0; i < 3; i++) {
            auto bp = createBasicRB();
            bp->sprite_->setTint(RED);
            p(0.2, circle, bp, 5, 90, getDir(), 0, 30);
            bpq_.push(bp);
        }
        time_ = 2;
        patternDelay(1);
    }
}

void SimpleBoss::phase3() {
    double delta = Clock::getLap();
    if (sqrt(pow(getDir().x, 2) + pow(getDir().y, 2)) < 100) {
        genSelfDefense();
    }
    else if (time_ > 0) {
        auto bp = createBasicRB();
        bp->sprite_->setTint(BLACK);
        bp->setSpeed({ 100, 100 });
        p(0.05, circle, bp, 1, 0, getRandomDir(50), 0, 30);
        bpq_.push(bp);
        time_ -= delta + 0.1;
        if (time_ <= 0) {
            patternDelay(0.5);
        }
    }
    else {
        for (int i = 0; i < 3; i++) {
            auto bp = createBasicRB();
            bp->sprite_->setTint(BLACK);
            p(0.2, circle, bp, 5, 90, getDir(), 0, 30);
            bpq_.push(bp);
        }
        time_ = 2;
        patternDelay(1);
    }
}

void SimpleBoss::clearPatQueue() {
    while (!patterns_.empty()) {
        patterns_.pop();
        delete bpq_.front();
        bpq_.pop();
    }
}

void SimpleBoss::routine() {
    double delta = Clock::getLap();
    if (hp_ <= 0) {
        die();
        return;
    }
    if (patterns_.empty()) {
        if (hp_ > 40) {
            phase1();
        }
        else if (hp_ > 20) {
            if (phase_ == 0) {
                phase_++;
                clearPatQueue();
            }
            phase2();
        }
        else {
            if (phase_ == 1) {
                phase_++;
                clearPatQueue();
            }
            phase3();
        }
    }
    else {
        execPat();
    }

    sprite_->routine();
}

void SimpleBoss::draw() {
    sprite_->draw(body_->getCoord());
    Vector2 lifePos = { body_->getCoord().x - 25 + body_->getDims().x / 2, body_->getCoord().y - 10 };
    DrawRectangle(lifePos.x, lifePos.y, 50, 5, BLACK);
    DrawRectangle(lifePos.x, lifePos.y, ceil(50 * ((double)hp_ / 60)), 5, PURPLE);
    DrawRectangleLines(lifePos.x-1, lifePos.y, 52, 5, GRAY);
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