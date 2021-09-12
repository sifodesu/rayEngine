#include <iostream>
#include "rigidBody.h"
#include "math.h"
#include "float.h"

Quadtree RigidBody::quad;
std::map<int, RigidBody*> RigidBody::pool;

using json = nlohmann::json;
using namespace std;

void RigidBody::updateQuad() {

}

//TODO: handle case when out of box quad

RigidBody::RigidBody(Rectangle surface, bool solid) : surface_(surface), solid_(solid) {
    if (pool.size())
        pool_id_ = (--pool.end())->first + 1;
    else
        pool_id_ = 0;
    pool[pool_id_] = this;
    quad.add(quadNode{ pool_id_, surface_ });
}

RigidBody::RigidBody(json obj, GObject* father) {
    Rectangle rect{ 0.0f };
    speed_ = { 0 };
    solid_ = true;
    father_ = father;
    if (!obj.contains("body")) {
        cout << "ERROR: no rigidbody in json" << endl;
        return;
    }
    obj = obj["body"];

    if (obj.contains("x"))
        rect.x = obj["x"];

    if (obj.contains("y"))
        rect.y = obj["y"];

    if (obj.contains("w"))
        rect.width = obj["w"];

    if (obj.contains("h"))
        rect.height = obj["h"];

    surface_ = rect;
    if (obj.contains("solid"))
        solid_ = obj["solid"];

    if (pool.size())
        pool_id_ = (--pool.end())->first + 1;
    else
        pool_id_ = 0;
    pool[pool_id_] = this;
    quad.add(quadNode{ pool_id_, surface_ });
}

RigidBody::~RigidBody() {
    pool.erase(pool_id_);
    quad.remove(quadNode{ pool_id_, surface_ });
}
void RigidBody::setSolid(bool solid) {
    solid_ = solid;
}
std::vector<RigidBody*> RigidBody::query(Rectangle rect) {
    auto queryVec = quad.query(rect);
    std::vector<RigidBody*> ret;

    for (auto node : queryVec) {
        ret.push_back(pool[node.id]);
    }
    return ret;
}

Vector2 RigidBody::getCoord() {
    return Vector2{ surface_.x, surface_.y };
}

void RigidBody::setCoord(Vector2 pos) {
    quad.remove({ pool_id_, surface_ });
    surface_.x = pos.x;
    surface_.y = pos.y;
    quad.add({ pool_id_, surface_ });
}

void RigidBody::setSpeed(Vector2 speed) {
    speed_ = speed;
}
Vector2 RigidBody::getSpeed() {
    return speed_;
}


void RigidBody::routine() {
    Rectangle fixSpeed = surface_;
    if (speed_.x > 0)
        fixSpeed.width += 1;
    if (speed_.x < 0)
        fixSpeed.x -= 1;
    for (RigidBody* body : query(fixSpeed))
        if (body->solid_ && solid_ && (body->pool_id_ != pool_id_))
            speed_.x = 0;

    fixSpeed = surface_;
    if (speed_.y > 0)
        fixSpeed.height += 1;
    if (speed_.y < 0)
        fixSpeed.y -= 1;
    for (RigidBody* body : query(fixSpeed))
        if (body->solid_ && solid_ && (body->pool_id_ != pool_id_))
            speed_.y = 0;


    Vector2 posBackup = { surface_.x, surface_.y };
    double delta = clock_.getLap();
    double distX = delta * speed_.x;
    double distY = delta * speed_.y;
    int signX = distX > 0 ? 1 : -1;
    int signY = distY > 0 ? 1 : -1;
    distX = abs(distX); distY = abs(distY);

    if (distX < FLT_EPSILON && distY < FLT_EPSILON)
        return;

    double incX = distX / (distX + distY);
    double incY = distY / (distX + distY);
    incX = min(distX, incX);
    incY = min(distY, incY);

    Rectangle temp = surface_;
    quad.remove({ pool_id_, surface_ });
    while (distX > FLT_EPSILON || distY > FLT_EPSILON) {
        if (distX > FLT_EPSILON)
            temp.x += incX * signX; distX -= incX;

        if (distY > FLT_EPSILON)
            temp.y += incY * signY; distY -= incY;

        bool solid_collide = false;
        for (RigidBody* body : query(temp))
            if (body->solid_ && solid_ && (body->pool_id_ != pool_id_))
                solid_collide = true;

        if (!solid_collide)
            surface_ = temp;
        else
            break;
    }
    // if(distX > FLT_EPSILON) {

    // }
    // if (abs(posBackup.x - surface_.x) > FLT_EPSILON || abs(posBackup.y - surface_.y) > FLT_EPSILON) {

    // }
    quad.add({ pool_id_, surface_ });

}
