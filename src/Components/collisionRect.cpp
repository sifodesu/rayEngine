#include "collisionRect.h"

Quadtree CollisionRect::quad_;
std::map<int, CollisionRect*>  CollisionRect::pool;

using namespace std;

CollisionRect::CollisionRect(const CollisionDesc& desc, GObject* father) {
    father_ = father;
    surface_ = desc.rect;
    solid_ = desc.solid;
    pool_id_ = -1;
    in_quadtree_ = false;  // Initialize the flag
    stored_node_ = {-1, {0, 0, 0, 0}};  // Initialize stored node

    if (pool.size()) {
        pool_id_ = (--pool.end())->first + 1;
    } else {
        pool_id_ = 0;
    }
    pool[pool_id_] = this;
    add();
}

CollisionRect::~CollisionRect() {
    remove();
    pool.erase(pool_id_);
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

void CollisionRect::setSurface(Rectangle surf) {
    remove();
    surface_ = surf;
    add();
}

std::vector<CollisionRect*> CollisionRect::query(Rectangle rect, bool with_solid) {
    std::vector<quadNode> queryVec = quad_.query(rect);
    std::vector<CollisionRect*> ret;
    for (auto node : queryVec) {
        if ((pool[node.id]->solid_ && with_solid) || !with_solid) {
            ret.push_back(pool[node.id]);
        }
    }
    return ret;
}

std::vector<CollisionRect*> CollisionRect::all(bool with_solid) {
    std::vector<CollisionRect*> ret;
    ret.reserve(pool.size());
    for (auto& [id, body] : pool) {
        if (!body) continue;
        if (with_solid && !body->solid_) continue;
        ret.push_back(body);
    }
    return ret;
}
