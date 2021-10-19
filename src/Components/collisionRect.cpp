#include "collisionRect.h"

Quadtree CollisionRect::quad;
std::map<int, CollisionRect*>  CollisionRect::pool;

using namespace std;

CollisionRect::CollisionRect(nlohmann::json obj, GObject* father) {
    Rectangle rect{ 0.0f };
    solid_ = true;
    in_quad_ = true;
    father_ = father;
    pool_id_ = -1;

    if (!obj.contains("collisionRect")) {
        std::cout << "ERROR: no collision mask in json" << std::endl;
        return;
    }

    obj = obj["collisionRect"];
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

    if (obj.contains("in_quad"))
        in_quad_ = obj["in_quad"];

    if (in_quad_) {
        if (pool.size()) {
            pool_id_ = (--pool.end())->first + 1;
        }
        else {
            pool_id_ = 0;
        }
        pool[pool_id_] = this;
        quad.add({ pool_id_, surface_ });
    }
}

void CollisionRect::setCoord(Vector2 pos) {
    remove();
    surface_.x = pos.x;
    surface_.y = pos.y;
    add();
}
void CollisionRect::setDims(Vector2 dims) {
    remove();
    surface_.width = dims.x;
    surface_.height = dims.y;
    add();
}

std::vector<CollisionRect*> CollisionRect::query(Rectangle rect, bool force_solid) {
    auto queryVec = quad.query(rect);
    std::vector<CollisionRect*> ret;

    for (auto node : queryVec) {
        if ((pool[node.id]->solid_ && force_solid) || !force_solid) {
            ret.push_back(pool[node.id]);
        }
    }
    return ret;
}