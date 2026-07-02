#include "portal_m.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "character.h"
#include "collisionRect.h"
#include "object_m.h"
#include "oneWayPlatform.h"
#include "portal.h"
#include "raycam_m.h"
#include "rigidBody.h"

namespace {

constexpr float kMinPieceSize = 0.01f;
constexpr float kCrossEpsilon = 0.001f;
constexpr float kApertureTolerance = 0.05f;
constexpr float kExitClearance = 0.05f;
constexpr float kProgressDistance = 0.05f;
constexpr float kMinimumExitSpeed = 40.0f;
constexpr double kStallTimeout = 0.35;
constexpr double kMaximumTransitTime = 3.0;

struct PortalFrame {
    Rectangle rect{};
    PortalDirection direction = PortalDirection::RIGHT;
    Vector2 normal{1.0f, 0.0f};
    Vector2 tangent{0.0f, 1.0f};
    Vector2 planeCenter{};
    float plane = 0.0f;
};

struct Transit {
    int entityId = -1;
    Portal* source = nullptr;
    Portal* target = nullptr;
    PortalFrame sourceFrame;
    PortalFrame targetFrame;
    Rectangle previousRect{};
    Rectangle sourceRect{};
    Rectangle targetRect{};
    std::unique_ptr<CollisionRect> sourceProxy;
    std::unique_ptr<CollisionRect> targetProxy;
    std::optional<Rectangle> sourcePiece;
    std::optional<Rectangle> targetPiece;
    bool blockedEntering = false;
    float progressAnchor = 0.0f;
    double stalledFor = 0.0;
    double elapsed = 0.0;
};

struct DisabledTile {
    bool originalSolid = true;
    int references = 0;
};

enum class RecoveryResult {
    Recovered,
    Respawn
};

struct PendingEnd {
    int entityId = -1;
    bool respawn = false;
};

std::map<int, Portal*> portals;
std::map<int, Transit> transits;
std::map<int, DisabledTile> disabledTiles;
std::map<int, std::vector<int>> portalTiles;

Vector2 normalFor(PortalDirection direction) {
    switch (direction) {
        case PortalDirection::UP:
            return {0.0f, -1.0f};
        case PortalDirection::DOWN:
            return {0.0f, 1.0f};
        case PortalDirection::LEFT:
            return {-1.0f, 0.0f};
        case PortalDirection::RIGHT:
            return {1.0f, 0.0f};
    }
    return {1.0f, 0.0f};
}

Vector2 tangentFor(PortalDirection direction) {
    switch (direction) {
        case PortalDirection::UP:
        case PortalDirection::DOWN:
            return {1.0f, 0.0f};
        case PortalDirection::LEFT:
        case PortalDirection::RIGHT:
            return {0.0f, 1.0f};
    }
    return {0.0f, 1.0f};
}

Vector2 centerOf(Rectangle rect) {
    return {
        rect.x + rect.width * 0.5f,
        rect.y + rect.height * 0.5f
    };
}

float dot(Vector2 left, Vector2 right) {
    return left.x * right.x + left.y * right.y;
}

bool hasArea(Rectangle rect) {
    return rect.width > kMinPieceSize && rect.height > kMinPieceSize;
}

std::optional<Rectangle> intersection(Rectangle left, Rectangle right) {
    float x = std::max(left.x, right.x);
    float y = std::max(left.y, right.y);
    float farX = std::min(left.x + left.width, right.x + right.width);
    float farY = std::min(left.y + left.height, right.y + right.height);
    Rectangle result{x, y, farX - x, farY - y};
    if (!hasArea(result)) return std::nullopt;
    return result;
}

Rectangle withCenter(Rectangle rect, Vector2 center) {
    rect.x = center.x - rect.width * 0.5f;
    rect.y = center.y - rect.height * 0.5f;
    return rect;
}

std::optional<PortalFrame> frameFor(Portal* portal) {
    if (!portal || !portal->getDirection().has_value()) return std::nullopt;

    PortalFrame frame;
    frame.rect = portal->getPortalRect();
    frame.direction = *portal->getDirection();
    frame.normal = normalFor(frame.direction);
    frame.tangent = tangentFor(frame.direction);

    switch (frame.direction) {
        case PortalDirection::UP:
            frame.plane = frame.rect.y;
            frame.planeCenter = {
                frame.rect.x + frame.rect.width * 0.5f,
                frame.plane
            };
            break;
        case PortalDirection::DOWN:
            frame.plane = frame.rect.y + frame.rect.height;
            frame.planeCenter = {
                frame.rect.x + frame.rect.width * 0.5f,
                frame.plane
            };
            break;
        case PortalDirection::LEFT:
            frame.plane = frame.rect.x;
            frame.planeCenter = {
                frame.plane,
                frame.rect.y + frame.rect.height * 0.5f
            };
            break;
        case PortalDirection::RIGHT:
            frame.plane = frame.rect.x + frame.rect.width;
            frame.planeCenter = {
                frame.plane,
                frame.rect.y + frame.rect.height * 0.5f
            };
            break;
    }
    return frame;
}

float openSideDistance(Vector2 point, const PortalFrame& frame) {
    return dot(
        {point.x - frame.planeCenter.x, point.y - frame.planeCenter.y},
        frame.normal
    );
}

bool overlapsAperture(Rectangle rect, const PortalFrame& frame) {
    return CheckCollisionRecs(rect, frame.rect);
}

std::optional<Rectangle> openPiece(Rectangle rect, const PortalFrame& frame) {
    Rectangle piece = rect;
    switch (frame.direction) {
        case PortalDirection::RIGHT: {
            float left = std::max(rect.x, frame.plane);
            piece.x = left;
            piece.width = rect.x + rect.width - left;
            break;
        }
        case PortalDirection::LEFT: {
            float right = std::min(rect.x + rect.width, frame.plane);
            piece.width = right - rect.x;
            break;
        }
        case PortalDirection::DOWN: {
            float top = std::max(rect.y, frame.plane);
            piece.y = top;
            piece.height = rect.y + rect.height - top;
            break;
        }
        case PortalDirection::UP: {
            float bottom = std::min(rect.y + rect.height, frame.plane);
            piece.height = bottom - rect.y;
            break;
        }
    }
    if (!hasArea(piece)) return std::nullopt;
    return piece;
}

std::optional<Rectangle> insidePiece(Rectangle rect, const PortalFrame& frame) {
    Rectangle piece = rect;
    switch (frame.direction) {
        case PortalDirection::RIGHT: {
            float right = std::min(rect.x + rect.width, frame.plane);
            piece.width = right - rect.x;
            break;
        }
        case PortalDirection::LEFT: {
            float left = std::max(rect.x, frame.plane);
            piece.x = left;
            piece.width = rect.x + rect.width - left;
            break;
        }
        case PortalDirection::DOWN: {
            float bottom = std::min(rect.y + rect.height, frame.plane);
            piece.height = bottom - rect.y;
            break;
        }
        case PortalDirection::UP: {
            float top = std::max(rect.y, frame.plane);
            piece.y = top;
            piece.height = rect.y + rect.height - top;
            break;
        }
    }
    if (!hasArea(piece)) return std::nullopt;
    return piece;
}

bool straddlesPlane(Rectangle rect, const PortalFrame& frame) {
    return openPiece(rect, frame).has_value() &&
           insidePiece(rect, frame).has_value();
}

bool fitsAperture(Rectangle rect, const PortalFrame& frame) {
    if (frame.direction == PortalDirection::LEFT ||
        frame.direction == PortalDirection::RIGHT) {
        return rect.y >= frame.rect.y - kApertureTolerance &&
               rect.y + rect.height <=
                   frame.rect.y + frame.rect.height + kApertureTolerance;
    }

    return rect.x >= frame.rect.x - kApertureTolerance &&
           rect.x + rect.width <=
               frame.rect.x + frame.rect.width + kApertureTolerance;
}

Rectangle snapToAperture(Rectangle rect, const PortalFrame& frame) {
    bool vertical =
        frame.direction == PortalDirection::LEFT ||
        frame.direction == PortalDirection::RIGHT;
    float bodySize = vertical ? rect.height : rect.width;
    float apertureSize = vertical ? frame.rect.height : frame.rect.width;
    if (bodySize > apertureSize) return rect;

    float minimum = vertical ? frame.rect.y : frame.rect.x;
    float maximum = minimum + apertureSize - bodySize;
    float& position = vertical ? rect.y : rect.x;
    if (position < minimum &&
        position >= minimum - kApertureTolerance) {
        position = minimum;
    } else if (position > maximum &&
               position <= maximum + kApertureTolerance) {
        position = maximum;
    }
    return rect;
}

Rectangle transformBetween(
    Rectangle rect,
    const PortalFrame& source,
    const PortalFrame& target
) {
    Vector2 center = centerOf(rect);
    Vector2 offset{
        center.x - source.planeCenter.x,
        center.y - source.planeCenter.y
    };
    float normalOffset = dot(offset, source.normal);
    float tangentOffset = dot(offset, source.tangent);
    Vector2 targetCenter{
        target.planeCenter.x - target.normal.x * normalOffset +
            target.tangent.x * tangentOffset,
        target.planeCenter.y - target.normal.y * normalOffset +
            target.tangent.y * tangentOffset
    };
    return withCenter(rect, targetCenter);
}

Vector2 rotateBetween(
    Vector2 value,
    const PortalFrame& source,
    const PortalFrame& target
) {
    float normalValue = dot(value, source.normal);
    float tangentValue = dot(value, source.tangent);
    return {
        -target.normal.x * normalValue + target.tangent.x * tangentValue,
        -target.normal.y * normalValue + target.tangent.y * tangentValue
    };
}

Vector2 gravityVector(GravityDirection direction, double acceleration) {
    float amount = static_cast<float>(acceleration);
    switch (direction) {
        case GravityDirection::UP:
            return {0.0f, -amount};
        case GravityDirection::DOWN:
            return {0.0f, amount};
        case GravityDirection::LEFT:
            return {-amount, 0.0f};
        case GravityDirection::RIGHT:
            return {amount, 0.0f};
    }
    return {0.0f, amount};
}

GravityDirection gravityFor(PortalDirection direction) {
    switch (direction) {
        case PortalDirection::UP:
            return GravityDirection::UP;
        case PortalDirection::DOWN:
            return GravityDirection::DOWN;
        case PortalDirection::LEFT:
            return GravityDirection::LEFT;
        case PortalDirection::RIGHT:
            return GravityDirection::RIGHT;
    }
    return GravityDirection::DOWN;
}

bool ignoresAperture(Rectangle movingRect, CollisionRect* obstacle) {
    if (!obstacle) return false;
    if (dynamic_cast<Portal*>(obstacle->getFather())) return true;

    std::optional<Rectangle> overlap =
        intersection(movingRect, obstacle->getSurface());
    if (!overlap.has_value()) return false;

    for (const auto& [_, portal] : portals) {
        if (!portal || !portal->getLinkedPortal()) continue;
        auto frame = frameFor(portal);
        if (!frame.has_value()) continue;

        if (frame->direction == PortalDirection::LEFT ||
            frame->direction == PortalDirection::RIGHT) {
            if (overlap->y >= frame->rect.y - kApertureTolerance &&
                overlap->y + overlap->height <=
                    frame->rect.y + frame->rect.height + kApertureTolerance &&
                CheckCollisionRecs(*overlap, frame->rect)) {
                return true;
            }
        } else if (
            overlap->x >= frame->rect.x - kApertureTolerance &&
            overlap->x + overlap->width <=
                frame->rect.x + frame->rect.width + kApertureTolerance &&
            CheckCollisionRecs(*overlap, frame->rect)) {
            return true;
        }
    }
    return false;
}

void releasePortalTiles(int portalId) {
    auto idsIt = portalTiles.find(portalId);
    if (idsIt == portalTiles.end()) return;

    for (int tileId : idsIt->second) {
        auto disabledIt = disabledTiles.find(tileId);
        if (disabledIt == disabledTiles.end()) continue;

        if (--disabledIt->second.references > 0) continue;

        auto tileIt = Object_m::level_tiles_.find(tileId);
        if (tileIt != Object_m::level_tiles_.end() && tileIt->second) {
            if (CollisionRect* body = tileIt->second->getCollisionBody()) {
                body->setSolid(disabledIt->second.originalSolid);
            }
        }
        disabledTiles.erase(disabledIt);
    }
    idsIt->second.clear();
}

void openPortalTiles(Portal* portal) {
    if (!portal) return;
    auto frame = frameFor(portal);
    if (!frame.has_value()) return;

    releasePortalTiles(portal->id_);
    std::vector<int>& tileIds = portalTiles[portal->id_];
    for (CollisionRect* body : CollisionRect::query(frame->rect, false)) {
        if (!body || !CheckCollisionRecs(body->getSurface(), frame->rect)) continue;

        GObject* owner = body->getFather();
        if (!owner ||
            Object_m::level_tiles_.find(owner->id_) ==
                Object_m::level_tiles_.end()) {
            continue;
        }

        auto [it, inserted] = disabledTiles.emplace(
            owner->id_,
            DisabledTile{body->isSolid(), 0}
        );
        ++it->second.references;
        tileIds.push_back(owner->id_);
        body->setSolid(false);
    }
}

bool isActiveMainBody(CollisionRect* body) {
    if (!body || body->isRenderProxy()) return false;
    GObject* owner = body->getFather();
    return owner && transits.contains(owner->id_);
}

bool isBlocked(
    GObject* entity,
    CollisionRect* entityBody,
    Rectangle rect,
    const Transit* transit = nullptr
) {
    if (!entityBody || !entityBody->isSolid()) return false;

    for (CollisionRect* obstacle : CollisionRect::query(rect, true)) {
        if (!obstacle || obstacle == entityBody) continue;
        if (transit &&
            (obstacle == transit->sourceProxy.get() ||
             obstacle == transit->targetProxy.get())) {
            continue;
        }
        if (isActiveMainBody(obstacle)) continue;

        GObject* owner = obstacle->getFather();
        if (!owner || owner == entity || !owner->blocksMovementFor(entity)) continue;
        if (dynamic_cast<Portal*>(owner)) continue;
        if (ignoresAperture(rect, obstacle)) continue;
        if (OneWayPlatform::isOneWayPlatformBody(obstacle) &&
            !OneWayPlatform::blocksBody(entityBody, rect, obstacle)) {
            continue;
        }
        if (CheckCollisionRecs(rect, obstacle->getSurface())) return true;
    }
    return false;
}

bool movesIntoPortal(
    Rectangle current,
    Rectangle proposed,
    const PortalFrame& frame
) {
    return openSideDistance(centerOf(proposed), frame) <
           openSideDistance(centerOf(current), frame) - kCrossEpsilon;
}

Rectangle placeOnOpenSide(Rectangle rect, const PortalFrame& frame) {
    switch (frame.direction) {
        case PortalDirection::UP:
            rect.y = frame.plane - rect.height - kExitClearance;
            break;
        case PortalDirection::DOWN:
            rect.y = frame.plane + kExitClearance;
            break;
        case PortalDirection::LEFT:
            rect.x = frame.plane - rect.width - kExitClearance;
            break;
        case PortalDirection::RIGHT:
            rect.x = frame.plane + kExitClearance;
            break;
    }
    return rect;
}

void addCandidate(
    std::vector<float>& candidates,
    float value,
    float minimum,
    float maximum
) {
    value = std::clamp(value, minimum, maximum);
    for (float candidate : candidates) {
        if (std::fabs(candidate - value) <= kCrossEpsilon) return;
    }
    candidates.push_back(value);
}

std::optional<Rectangle> safeOpenSideRect(
    GObject* entity,
    CollisionRect* entityBody,
    const PortalFrame& frame,
    Rectangle desired
) {
    bool vertical =
        frame.direction == PortalDirection::LEFT ||
        frame.direction == PortalDirection::RIGHT;
    float bodySize = vertical ? desired.height : desired.width;
    float apertureSize = vertical ? frame.rect.height : frame.rect.width;
    if (bodySize > apertureSize + kApertureTolerance) return std::nullopt;

    float minimum = vertical ? frame.rect.y : frame.rect.x;
    float maximum = minimum + apertureSize - bodySize;
    float requested = std::clamp(
        vertical ? desired.y : desired.x,
        minimum,
        maximum
    );
    desired = placeOnOpenSide(desired, frame);

    std::vector<float> candidates;
    addCandidate(candidates, requested, minimum, maximum);
    addCandidate(candidates, minimum, minimum, maximum);
    addCandidate(candidates, maximum, minimum, maximum);

    Rectangle searchBand = desired;
    if (vertical) {
        searchBand.y = minimum;
        searchBand.height = apertureSize;
    } else {
        searchBand.x = minimum;
        searchBand.width = apertureSize;
    }

    for (CollisionRect* obstacle : CollisionRect::query(searchBand, true)) {
        if (!obstacle || obstacle == entityBody) continue;
        GObject* owner = obstacle->getFather();
        if (!owner || owner == entity || dynamic_cast<Portal*>(owner)) continue;

        Rectangle obstacleRect = obstacle->getSurface();
        if (vertical) {
            addCandidate(
                candidates,
                obstacleRect.y - desired.height - kExitClearance,
                minimum,
                maximum
            );
            addCandidate(
                candidates,
                obstacleRect.y + obstacleRect.height + kExitClearance,
                minimum,
                maximum
            );
        } else {
            addCandidate(
                candidates,
                obstacleRect.x - desired.width - kExitClearance,
                minimum,
                maximum
            );
            addCandidate(
                candidates,
                obstacleRect.x + obstacleRect.width + kExitClearance,
                minimum,
                maximum
            );
        }
    }

    std::stable_sort(
        candidates.begin(),
        candidates.end(),
        [requested](float left, float right) {
            return std::fabs(left - requested) < std::fabs(right - requested);
        }
    );

    for (float position : candidates) {
        Rectangle candidate = desired;
        if (vertical) {
            candidate.y = position;
        } else {
            candidate.x = position;
        }
        if (!isBlocked(entity, entityBody, candidate)) return candidate;
    }
    return std::nullopt;
}

void updateProxy(
    std::unique_ptr<CollisionRect>& proxy,
    GObject* entity,
    std::optional<Rectangle> piece,
    bool solid
) {
    if (!piece.has_value()) {
        proxy.reset();
        return;
    }

    if (!proxy) {
        CollisionDesc desc;
        desc.rect = *piece;
        desc.solid = solid;
        proxy = std::make_unique<CollisionRect>(desc, entity);
        proxy->setRenderProxy(true);
        return;
    }

    proxy->setSurface(*piece);
    proxy->setSolid(solid);
}

void refresh(Transit& transit, GObject* entity, Rectangle sourceRect) {
    CollisionRect* body = entity ? entity->getCollisionBody() : nullptr;
    bool solid = body && body->isSolid();
    transit.sourceRect = snapToAperture(sourceRect, transit.sourceFrame);
    transit.targetRect = transformBetween(
        transit.sourceRect,
        transit.sourceFrame,
        transit.targetFrame
    );
    transit.targetRect = snapToAperture(
        transit.targetRect,
        transit.targetFrame
    );
    transit.sourcePiece = openPiece(transit.sourceRect, transit.sourceFrame);
    transit.targetPiece = openPiece(transit.targetRect, transit.targetFrame);
    updateProxy(transit.sourceProxy, entity, transit.sourcePiece, solid);
    updateProxy(transit.targetProxy, entity, transit.targetPiece, solid);
}

bool canBegin(
    Portal* source,
    GObject* entity,
    Rectangle fromRect,
    Rectangle toRect,
    bool requireMovement
) {
    if (!source || !entity || entity->is3DRenderable() ||
        dynamic_cast<Portal*>(entity)) {
        return false;
    }

    Portal* target = source->getLinkedPortal();
    if (!target || !source->getDirection().has_value() ||
        !target->getDirection().has_value()) {
        return false;
    }
    if (Object_m::level_ents_.find(entity->id_) == Object_m::level_ents_.end()) {
        return false;
    }

    auto frame = frameFor(source);
    if (!frame.has_value() ||
        !overlapsAperture(toRect, *frame) ||
        !straddlesPlane(toRect, *frame) ||
        !fitsAperture(toRect, *frame)) {
        return false;
    }

    if (requireMovement &&
        openSideDistance(centerOf(toRect), *frame) >=
            openSideDistance(centerOf(fromRect), *frame) - kCrossEpsilon) {
        return false;
    }
    return true;
}

bool startTransit(Portal* source, GObject* entity, Rectangle fromRect) {
    if (!source || !entity || transits.contains(entity->id_)) return false;

    Portal* target = source->getLinkedPortal();
    auto sourceFrame = frameFor(source);
    auto targetFrame = frameFor(target);
    if (!target || !sourceFrame.has_value() || !targetFrame.has_value()) {
        return false;
    }

    Transit transit;
    transit.entityId = entity->id_;
    transit.source = source;
    transit.target = target;
    transit.sourceFrame = *sourceFrame;
    transit.targetFrame = *targetFrame;
    transit.previousRect = fromRect;
    refresh(transit, entity, fromRect);
    transit.progressAnchor =
        openSideDistance(centerOf(transit.sourceRect), transit.sourceFrame);
    transits.emplace(entity->id_, std::move(transit));
    return true;
}

Portal* crossedPortal(
    GObject* entity,
    Rectangle fromRect,
    Rectangle toRect
) {
    if (!entity || transits.contains(entity->id_)) return nullptr;

    for (CollisionRect* body : CollisionRect::query(toRect, false)) {
        if (!body) continue;
        auto* portal = dynamic_cast<Portal*>(body->getFather());
        if (portal && canBegin(portal, entity, fromRect, toRect, true)) {
            return portal;
        }
    }
    return nullptr;
}

Rectangle worldClipToScreen(Rectangle worldRect) {
    Camera2D camera = Raycam_m::getCam();
    Vector2 topLeft = GetWorldToScreen2D({worldRect.x, worldRect.y}, camera);
    Vector2 bottomRight = GetWorldToScreen2D(
        {worldRect.x + worldRect.width, worldRect.y + worldRect.height},
        camera
    );
    float left = std::floor(std::min(topLeft.x, bottomRight.x));
    float top = std::floor(std::min(topLeft.y, bottomRight.y));
    float right = std::ceil(std::max(topLeft.x, bottomRight.x));
    float bottom = std::ceil(std::max(topLeft.y, bottomRight.y));
    return {left, top, right - left, bottom - top};
}

void drawClipped(GObject* entity, Rectangle fullRect, Rectangle clipRect) {
    if (!entity || !hasArea(clipRect)) return;
    Rectangle screenRect = worldClipToScreen(clipRect);
    if (!hasArea(screenRect)) return;

    BeginScissorMode(
        static_cast<int>(screenRect.x),
        static_cast<int>(screenRect.y),
        static_cast<int>(screenRect.width),
        static_cast<int>(screenRect.height)
    );
    entity->drawAtBody(fullRect);
    EndScissorMode();
}

void applyGravity(RigidBody* body, Portal* target) {
    if (!body || !target || !target->forcesGravity() ||
        !target->getDirection().has_value()) {
        return;
    }
    body->setGravityDirection(gravityFor(*target->getDirection()));
}

void applyExit(Transit& transit, GObject*, CollisionRect* body) {
    if (!body) return;

    body->setSurface(transit.targetRect);
    if (auto* rigidBody = dynamic_cast<RigidBody*>(body)) {
        Vector2 speed = rotateBetween(
            rigidBody->getSpeed(),
            transit.sourceFrame,
            transit.targetFrame
        );
        applyGravity(rigidBody, transit.target);
        rigidBody->setSpeed(speed);
    }
}

void applyCarriedExit(Transit& transit, CollisionRect* body) {
    if (!body || body->isRenderProxy()) return;
    GObject* carried = body->getFather();
    if (!carried || carried == transit.source || carried == transit.target) return;
    if (carried->id_ == transit.entityId) return;
    if (Object_m::level_ents_.find(carried->id_) == Object_m::level_ents_.end()) {
        return;
    }

    transits.erase(carried->id_);
    body->setSurface(
        transformBetween(
            body->getSurface(),
            transit.sourceFrame,
            transit.targetFrame
        )
    );
    if (auto* rigidBody = dynamic_cast<RigidBody*>(body)) {
        Vector2 speed = rotateBetween(
            rigidBody->getSpeed(),
            transit.sourceFrame,
            transit.targetFrame
        );
        applyGravity(rigidBody, transit.target);
        rigidBody->setSpeed(speed);
    }
}

void ensureExitVelocity(CollisionRect* body, const PortalFrame& frame) {
    auto* rigidBody = dynamic_cast<RigidBody*>(body);
    if (!rigidBody) return;

    Vector2 speed = rigidBody->getSpeed();
    float outwardSpeed = dot(speed, frame.normal);
    if (outwardSpeed >= kMinimumExitSpeed) return;

    float correction = kMinimumExitSpeed - outwardSpeed;
    speed.x += frame.normal.x * correction;
    speed.y += frame.normal.y * correction;
    rigidBody->setSpeed(speed);
}

RecoveryResult recover(
    Transit& transit,
    GObject* entity,
    CollisionRect* body,
    const std::vector<CollisionRect*>& carriedBodies,
    bool preferTarget
) {
    auto tryTarget = [&]() {
        auto safeRect = safeOpenSideRect(
            entity,
            body,
            transit.targetFrame,
            transit.targetRect
        );
        if (!safeRect.has_value()) return false;

        transit.targetRect = *safeRect;
        for (CollisionRect* carriedBody : carriedBodies) {
            applyCarriedExit(transit, carriedBody);
        }
        applyExit(transit, entity, body);
        ensureExitVelocity(body, transit.targetFrame);
        return true;
    };

    auto trySource = [&]() {
        auto safeRect = safeOpenSideRect(
            entity,
            body,
            transit.sourceFrame,
            transit.sourceRect
        );
        if (!safeRect.has_value()) return false;

        body->setSurface(*safeRect);
        ensureExitVelocity(body, transit.sourceFrame);
        return true;
    };

    bool recovered = preferTarget
        ? tryTarget() || trySource()
        : trySource() || tryTarget();
    if (recovered) return RecoveryResult::Recovered;
    if (dynamic_cast<Character*>(entity)) return RecoveryResult::Respawn;

    body->setSurface(transit.previousRect);
    if (auto* rigidBody = dynamic_cast<RigidBody*>(body)) {
        rigidBody->setSpeed({});
    }
    return RecoveryResult::Recovered;
}

bool hasStalled(
    Transit& transit,
    CollisionRect* body,
    const PortalFrame& sourceFrame,
    double delta
) {
    delta = std::isfinite(delta) ? std::clamp(delta, 0.0, 0.2) : 0.0;
    transit.elapsed += delta;
    if (transit.elapsed >= kMaximumTransitTime) return true;

    float distance = openSideDistance(centerOf(body->getSurface()), sourceFrame);
    if (std::fabs(distance - transit.progressAnchor) >= kProgressDistance) {
        transit.progressAnchor = distance;
        transit.stalledFor = 0.0;
        return false;
    }

    transit.stalledFor += delta;
    return transit.stalledFor >= kStallTimeout;
}

void finishTransit(int entityId, bool respawn) {
    transits.erase(entityId);
    if (!respawn) return;

    auto entityIt = Object_m::level_ents_.find(entityId);
    if (entityIt == Object_m::level_ents_.end()) return;
    if (auto* character = dynamic_cast<Character*>(entityIt->second.get())) {
        character->respawn();
    }
}

} // namespace

