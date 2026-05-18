#pragma once

#include "gObject.h"
#include "spawn.h"

class LightSource : public GObject {
public:
    explicit LightSource(const SpawnData& data);

    Rectangle getRect() override { return rect_; }

private:
    Rectangle rect_{0.0f, 0.0f, 8.0f, 8.0f};
};
