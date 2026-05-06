#pragma once

#include "gObject.h"
#include "spawn.h"

class Fog : public GObject {
public:
    explicit Fog(const SpawnData& data);
    ~Fog() override;

    void draw() override;
    void drawAtBody(Rectangle bodyRect) override;
    Rectangle getRect() override;
    CollisionRect* getCollisionBody() override { return body_; }

    static void beginFrame();
    static void flushFogPasses();
    static void clear();

private:
    CollisionRect* body_{nullptr};
};