void Portal_m::registerPortal(Portal* portal) {
    if (!portal) return;
    portals[portal->id_] = portal;
    openPortalTiles(portal);
}

void Portal_m::unregisterPortal(Portal* portal) {
    if (!portal) return;

    for (auto it = transits.begin(); it != transits.end();) {
        if (it->second.source == portal || it->second.target == portal) {
            it = transits.erase(it);
        } else {
            ++it;
        }
    }
    releasePortalTiles(portal->id_);
    portalTiles.erase(portal->id_);

    auto portalIt = portals.find(portal->id_);
    if (portalIt != portals.end() && portalIt->second == portal) {
        portals.erase(portalIt);
    }
}

void Portal_m::setupLinks() {
    for (const auto& [_, portal] : portals) {
        if (!portal) continue;
        openPortalTiles(portal);
        portal->resolveLinkTarget();
    }
}

void Portal_m::restoreTiles() {
    for (const auto& [tileId, disabled] : disabledTiles) {
        auto tileIt = Object_m::level_tiles_.find(tileId);
        if (tileIt == Object_m::level_tiles_.end() || !tileIt->second) continue;
        if (CollisionRect* body = tileIt->second->getCollisionBody()) {
            body->setSolid(disabled.originalSolid);
        }
    }
    disabledTiles.clear();
    for (auto& [_, tileIds] : portalTiles) {
        tileIds.clear();
    }
}

