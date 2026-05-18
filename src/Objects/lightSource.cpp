#include "lightSource.h"

LightSource::LightSource(const SpawnData& data)
    : GObject(data.id) {
    if (data.physics.collision.has_value()) {
        rect_ = data.physics.collision->rect;
    }
}
