#include "collisionRect.h"

Quadtree CollisionRect::quad_static;
Quadtree CollisionRect::quad_dynamic;
std::map<int, CollisionRect*>  CollisionRect::pool;

using namespace std;

CollisionRect::CollisionRect(nlohmann::json obj, GObject* father) {
    Rectangle rect{ 0.0f };
    solid_ = true;
    father_ = father;
    is_static = true;
    pool_id_ = -1;

    if (!obj.contains("collisionRect")) {
        // std::cout << "ERROR: no collision mask in json" << std::endl;
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
    
    if (obj.contains("static"))
        is_static = obj["static"];

    
    if (pool.size()) {
        pool_id_ = (--pool.end())->first + 1;
    }
    else {
        pool_id_ = 0;
    }
    pool[pool_id_] = this;
    add();   
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

std::vector<CollisionRect*> CollisionRect::query(Rectangle rect, bool _is_static, bool with_solid) {
    std::vector<quadNode> queryVec;
    if (_is_static)
    {
        queryVec = quad_static.query(rect);
    }
    else
    {
        queryVec = quad_dynamic.query(rect);
    }
    std::vector<CollisionRect*> ret;
    for (auto node : queryVec) {
        if ((pool[node.id]->solid_ && with_solid) || !with_solid) {
            ret.push_back(pool[node.id]);
        }
    }
    return ret;
}