bool Portal_m::beginTransit(Portal* source, GObject* entity) {
    if (!source || !entity) return false;
    if (transits.contains(entity->id_)) return true;

    CollisionRect* body = entity->getCollisionBody();
    if (!body) return false;
    Rectangle rect = body->getSurface();
    if (!canBegin(source, entity, rect, rect, false)) return false;
    return startTransit(source, entity, rect);
}

void Portal_m::prepareMovement(
    GObject* entity,
    CollisionRect* body,
    Rectangle fromRect,
    Rectangle toRect
) {
    if (!entity || !body || transits.contains(entity->id_)) return;
    Portal* source = crossedPortal(entity, fromRect, toRect);
    if (!source) return;
    startTransit(source, entity, fromRect);
}

bool Portal_m::isMovementBlocked(
    GObject* entity,
    CollisionRect* body,
    Rectangle proposedRect
) {
    if (!entity || !body) return false;

    auto transitIt = transits.find(entity->id_);
    if (transitIt == transits.end()) {
        return isBlocked(entity, body, proposedRect);
    }

    Transit& transit = transitIt->second;
    Rectangle sourceRect = snapToAperture(
        proposedRect,
        transit.sourceFrame
    );
    Rectangle targetRect = snapToAperture(
        transformBetween(
            sourceRect,
            transit.sourceFrame,
            transit.targetFrame
        ),
        transit.targetFrame
    );
    auto sourcePiece = openPiece(sourceRect, transit.sourceFrame);
    auto targetPiece = openPiece(targetRect, transit.targetFrame);

    bool blocked =
        (sourcePiece.has_value() &&
         !fitsAperture(sourceRect, transit.sourceFrame)) ||
        (sourcePiece.has_value() && targetPiece.has_value() &&
         !fitsAperture(targetRect, transit.targetFrame)) ||
        (sourcePiece.has_value() &&
         isBlocked(entity, body, *sourcePiece, &transit)) ||
        (targetPiece.has_value() &&
         isBlocked(entity, body, *targetPiece, &transit));

    if (blocked &&
        movesIntoPortal(body->getSurface(), sourceRect, transit.sourceFrame)) {
        transit.blockedEntering = true;
    }
    return blocked;
}

