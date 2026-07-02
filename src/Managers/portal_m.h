#pragma once

#include <optional>
#include <vector>

#include <raylib.h>

class CollisionRect;
class GObject;
class Portal;
enum class GravityDirection;

class Portal_m {
public:
    static void registerPortal(Portal* portal);
    static void unregisterPortal(Portal* portal);
    static void setupLinks();
    static void restoreTiles();

    static bool beginTransit(Portal* source, GObject* entity);
    static void prepareMovement(
        GObject* entity,
        CollisionRect* body,
        Rectangle fromRect,
        Rectangle toRect
    );
    static bool isMovementBlocked(
        GObject* entity,
        CollisionRect* body,
        Rectangle proposedRect
    );
    static bool isProbeBlocked(
        GObject* entity,
        CollisionRect* body,
        Rectangle probeRect
    );
    static bool separateCollisions(GObject* entity, CollisionRect* body);
    static bool sync(
        GObject* entity,
        const std::vector<CollisionRect*>& carriedBodies = {}
    );
    static std::optional<Vector2> gravityStep(
        GObject* entity,
        GravityDirection currentDirection,
        double acceleration,
        float delta
    );

    static void update(double delta);
    static void cancel(GObject* entity);
    static void clear();

    static bool isInTransit(GObject* entity);
    static std::optional<Rectangle> sourceSurface(GObject* entity);
    static std::optional<Rectangle> targetSurface(GObject* entity);
    static std::vector<Rectangle> visibleSurfaces(GObject* entity);
    static std::optional<Vector2> visibleCenter(GObject* entity);
    static bool isSourceVisible(GObject* entity, Rectangle rect);
    static std::optional<Rectangle> transformRect(GObject* entity, Rectangle rect);

    static void drawForLayer(int layer);
    static bool shouldIgnoreCollision(GObject* moving, CollisionRect* obstacle);
};
