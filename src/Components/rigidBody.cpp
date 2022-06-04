#include <iostream>
#include "rigidBody.h"
#include "math.h"
#include "float.h"
#include "bullet.h"
#include "definitions.h"


using json = nlohmann::json;
using namespace std;

//TODO: handle case when out of box quad
RigidBody::RigidBody(json obj, GObject* father) : CollisionRect(obj, father) {
    speed_ = { 0 };
    acceleration_ = 0;
    curve_ = 0;
    
    obj = obj["collisionRect"];
    if (!obj.contains("body")) {
        // cout << "ERROR: no rigidbody in json" << endl;
        return;
    }

    obj = obj["body"];

    if (obj.contains("acceleration"))
        acceleration_ = obj["acceleration"];

    if (obj.contains("curve"))
        curve_ = obj["curve"];

    if (obj.contains("speed")) {
        if (obj["speed"].contains("x")) {
            speed_.x = obj["speed"]["x"];
        }
        if (obj["speed"].contains("y")) {
            speed_.y = obj["speed"]["y"];
        }
    }
}


void RigidBody::setCurve(double curve) {
    curve_ = curve;
}
void RigidBody::setAcceleration(double acc) {
    acceleration_ = acc;
}

void RigidBody::setSpeed(Vector2 speed) {
    speed_ = speed;
}
Vector2 RigidBody::getSpeed() {
    return speed_;
}
void RigidBody::fixSpeed() {
    if (solid_) {
        Rectangle fixSpeed = getSurface();
        if (speed_.x > 0)
            fixSpeed.width += 1;
        if (speed_.x < 0)
            fixSpeed.x -= 1;
        for (CollisionRect* body : query(fixSpeed))
            if (body->isSolid() && (body->getId() != pool_id_))
                speed_.x = 0;

        fixSpeed = getSurface();
        if (speed_.y > 0)
            fixSpeed.height += 1;
        if (speed_.y < 0)
            fixSpeed.y -= 1;
        for (CollisionRect* body : query(fixSpeed))
            if (body->isSolid() && (body->getId() != pool_id_))
                speed_.y = 0;
    }
}

void RigidBody::routine() {
    float delta = (float)Clock::getLap();
    if (abs(speed_.x) < FLT_EPSILON && abs(speed_.y) < FLT_EPSILON)
        return;

    float tempX = cos(curve_ * delta) * speed_.x - sin(curve_ * delta) * speed_.y;
    float tempY = sin(curve_ * delta) * speed_.x + cos(curve_ * delta) * speed_.y;
    speed_ = { tempX, tempY };

    fixSpeed();

    remove();

    float speedNorm = sqrt(pow(speed_.x * delta, 2) + pow(speed_.y * delta, 2));
    Vector2 unitSpeed = { speed_.x * delta / speedNorm, speed_.y * delta / speedNorm };

    while (speedNorm > 0) {
        Rectangle temp = getSurface();
        temp.x += unitSpeed.x * speedNorm;
        temp.y += unitSpeed.y * speedNorm;
        bool solid_collide = false;
        for (CollisionRect* body : query(temp)) {
            if (body->isSolid() && solid_ && (body->getId() != pool_id_)) {
                solid_collide = true;
            }
        }

        if (!solid_collide) {
            setSurface(temp);
            break;
        }
        else {
            speedNorm -= 0.1;
        }
    }

    add();
    speed_.x += acceleration_ * delta * speed_.x;
    speed_.y += acceleration_ * delta * speed_.y;
}