bool Portal_m::isProbeBlocked(
    GObject* entity,
    CollisionRect* body,
    Rectangle probeRect
) {
    if (!entity || !body) return false;

    auto transitIt = transits.find(entity->id_);
    if (transitIt == transits.end()) {
        return isBlocked(entity, body, probeRect);
    }

    Transit& transit = transitIt->second;
    Rectangle sourceRect = snapToAperture(probeRect, transit.sourceFrame);
    Rectangle targetRect = snapToAperture(
        transformBetween(
            sourceRect,
            transit.sourceFrame,
            transit.targetFrame
        ),
        transit.targetFrame
    );
    auto sourcePiece = openPiece(sourceRect, transit.sourceFrame);
    auto targetPiece = openPiece(targetRect, transit.targetFrame);
    return
        (sourcePiece.has_value() &&
         isBlocked(entity, body, *sourcePiece, &transit)) ||
        (targetPiece.has_value() &&
         isBlocked(entity, body, *targetPiece, &transit));
}

bool Portal_m::separateCollisions(GObject* entity, CollisionRect* body) {
    if (!entity || !body) return false;

    auto transitIt = transits.find(entity->id_);
    if (transitIt == transits.end()) return false;

    Transit& transit = transitIt->second;

    bool separated = false;
    constexpr int maxAttempts = 20;
    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        refresh(transit, entity, body->getSurface());

        struct Piece {
            std::optional<Rectangle> rect;
            bool targetSide;
        };
        Piece pieces[] = {
            {transit.sourcePiece, false},
            {transit.targetPiece, true}
        };

        bool moved = false;
        for (const Piece& piece : pieces) {
            if (!piece.rect.has_value()) continue;

            for (CollisionRect* obstacle :
                 CollisionRect::query(*piece.rect, true)) {
                if (!obstacle || obstacle == body ||
                    obstacle == transit.sourceProxy.get() ||
                    obstacle == transit.targetProxy.get() ||
                    isActiveMainBody(obstacle)) {
                    continue;
                }

                GObject* owner = obstacle->getFather();
                if (!owner || owner == entity ||
                    !owner->blocksMovementFor(entity) ||
                    dynamic_cast<Portal*>(owner) ||
                    ignoresAperture(*piece.rect, obstacle)) {
                    continue;
                }
                if (OneWayPlatform::isOneWayPlatformBody(obstacle) &&
                    !OneWayPlatform::blocksBody(body, *piece.rect, obstacle)) {
                    continue;
                }

                Rectangle obstacleRect = obstacle->getSurface();
                if (!CheckCollisionRecs(*piece.rect, obstacleRect)) continue;

                float overlapLeft =
                    piece.rect->x + piece.rect->width - obstacleRect.x;
                float overlapRight =
                    obstacleRect.x + obstacleRect.width - piece.rect->x;
                float overlapTop =
                    piece.rect->y + piece.rect->height - obstacleRect.y;
                float overlapBottom =
                    obstacleRect.y + obstacleRect.height - piece.rect->y;
                float overlapX = std::min(overlapLeft, overlapRight);
                float overlapY = std::min(overlapTop, overlapBottom);
                Vector2 pieceCenter = centerOf(*piece.rect);
                Vector2 obstacleCenter = centerOf(obstacleRect);
                Vector2 separation{};

                if (overlapX < overlapY) {
                    separation.x = pieceCenter.x < obstacleCenter.x
                        ? -overlapX - 0.01f
                        : overlapX + 0.01f;
                } else {
                    separation.y = pieceCenter.y < obstacleCenter.y
                        ? -overlapY - 0.01f
                        : overlapY + 0.01f;
                }

                if (piece.targetSide) {
                    Rectangle targetRect = transit.targetRect;
                    targetRect.x += separation.x;
                    targetRect.y += separation.y;
                    body->setSurface(
                        transformBetween(
                            targetRect,
                            transit.targetFrame,
                            transit.sourceFrame
                        )
                    );
                } else {
                    Rectangle sourceRect = body->getSurface();
                    sourceRect.x += separation.x;
                    sourceRect.y += separation.y;
                    body->setSurface(sourceRect);
                }

                separated = true;
                moved = true;
                break;
            }
            if (moved) break;
        }
        if (!moved) break;
    }

    refresh(transit, entity, body->getSurface());
    return separated;
}

