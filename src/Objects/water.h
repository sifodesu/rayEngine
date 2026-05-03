#pragma once

#include "gObject.h"
#include "spawn.h"

enum class WaterVisualKind {
    Still = 0,
    Waterfall = 1
};

class Water : public GObject {
public:
    explicit Water(const SpawnData& data);
    ~Water() override;

    void draw() override;
    void drawAtBody(Rectangle bodyRect) override;
    Rectangle getRect() override;
    CollisionRect* getCollisionBody() override { return body_; }

    static void beginFrame();
    static void flushRefractionPasses();
    static void clear();

private:
    WaterVisualKind kind_;
    CollisionRect* body_{nullptr};
};
