#include "rigidBody.h"
#include "math.h"

Quadtree RigidBody::quad;
std::vector<RigidBody*> RigidBody::pool;

using json = nlohmann::json;
using namespace std;

RigidBody::RigidBody(Rectangle surface, bool solid) : surface_(surface), solid_(solid) {
    pool_id_ = pool.size();
    pool.push_back(this);
    quad.add(quadNode{ pool_id_, surface_ });
}

RigidBody::RigidBody(json obj, GObject* father) {
    Rectangle rect{ 0.0f };
    speed_ = { 0 };
    solid_ = true;
    father_ = father;

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

    pool_id_ = pool.size();
    pool.push_back(this);
    quad.add(quadNode{ pool_id_, surface_ });
}

RigidBody::~RigidBody() {
    pool.erase(pool.begin() + pool_id_);
    quad.remove(quadNode{ pool_id_, surface_ });
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

void RigidBody::setSpeed(Vector2 speed) {
    speed_ = speed;
}

void RigidBody::routine() {
    double delta = clock_.getLap();
    double distX = delta * speed_.x;
    double distY = delta * speed_.y;
    if (abs(distX) < 0.0000001 && abs(distY) < 0.0000001)
        return;

    double incX = distX / (abs(distX) + abs(distY));
    double incY = distY / (abs(distX) + abs(distY));
    incX = min(abs(distX), 1.0);
    incY = min(abs(distY), 1.0);
    distX = abs(distX); distY = abs(distY);

    Rectangle temp = surface_;
    while (distX > 0.0000001 || distY > 0.0000001) {
        if (abs(distX) > 0.0000001)
            temp.x += incX; distX -= abs(incX);

        if (abs(distY) > 0.0000001)
            temp.y += incY; distY -= abs(incY);

        bool solid_collide = false;
        for (RigidBody* body : query(temp))
            if (body->solid_ && (body->pool_id_ != pool_id_))
                solid_collide = true;

        if (!solid_collide)
            surface_ = temp;
        else
            break;
    }

}