bool Portal_m::sync(
    GObject* entity,
    const std::vector<CollisionRect*>& carriedBodies
) {
    if (!entity) return false;

    auto transitIt = transits.find(entity->id_);
    if (transitIt == transits.end()) return false;

    Transit& transit = transitIt->second;
    CollisionRect* body = entity->getCollisionBody();
    if (!body || !transit.source || !transit.target) {
        bool respawn = dynamic_cast<Character*>(entity) != nullptr;
        finishTransit(entity->id_, respawn);
        return respawn;
    }
    refresh(transit, entity, body->getSurface());

    if (!transit.sourcePiece.has_value()) {
        for (CollisionRect* carriedBody : carriedBodies) {
            applyCarriedExit(transit, carriedBody);
        }
        applyExit(transit, entity, body);
        finishTransit(entity->id_, false);
        return true;
    }

    if (transit.blockedEntering) {
        bool respawn =
            recover(transit, entity, body, carriedBodies, true) ==
            RecoveryResult::Respawn;
        finishTransit(entity->id_, respawn);
        return true;
    }

    if (!transit.targetPiece.has_value() &&
        !overlapsAperture(body->getSurface(), transit.sourceFrame)) {
        finishTransit(entity->id_, false);
        return false;
    }

    transit.previousRect = body->getSurface();
    return false;
}

