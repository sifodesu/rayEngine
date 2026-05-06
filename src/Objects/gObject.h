#pragma once
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <raylib.h>
#include <nlohmann/json.hpp>
#include "spawn.h"

class AdiComponent;
class LinkableComponent;
class KillComponent;
class LightComponent;
class CollisionRect;
class Sprite;

class GObject {
public:
    explicit GObject(const int id);
    virtual ~GObject();
    virtual void routine() {}
    virtual void trigger() {}
    virtual void draw() {}
    virtual void drawAtBody(Rectangle) { draw(); }
    virtual void draw3D() {}
    virtual bool is3DRenderable() const { return false; }
    virtual bool isPlayerClone() const { return false; }
    virtual bool blocksMovementFor(GObject*) const { return true; }
    virtual void onCollision(GObject*) {}
    virtual Rectangle getRect() { return Rectangle{0.0f, 0.0f, 1.0f, 1.0f}; }
    virtual CollisionRect* getCollisionBody() { return nullptr; }
    virtual void collectDebugSprites(std::vector<Sprite*>&) {}

    void setKillOnCollision(bool enabled);
    void applyKillOnCollision(GObject* other);
    bool hasKillOnCollision() const { return killOnCol_; }
    void configureLight(const SpawnData& data);
    LightComponent* getLightComponent() { return lightComponent_; }
    const LightComponent* getLightComponent() const { return lightComponent_; }
    
    // Méthode virtuelle pour récupérer l'AdiComponent (si présent)
    virtual std::optional<AdiComponent*> getAdiComponent() { return std::nullopt; }
    
    // Méthode virtuelle pour récupérer le LinkableComponent (si présent)
    virtual std::optional<LinkableComponent*> getLinkableComponent() { return std::nullopt; }

    const int id_;
    // int x, y;
    bool to_delete_;
    int layer_;

protected:
    bool killOnCol_ = false;
    KillComponent* killOnColComponent_ = nullptr;
    LightComponent* lightComponent_ = nullptr;
};