std::optional<Vector2> Portal_m::gravityStep(
    GObject* entity,
    GravityDirection currentDirection,
    double acceleration,
    float delta
) {
    if (!entity || delta <= 0.0f) return std::nullopt;

    auto transitIt = transits.find(entity->id_);
    if (transitIt == transits.end()) return std::nullopt;

    Transit& transit = transitIt->second;
    CollisionRect* body = entity->getCollisionBody();
    if (!body) return std::nullopt;

    if (openSideDistance(
            centerOf(body->getSurface()),
            transit.sourceFrame
        ) >= 0.0f) {
        Vector2 gravity = gravityVector(currentDirection, acceleration);
        return Vector2{gravity.x * delta, gravity.y * delta};
    }

    GravityDirection targetDirection = currentDirection;
    if (transit.target->forcesGravity() &&
        transit.target->getDirection().has_value()) {
        targetDirection = gravityFor(*transit.target->getDirection());
    }
    Vector2 targetGravity = gravityVector(targetDirection, acceleration);
    Vector2 sourceGravity = rotateBetween(
        targetGravity,
        transit.targetFrame,
        transit.sourceFrame
    );
    return Vector2{sourceGravity.x * delta, sourceGravity.y * delta};
}

void Portal_m::update(double delta) {
    std::vector<PendingEnd> endings;

    for (auto& [entityId, transit] : transits) {
        auto entityIt = Object_m::level_ents_.find(entityId);
        if (entityIt == Object_m::level_ents_.end()) {
            endings.push_back({entityId, false});
            continue;
        }

        GObject* entity = entityIt->second.get();
        CollisionRect* body = entity ? entity->getCollisionBody() : nullptr;
        if (!entity || !body || !transit.source || !transit.target) {
            endings.push_back({
                entityId,
                dynamic_cast<Character*>(entity) != nullptr
            });
            continue;
        }
        refresh(transit, entity, body->getSurface());

        if (!transit.sourcePiece.has_value()) {
            applyExit(transit, entity, body);
            endings.push_back({entityId, false});
            continue;
        }

        if (transit.blockedEntering) {
            bool respawn =
                recover(transit, entity, body, {}, true) ==
                RecoveryResult::Respawn;
            endings.push_back({entityId, respawn});
            continue;
        }

        if (hasStalled(transit, body, transit.sourceFrame, delta)) {
            bool preferTarget =
                openSideDistance(
                    centerOf(body->getSurface()),
                    transit.sourceFrame
                ) < 0.0f;
            bool respawn =
                recover(transit, entity, body, {}, preferTarget) ==
                RecoveryResult::Respawn;
            endings.push_back({entityId, respawn});
            continue;
        }

        if (!transit.targetPiece.has_value() &&
            !overlapsAperture(body->getSurface(), transit.sourceFrame)) {
            endings.push_back({entityId, false});
            continue;
        }

        transit.previousRect = body->getSurface();
    }

    for (const PendingEnd& ending : endings) {
        finishTransit(ending.entityId, ending.respawn);
    }
}

void Portal_m::cancel(GObject* entity) {
    if (entity) transits.erase(entity->id_);
}

void Portal_m::clear() {
    transits.clear();
}

bool Portal_m::isInTransit(GObject* entity) {
    return entity && transits.contains(entity->id_);
}

std::optional<Rectangle> Portal_m::sourceSurface(GObject* entity) {
    if (!entity) return std::nullopt;
    auto it = transits.find(entity->id_);
    if (it == transits.end()) return std::nullopt;
    return it->second.sourceRect;
}

std::optional<Rectangle> Portal_m::targetSurface(GObject* entity) {
    if (!entity) return std::nullopt;
    auto it = transits.find(entity->id_);
    if (it == transits.end()) return std::nullopt;
    return it->second.targetRect;
}

std::vector<Rectangle> Portal_m::visibleSurfaces(GObject* entity) {
    if (!entity) return {};
    auto it = transits.find(entity->id_);
    if (it == transits.end()) {
        if (CollisionRect* body = entity->getCollisionBody()) {
            return {body->getSurface()};
        }
        return {};
    }

    std::vector<Rectangle> surfaces;
    if (it->second.sourcePiece.has_value()) {
        surfaces.push_back(*it->second.sourcePiece);
    }
    if (it->second.targetPiece.has_value()) {
        surfaces.push_back(*it->second.targetPiece);
    }
    return surfaces;
}

std::optional<Vector2> Portal_m::visibleCenter(GObject* entity) {
    if (!entity) return std::nullopt;
    auto it = transits.find(entity->id_);
    if (it == transits.end()) return std::nullopt;

    std::optional<Rectangle> largest;
    float largestArea = 0.0f;
    auto consider = [&](std::optional<Rectangle> rect) {
        if (!rect.has_value()) return;
        float area = rect->width * rect->height;
        if (area < largestArea) return;
        largest = rect;
        largestArea = area;
    };
    consider(it->second.sourcePiece);
    consider(it->second.targetPiece);
    if (!largest.has_value()) return std::nullopt;
    return centerOf(*largest);
}

bool Portal_m::isSourceVisible(GObject* entity, Rectangle rect) {
    if (!entity) return false;
    auto it = transits.find(entity->id_);
    if (it == transits.end()) return true;
    return it->second.sourcePiece.has_value() &&
           CheckCollisionRecs(rect, *it->second.sourcePiece);
}

std::optional<Rectangle> Portal_m::transformRect(
    GObject* entity,
    Rectangle rect
) {
    if (!entity) return std::nullopt;
    auto it = transits.find(entity->id_);
    if (it == transits.end()) return std::nullopt;

    return transformBetween(
        rect,
        it->second.sourceFrame,
        it->second.targetFrame
    );
}

void Portal_m::drawForLayer(int layer) {
    for (const auto& [entityId, transit] : transits) {
        auto entityIt = Object_m::level_ents_.find(entityId);
        if (entityIt == Object_m::level_ents_.end()) continue;

        GObject* entity = entityIt->second.get();
        if (!entity || entity->layer_ != layer) continue;
        if (transit.sourcePiece.has_value()) {
            drawClipped(entity, transit.sourceRect, *transit.sourcePiece);
        }
        if (transit.targetPiece.has_value()) {
            drawClipped(entity, transit.targetRect, *transit.targetPiece);
        }
    }
}

bool Portal_m::shouldIgnoreCollision(
    GObject* moving,
    CollisionRect* obstacle
) {
    if (!moving || !obstacle) return false;

    GObject* obstacleOwner = obstacle->getFather();
    if (dynamic_cast<Portal*>(obstacleOwner) || isActiveMainBody(obstacle)) {
        return true;
    }

    auto transitIt = transits.find(moving->id_);
    if (transitIt != transits.end()) {
        Transit& transit = transitIt->second;
        if (obstacle == transit.sourceProxy.get() ||
            obstacle == transit.targetProxy.get() ||
            !obstacleOwner ||
            obstacleOwner == moving) {
            return true;
        }

        Rectangle obstacleRect = obstacle->getSurface();
        bool touchesVisiblePiece = false;
        if (transit.sourcePiece.has_value() &&
            CheckCollisionRecs(*transit.sourcePiece, obstacleRect)) {
            if (ignoresAperture(*transit.sourcePiece, obstacle)) return true;
            touchesVisiblePiece = true;
        }
        if (transit.targetPiece.has_value() &&
            CheckCollisionRecs(*transit.targetPiece, obstacleRect)) {
            if (ignoresAperture(*transit.targetPiece, obstacle)) return true;
            touchesVisiblePiece = true;
        }
        return !touchesVisiblePiece;
    }

    CollisionRect* body = moving->getCollisionBody();
    return body && ignoresAperture(body->getSurface(), obstacle);
}
