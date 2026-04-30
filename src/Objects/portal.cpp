#include "portal.h"
#include "character.h"
#include "rigidBody.h"
#include "raycam_m.h"
#include "object_m.h"
#include "ldtk_m.h"
#include "collisionRect.h"
#include "oneWayPlatform.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <memory>
#include <optional>
#include <vector>

Portal* Portal::playerSpawnedPortal_ = nullptr;

namespace {

constexpr float kMinPieceSize = 0.01f;
constexpr float kCrossEpsilon = 0.001f;
constexpr float kApertureEpsilon = 0.05f;

struct PortalFrame {
    Rectangle rect{0, 0, 0, 0};
    PortalDirection direction = PortalDirection::RIGHT;
    Vector2 normal{1.0f, 0.0f};
    Vector2 tangent{0.0f, 1.0f};
    Vector2 planeCenter{0.0f, 0.0f};
    float plane = 0.0f;
};

struct PortalTransit {
    int entityId = -1;
    Portal* source = nullptr;
    Portal* target = nullptr;
    Rectangle previousFullRect{0, 0, 0, 0};
    Rectangle sourceFullRect{0, 0, 0, 0};
    Rectangle targetFullRect{0, 0, 0, 0};
    std::unique_ptr<CollisionRect> sourcePieceBody;
    std::unique_ptr<CollisionRect> targetPieceBody;
    std::optional<Rectangle> sourcePiece;
    std::optional<Rectangle> targetPiece;
};

struct DisabledPortalTile {
    bool originalSolid = true;
    int refs = 0;
};

std::map<int, PortalTransit> activeTransits;
std::map<int, DisabledPortalTile> disabledPortalTiles;

Vector2 portalNormal(PortalDirection direction) {
    switch (direction) {
        case PortalDirection::UP: return {0.0f, -1.0f};
        case PortalDirection::DOWN: return {0.0f, 1.0f};
        case PortalDirection::LEFT: return {-1.0f, 0.0f};
        case PortalDirection::RIGHT: return {1.0f, 0.0f};
    }
    return {1.0f, 0.0f};
}

Vector2 portalTangent(PortalDirection direction) {
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

Vector2 rectCenter(Rectangle rect) {
    return {rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
}

float dot(Vector2 a, Vector2 b) {
    return a.x * b.x + a.y * b.y;
}

bool hasArea(Rectangle rect) {
    return rect.width > kMinPieceSize && rect.height > kMinPieceSize;
}

std::optional<Rectangle> intersectionRect(Rectangle a, Rectangle b) {
    float left = std::max(a.x, b.x);
    float top = std::max(a.y, b.y);
    float right = std::min(a.x + a.width, b.x + b.width);
    float bottom = std::min(a.y + a.height, b.y + b.height);
    Rectangle intersection{left, top, right - left, bottom - top};
    if (!hasArea(intersection)) return std::nullopt;
    return intersection;
}

Rectangle moveRectCenter(Rectangle rect, Vector2 center) {
    rect.x = center.x - rect.width / 2.0f;
    rect.y = center.y - rect.height / 2.0f;
    return rect;
}

std::optional<PortalFrame> makeFrame(Portal* portal) {
    if (!portal || !portal->getDirection().has_value()) return std::nullopt;

    PortalFrame frame;
    frame.rect = portal->getPortalRect();
    frame.direction = *portal->getDirection();
    frame.normal = portalNormal(frame.direction);
    frame.tangent = portalTangent(frame.direction);

    switch (frame.direction) {
        case PortalDirection::RIGHT:
            frame.plane = frame.rect.x + frame.rect.width;
            frame.planeCenter = {frame.plane, frame.rect.y + frame.rect.height / 2.0f};
            break;
        case PortalDirection::LEFT:
            frame.plane = frame.rect.x;
            frame.planeCenter = {frame.plane, frame.rect.y + frame.rect.height / 2.0f};
            break;
        case PortalDirection::DOWN:
            frame.plane = frame.rect.y + frame.rect.height;
            frame.planeCenter = {frame.rect.x + frame.rect.width / 2.0f, frame.plane};
            break;
        case PortalDirection::UP:
            frame.plane = frame.rect.y;
            frame.planeCenter = {frame.rect.x + frame.rect.width / 2.0f, frame.plane};
            break;
    }

    return frame;
}

float signedDistanceToOpenSide(Vector2 point, const PortalFrame& frame) {
    return dot({point.x - frame.planeCenter.x, point.y - frame.planeCenter.y}, frame.normal);
}

bool overlapsAperture(Rectangle rect, const PortalFrame& frame) {
    return CheckCollisionRecs(rect, frame.rect);
}

std::optional<Rectangle> openSidePiece(Rectangle rect, const PortalFrame& frame) {
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

std::optional<Rectangle> insideSidePiece(Rectangle rect, const PortalFrame& frame) {
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

bool straddlesPortalPlane(Rectangle rect, const PortalFrame& frame) {
    return openSidePiece(rect, frame).has_value() && insideSidePiece(rect, frame).has_value();
}

bool fitsApertureTangent(Rectangle rect, const PortalFrame& frame) {
    if (frame.direction == PortalDirection::LEFT || frame.direction == PortalDirection::RIGHT) {
        return rect.y >= frame.rect.y - kApertureEpsilon &&
               rect.y + rect.height <= frame.rect.y + frame.rect.height + kApertureEpsilon;
    }

    return rect.x >= frame.rect.x - kApertureEpsilon &&
           rect.x + rect.width <= frame.rect.x + frame.rect.width + kApertureEpsilon;
}

Rectangle transformRectBetweenFrames(Rectangle rect, const PortalFrame& from, const PortalFrame& to) {
    Vector2 center = rectCenter(rect);
    Vector2 delta{center.x - from.planeCenter.x, center.y - from.planeCenter.y};
    float localOpen = dot(delta, from.normal);
    float localTangent = dot(delta, from.tangent);
    Vector2 mappedCenter{
        to.planeCenter.x + to.normal.x * -localOpen + to.tangent.x * localTangent,
        to.planeCenter.y + to.normal.y * -localOpen + to.tangent.y * localTangent
    };
    return moveRectCenter(rect, mappedCenter);
}

Vector2 rotateVectorBetweenFrames(Vector2 value, const PortalFrame& from, const PortalFrame& to) {
    float localOpen = dot(value, from.normal);
    float localTangent = dot(value, from.tangent);
    return {
        to.normal.x * -localOpen + to.tangent.x * localTangent,
        to.normal.y * -localOpen + to.tangent.y * localTangent
    };
}

Vector2 gravityVector(GravityDirection direction, double acceleration) {
    float amount = static_cast<float>(acceleration);
    switch (direction) {
        case GravityDirection::DOWN:
            return {0.0f, amount};
        case GravityDirection::UP:
            return {0.0f, -amount};
        case GravityDirection::LEFT:
            return {-amount, 0.0f};
        case GravityDirection::RIGHT:
            return {amount, 0.0f};
    }
    return {0.0f, amount};
}

GravityDirection gravityDirectionFromPortalDirection(PortalDirection direction) {
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

bool shouldIgnoreApertureCollision(Rectangle movingRect, CollisionRect* obstacle) {
    if (!obstacle) return false;
    GObject* owner = obstacle->getFather();
    if (dynamic_cast<Portal*>(owner)) return true;

    Rectangle obstacleRect = obstacle->getSurface();
    std::optional<Rectangle> collisionArea = intersectionRect(movingRect, obstacleRect);
    if (!collisionArea.has_value()) return false;

    for (auto& [_, obj] : Object_m::level_ents_) {
        Portal* portal = dynamic_cast<Portal*>(obj.get());
        if (!portal || !portal->getLinkedPortal()) continue;
        auto frame = makeFrame(portal);
        if (!frame.has_value()) continue;

        if (frame->direction == PortalDirection::LEFT || frame->direction == PortalDirection::RIGHT) {
            float apertureTop = frame->rect.y - kApertureEpsilon;
            float apertureBottom = frame->rect.y + frame->rect.height + kApertureEpsilon;
            if (collisionArea->y >= apertureTop &&
                collisionArea->y + collisionArea->height <= apertureBottom &&
                CheckCollisionRecs(*collisionArea, frame->rect)) {
                return true;
            }
        } else {
            float apertureLeft = frame->rect.x - kApertureEpsilon;
            float apertureRight = frame->rect.x + frame->rect.width + kApertureEpsilon;
            if (collisionArea->x >= apertureLeft &&
                collisionArea->x + collisionArea->width <= apertureRight &&
                CheckCollisionRecs(*collisionArea, frame->rect)) {
                return true;
            }
        }
    }

    return false;
}

void releaseDisabledTiles(std::vector<int>& tileIds) {
    for (int tileId : tileIds) {
        auto disabledIt = disabledPortalTiles.find(tileId);
        if (disabledIt == disabledPortalTiles.end()) continue;

        disabledIt->second.refs -= 1;
        if (disabledIt->second.refs > 0) continue;

        auto tileIt = Object_m::level_tiles_.find(tileId);
        if (tileIt != Object_m::level_tiles_.end() && tileIt->second) {
            CollisionRect* body = tileIt->second->getCollisionBody();
            if (body) body->setSolid(disabledIt->second.originalSolid);
        }
        disabledPortalTiles.erase(disabledIt);
    }
    tileIds.clear();
}

void disableTilesUnderPortal(Portal* portal, std::vector<int>& tileIds) {
    auto frame = makeFrame(portal);
    if (!frame.has_value()) return;

    releaseDisabledTiles(tileIds);

    for (CollisionRect* col : CollisionRect::query(frame->rect, false)) {
        if (!col || !CheckCollisionRecs(col->getSurface(), frame->rect)) continue;

        GObject* owner = col->getFather();
        if (!owner) continue;
        if (Object_m::level_tiles_.find(owner->id_) == Object_m::level_tiles_.end()) continue;

        auto [it, inserted] = disabledPortalTiles.emplace(
            owner->id_,
            DisabledPortalTile{col->isSolid(), 0}
        );
        it->second.refs += 1;
        tileIds.push_back(owner->id_);
        col->setSolid(false);
    }
}

bool isActiveMainBody(CollisionRect* obstacle) {
    if (!obstacle || obstacle->isRenderProxy()) return false;
    GObject* owner = obstacle->getFather();
    return owner && activeTransits.find(owner->id_) != activeTransits.end();
}

bool pieceBlocked(
    const PortalTransit* transit,
    GObject* entity,
    CollisionRect* entityBody,
    Rectangle piece
) {
    if (!entityBody || !entityBody->isSolid()) return false;

    for (CollisionRect* col : CollisionRect::query(piece, true)) {
        if (!col || col == entityBody) continue;
        if (transit && (col == transit->sourcePieceBody.get() || col == transit->targetPieceBody.get())) continue;
        if (isActiveMainBody(col)) continue;

        GObject* owner = col->getFather();
        if (!owner || owner == entity) continue;
        if (dynamic_cast<Portal*>(owner)) continue;
        if (shouldIgnoreApertureCollision(piece, col)) continue;
        if (OneWayPlatform::isOneWayPlatformBody(col) &&
            !OneWayPlatform::blocksBody(entityBody, piece, col)) continue;
        if (CheckCollisionRecs(piece, col->getSurface())) return true;
    }

    return false;
}

bool standardBlocked(GObject* entity, CollisionRect* entityBody, Rectangle proposedRect) {
    if (!entityBody || !entityBody->isSolid()) return false;

    for (CollisionRect* col : CollisionRect::query(proposedRect, true)) {
        if (!col || col == entityBody) continue;
        if (isActiveMainBody(col)) continue;
        GObject* owner = col->getFather();
        if (!owner || owner == entity) continue;
        if (dynamic_cast<Portal*>(owner)) continue;
        if (shouldIgnoreApertureCollision(proposedRect, col)) continue;
        if (OneWayPlatform::isOneWayPlatformBody(col) &&
            !OneWayPlatform::blocksBody(entityBody, proposedRect, col)) continue;
        if (col->isSolid() && CheckCollisionRecs(proposedRect, col->getSurface())) return true;
    }

    return false;
}

void updatePieceBody(std::unique_ptr<CollisionRect>& body, GObject* entity, std::optional<Rectangle> piece, bool solid) {
    if (!piece.has_value()) {
        body.reset();
        return;
    }

    if (!body) {
        CollisionDesc desc;
        desc.rect = *piece;
        desc.solid = solid;
        body = std::make_unique<CollisionRect>(desc, entity);
        body->setRenderProxy(true);
    } else {
        body->setSurface(*piece);
        body->setSolid(solid);
    }
}

void refreshTransitPieces(PortalTransit& transit, GObject* entity, Rectangle sourceRect) {
    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) return;

    CollisionRect* body = entity ? entity->getCollisionBody() : nullptr;
    bool solid = body ? body->isSolid() : false;

    transit.sourceFullRect = sourceRect;
    transit.targetFullRect = transformRectBetweenFrames(sourceRect, *sourceFrame, *targetFrame);
    transit.sourcePiece = openSidePiece(transit.sourceFullRect, *sourceFrame);
    transit.targetPiece = openSidePiece(transit.targetFullRect, *targetFrame);
    updatePieceBody(transit.sourcePieceBody, entity, transit.sourcePiece, solid);
    updatePieceBody(transit.targetPieceBody, entity, transit.targetPiece, solid);
}

bool canStartTransit(Portal* portal, GObject* entity, Rectangle fromRect, Rectangle toRect, bool requireMovementIntoPortal) {
    if (!portal || !entity || entity->is3DRenderable() || dynamic_cast<Portal*>(entity)) return false;

    Portal* linkedPortal = portal->getLinkedPortal();
    if (!linkedPortal || !portal->getDirection().has_value() || !linkedPortal->getDirection().has_value()) return false;
    if (Object_m::level_ents_.find(entity->id_) == Object_m::level_ents_.end()) return false;

    auto frame = makeFrame(portal);
    if (!frame.has_value()) return false;
    if (!overlapsAperture(toRect, *frame) || !straddlesPortalPlane(toRect, *frame)) return false;
    if (!fitsApertureTangent(toRect, *frame)) return false;

    if (requireMovementIntoPortal) {
        float fromDistance = signedDistanceToOpenSide(rectCenter(fromRect), *frame);
        float toDistance = signedDistanceToOpenSide(rectCenter(toRect), *frame);
        if (toDistance >= fromDistance - kCrossEpsilon) return false;
    }

    return true;
}

bool beginTransit(Portal* portal, GObject* entity, Rectangle fromRect) {
    if (!portal || !entity) return false;
    if (activeTransits.find(entity->id_) != activeTransits.end()) return true;
    Portal* linkedPortal = portal->getLinkedPortal();
    if (!linkedPortal || !portal->getDirection().has_value() || !linkedPortal->getDirection().has_value()) return false;
    if (entity->is3DRenderable() || dynamic_cast<Portal*>(entity)) return false;
    if (Object_m::level_ents_.find(entity->id_) == Object_m::level_ents_.end()) return false;

    PortalTransit transit;
    transit.entityId = entity->id_;
    transit.source = portal;
    transit.target = portal->getLinkedPortal();
    transit.previousFullRect = fromRect;
    refreshTransitPieces(transit, entity, fromRect);

    activeTransits.emplace(entity->id_, std::move(transit));
    return true;
}

Portal* findCrossedPortal(GObject* entity, Rectangle fromRect, Rectangle toRect) {
    for (CollisionRect* body : CollisionRect::query(toRect, false)) {
        if (!body) continue;
        Portal* portal = dynamic_cast<Portal*>(body->getFather());
        if (!portal) continue;
        if (activeTransits.find(entity->id_) != activeTransits.end()) return nullptr;
        if (canStartTransit(portal, entity, fromRect, toRect, true)) return portal;
    }
    return nullptr;
}

Rectangle worldClipToScreenClip(Rectangle worldClip) {
    Camera2D camera = Raycam_m::getCam();
    Vector2 topLeft = GetWorldToScreen2D({worldClip.x, worldClip.y}, camera);
    Vector2 bottomRight = GetWorldToScreen2D(
        {worldClip.x + worldClip.width, worldClip.y + worldClip.height},
        camera
    );
    float xMin = std::min(topLeft.x, bottomRight.x);
    float yMin = std::min(topLeft.y, bottomRight.y);
    float xMax = std::max(topLeft.x, bottomRight.x);
    float yMax = std::max(topLeft.y, bottomRight.y);

    float pixelLeft = std::floor(xMin);
    float pixelTop = std::floor(yMin);
    float pixelRight = std::ceil(xMax);
    float pixelBottom = std::ceil(yMax);
    return {pixelLeft, pixelTop, pixelRight - pixelLeft, pixelBottom - pixelTop};
}

void drawClipped(GObject* entity, Rectangle fullRect, Rectangle clipRect) {
    if (!entity || !hasArea(clipRect)) return;
    Rectangle screenClip = worldClipToScreenClip(clipRect);
    if (!hasArea(screenClip)) return;

    BeginScissorMode(
        (int)screenClip.x,
        (int)screenClip.y,
        (int)screenClip.width,
        (int)screenClip.height
    );
    entity->drawAtBody(fullRect);
    EndScissorMode();
}

void applyExitState(PortalTransit& transit, GObject* entity, CollisionRect* entityBody) {
    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) return;

    entityBody->setSurface(transit.targetFullRect);

    RigidBody* rigidBody = dynamic_cast<RigidBody*>(entityBody);
    if (rigidBody) {
        Vector2 speed = rotateVectorBetweenFrames(rigidBody->getSpeed(), *sourceFrame, *targetFrame);
        if (transit.target->forcesGravity() && transit.target->getDirection().has_value()) {
            switch (*transit.target->getDirection()) {
                case PortalDirection::UP:
                    rigidBody->setGravityDirection(GravityDirection::UP);
                    break;
                case PortalDirection::DOWN:
                    rigidBody->setGravityDirection(GravityDirection::DOWN);
                    break;
                case PortalDirection::LEFT:
                    rigidBody->setGravityDirection(GravityDirection::LEFT);
                    break;
                case PortalDirection::RIGHT:
                    rigidBody->setGravityDirection(GravityDirection::RIGHT);
                    break;
            }
        }
        rigidBody->setSpeed(speed);
    }
}

void applyCarriedExitState(PortalTransit& transit, CollisionRect* carriedBody) {
    if (!carriedBody || carriedBody->isRenderProxy()) return;

    GObject* carried = carriedBody->getFather();
    if (!carried || carried == transit.source || carried == transit.target) return;
    if (Object_m::level_ents_.find(carried->id_) == Object_m::level_ents_.end()) return;

    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) return;

    auto carriedTransit = activeTransits.find(carried->id_);
    if (carriedTransit != activeTransits.end()) {
        activeTransits.erase(carriedTransit);
    }

    carriedBody->setSurface(transformRectBetweenFrames(carriedBody->getSurface(), *sourceFrame, *targetFrame));

    RigidBody* rigidBody = dynamic_cast<RigidBody*>(carriedBody);
    if (!rigidBody) return;

    Vector2 speed = rotateVectorBetweenFrames(rigidBody->getSpeed(), *sourceFrame, *targetFrame);
    if (transit.target->forcesGravity() && transit.target->getDirection().has_value()) {
        switch (*transit.target->getDirection()) {
            case PortalDirection::UP:
                rigidBody->setGravityDirection(GravityDirection::UP);
                break;
            case PortalDirection::DOWN:
                rigidBody->setGravityDirection(GravityDirection::DOWN);
                break;
            case PortalDirection::LEFT:
                rigidBody->setGravityDirection(GravityDirection::LEFT);
                break;
            case PortalDirection::RIGHT:
                rigidBody->setGravityDirection(GravityDirection::RIGHT);
                break;
        }
    }
    rigidBody->setSpeed(speed);
}

} // namespace

Portal::Portal(const SpawnData& data)
    : BasicEnt(data), isPlayerSpawned_(false) {

    spriteRect_ = body_->getSurface();

    if (data.interaction.direction.has_value()) {
        direction_ = data.interaction.direction.value();

        Rectangle collisionRect = body_->getSurface();
        switch (direction_.value()) {
            case PortalDirection::UP:
                collisionRect.y -= 1;
                collisionRect.height += 1;
                break;
            case PortalDirection::DOWN:
                collisionRect.height += 1;
                break;
            case PortalDirection::LEFT:
                collisionRect.x -= 1;
                collisionRect.width += 1;
                break;
            case PortalDirection::RIGHT:
                collisionRect.width += 1;
                break;
        }
        body_->setSurface(collisionRect);
    }
    body_->setSolid(false);
    disableTilesUnderPortal(this, disabledTileIds_);

    if (data.interaction.forceGravity.has_value()) {
        forceGravity_ = data.interaction.forceGravity.value();
    }

    std::string ownId = std::to_string(id_);
    linkable_ = new LinkableComponent(this, ownId);
    linkable_->onTriggerReceived = [this](const std::string& sourceId, const std::string& message, void* data) {
        this->onPortalLink(sourceId, message, data);
    };

    if (data.ldtk.linkId.has_value()) {
        targetLdtkId_ = data.ldtk.linkId.value();
    }

    if (data.ldtk.targetIds.has_value()) {
        for (const std::string& targetId : data.ldtk.targetIds.value()) {
            linkable_->addTargetId(targetId);
        }
    }
}

Portal::~Portal() {
    releaseDisabledTiles(disabledTileIds_);

    for (auto it = activeTransits.begin(); it != activeTransits.end(); ) {
        if (it->second.source == this || it->second.target == this) {
            it = activeTransits.erase(it);
        } else {
            ++it;
        }
    }

    delete linkable_;

    if (playerSpawnedPortal_ == this) {
        playerSpawnedPortal_ = nullptr;
    }
}

void Portal::routine() {
    BasicEnt::routine();

    if (!overlappingEntities_.empty()) {
        Rectangle myRect = body_->getSurface();
        std::vector<int> toRemove;
        for (int entId : overlappingEntities_) {
            auto it = Object_m::level_ents_.find(entId);
            if (it == Object_m::level_ents_.end()) {
                toRemove.push_back(entId);
                continue;
            }
            GObject* obj = it->second.get();
            CollisionRect* body = obj->getCollisionBody();
            if (!body || !CheckCollisionRecs(myRect, body->getSurface())) {
                toRemove.push_back(entId);
            }
        }
        for (int rem : toRemove) overlappingEntities_.erase(rem);
    }
}

void Portal::draw() {
    Color portalColor = WHITE;

    Portal* linkedPortal = getLinkedPortal();
    if (linkedPortal) {
        int colorSeed = std::min(id_, linkedPortal->id_);
        float hue = (colorSeed * 137.5f) / 360.0f;
        hue = hue - floor(hue);
        float saturation = 0.8f;
        float brightness = 0.9f;
        float c = brightness * saturation;
        float x = c * (1 - abs(fmod(hue * 6, 2) - 1));
        float m = brightness - c;

        float r, g, b;
        if (hue < 1.0f / 6.0f) {
            r = c; g = x; b = 0;
        } else if (hue < 2.0f / 6.0f) {
            r = x; g = c; b = 0;
        } else if (hue < 3.0f / 6.0f) {
            r = 0; g = c; b = x;
        } else if (hue < 4.0f / 6.0f) {
            r = 0; g = x; b = c;
        } else if (hue < 5.0f / 6.0f) {
            r = x; g = 0; b = c;
        } else {
            r = c; g = 0; b = x;
        }

        portalColor = Color{
            (unsigned char)((r + m) * 255),
            (unsigned char)((g + m) * 255),
            (unsigned char)((b + m) * 255),
            255
        };
    }

    sprite_->setTint(portalColor);
    sprite_->draw(spriteRect_);
    sprite_->setTint(WHITE);
}

std::optional<LinkableComponent*> Portal::getLinkableComponent() {
    return linkable_;
}

Portal* Portal::getLinkedPortal() const {
    std::vector<GObject*> linkedObjects = linkable_->getLinkedObjects();

    for (GObject* obj : linkedObjects) {
        Portal* portal = dynamic_cast<Portal*>(obj);
        if (portal) return portal;
    }

    return nullptr;
}

void Portal::onCollision(GObject* other) {
    if (!other || other == this) return;
    if (dynamic_cast<Portal*>(other)) return;
    if (Object_m::level_ents_.find(other->id_) == Object_m::level_ents_.end()) return;

    CollisionRect* entityBody = other->getCollisionBody();
    if (!entityBody) return;

    int oid = other->id_;
    if (overlappingEntities_.find(oid) != overlappingEntities_.end()) return;

    Portal* linkedPortal = getLinkedPortal();
    if (!linkedPortal) return;

    if (direction_.has_value() && linkedPortal->direction_.has_value()) {
        beginProgressiveTransit(other);
    } else {
        performTeleportation(other);
    }
}

Vector2 Portal::getCenter() const {
    Rectangle rect = body_->getSurface();
    return Vector2{rect.x + rect.width / 2.0f, rect.y + rect.height / 2.0f};
}

void Portal::spawnPortalAtPlayer() {
    Character* player = nullptr;
    for (auto& [id, obj] : Object_m::level_ents_) {
        Character* character = dynamic_cast<Character*>(obj.get());
        if (character) {
            player = character;
            break;
        }
    }

    if (!player) return;

    if (playerSpawnedPortal_) {
        playerSpawnedPortal_->to_delete_ = true;
        playerSpawnedPortal_ = nullptr;
    }

    SpawnData portalData;
    portalData.id = Object_m::genID();
    portalData.entityType = EntityType::Portal;
    portalData.layer = 1;
    portalData.ldtk.linkId = "player_portal";

    Rectangle playerRect = player->getRect();
    Vector2 playerCenter = {
        playerRect.x + playerRect.width / 2.0f,
        playerRect.y + playerRect.height / 2.0f
    };

    SpriteDesc sprite;
    sprite.filename = "gateway.png";
    portalData.sprite = sprite;

    CollisionDesc collision;
    collision.rect = Rectangle{playerCenter.x - 4, playerCenter.y - 4, 8, 8};
    collision.solid = false;
    portalData.physics.collision = collision;

    GObject* newPortalObj = Object_m::createFromSpawn(portalData);
    Portal* newPortal = dynamic_cast<Portal*>(newPortalObj);
    if (newPortal) {
        newPortal->setIsPlayerSpawned(true);
        playerSpawnedPortal_ = newPortal;
    }
}

void Portal::teleportPlayerToSpawnedPortal() {
    if (!playerSpawnedPortal_) return;

    Character* player = nullptr;
    for (auto& [id, obj] : Object_m::level_ents_) {
        Character* character = dynamic_cast<Character*>(obj.get());
        if (character) {
            player = character;
            break;
        }
    }
    if (!player) return;

    Vector2 portalCenter = playerSpawnedPortal_->getCenter();
    Rectangle playerRect = player->getRect();
    float newX = portalCenter.x - playerRect.width / 2.0f;
    float newY = portalCenter.y - playerRect.height / 2.0f;
    if (player->body_) {
        Rectangle newRect = player->body_->getSurface();
        newRect.x = newX;
        newRect.y = newY;
        player->body_->setSurface(newRect);
    }
    playerSpawnedPortal_->overlappingEntities_.insert(player->id_);
}

bool Portal::findSafePosition(Rectangle& entityRect, CollisionRect* entityBody, const Rectangle& targetPortalRect, PortalDirection direction) {
    Portal* linkedPortal = getLinkedPortal();

    std::vector<CollisionRect*> collisions = CollisionRect::query(entityRect, true);
    auto it = std::remove_if(collisions.begin(), collisions.end(),
        [this, linkedPortal, entityBody](CollisionRect* col) {
            return col == entityBody ||
                   col->getFather() == this ||
                   (linkedPortal && col->getFather() == linkedPortal);
        });
    collisions.erase(it, collisions.end());

    if (collisions.empty()) return true;

    float pushDistance = 2.0f;
    int maxAttempts = 20;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        Rectangle testRect = entityRect;

        switch (direction) {
            case PortalDirection::UP:
                testRect.y -= pushDistance * attempt;
                break;
            case PortalDirection::DOWN:
                testRect.y += pushDistance * attempt;
                break;
            case PortalDirection::LEFT:
                testRect.x -= pushDistance * attempt;
                break;
            case PortalDirection::RIGHT:
                testRect.x += pushDistance * attempt;
                break;
        }

        std::vector<CollisionRect*> testCollisions = CollisionRect::query(testRect, true);
        auto testIt = std::remove_if(testCollisions.begin(), testCollisions.end(),
            [this, linkedPortal, entityBody](CollisionRect* col) {
                return col == entityBody ||
                       col->getFather() == this ||
                       (linkedPortal && col->getFather() == linkedPortal);
            });
        testCollisions.erase(testIt, testCollisions.end());

        if (testCollisions.empty()) {
            entityRect = testRect;
            return true;
        }
    }

    return false;
}

bool Portal::beginProgressiveTransit(GObject* entity) {
    if (!entity || !body_) return false;
    CollisionRect* entityBody = entity->getCollisionBody();
    if (!entityBody) return false;
    Rectangle rect = entityBody->getSurface();
    if (!canStartTransit(this, entity, rect, rect, false)) return false;
    return beginTransit(this, entity, rect);
}

void Portal::prepareMovement(GObject* entity, CollisionRect* entityBody, Rectangle fromRect, Rectangle toRect) {
    if (!entity || !entityBody) return;
    if (activeTransits.find(entity->id_) != activeTransits.end()) return;

    Portal* crossedPortal = findCrossedPortal(entity, fromRect, toRect);
    if (!crossedPortal) return;

    beginTransit(crossedPortal, entity, fromRect);
}

bool Portal::isMovementBlocked(GObject* entity, CollisionRect* entityBody, Rectangle proposedRect) {
    if (!entity || !entityBody) return false;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) {
        return standardBlocked(entity, entityBody, proposedRect);
    }

    PortalTransit& transit = it->second;
    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) {
        return standardBlocked(entity, entityBody, proposedRect);
    }

    Rectangle targetRect = transformRectBetweenFrames(proposedRect, *sourceFrame, *targetFrame);
    std::optional<Rectangle> sourcePiece = openSidePiece(proposedRect, *sourceFrame);
    std::optional<Rectangle> targetPiece = openSidePiece(targetRect, *targetFrame);

    if (sourcePiece.has_value() && !fitsApertureTangent(proposedRect, *sourceFrame)) return true;
    if (sourcePiece.has_value() && targetPiece.has_value() && !fitsApertureTangent(targetRect, *targetFrame)) return true;
    if (sourcePiece.has_value() && pieceBlocked(&transit, entity, entityBody, *sourcePiece)) return true;
    if (targetPiece.has_value() && pieceBlocked(&transit, entity, entityBody, *targetPiece)) return true;
    return false;
}

bool Portal::isTransitProbeBlocked(GObject* entity, CollisionRect* entityBody, Rectangle probeRect) {
    if (!entity || !entityBody) return false;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) {
        return standardBlocked(entity, entityBody, probeRect);
    }

    PortalTransit& transit = it->second;
    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) {
        return standardBlocked(entity, entityBody, probeRect);
    }

    Rectangle targetRect = transformRectBetweenFrames(probeRect, *sourceFrame, *targetFrame);
    std::optional<Rectangle> sourcePiece = openSidePiece(probeRect, *sourceFrame);
    std::optional<Rectangle> targetPiece = openSidePiece(targetRect, *targetFrame);

    if (sourcePiece.has_value() && pieceBlocked(&transit, entity, entityBody, *sourcePiece)) return true;
    if (targetPiece.has_value() && pieceBlocked(&transit, entity, entityBody, *targetPiece)) return true;
    return false;
}

bool Portal::separateTransitCollisions(GObject* entity, CollisionRect* entityBody) {
    if (!entity || !entityBody) return false;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return false;

    PortalTransit& transit = it->second;
    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) return false;

    bool separated = false;
    const int maxAttempts = 20;

    for (int attempt = 0; attempt < maxAttempts; ++attempt) {
        refreshTransitPieces(transit, entity, entityBody->getSurface());

        struct Piece {
            std::optional<Rectangle> rect;
            bool targetSide = false;
        };
        Piece pieces[] = {
            {transit.sourcePiece, false},
            {transit.targetPiece, true}
        };

        bool moved = false;
        for (const Piece& piece : pieces) {
            if (!piece.rect.has_value()) continue;

            Rectangle pieceRect = *piece.rect;
            for (CollisionRect* other : CollisionRect::query(pieceRect, true)) {
                if (!other || other == entityBody) continue;
                if (other == transit.sourcePieceBody.get() || other == transit.targetPieceBody.get()) continue;
                if (isActiveMainBody(other)) continue;

                GObject* owner = other->getFather();
                if (!owner || owner == entity) continue;
                if (dynamic_cast<Portal*>(owner)) continue;
                if (shouldIgnoreApertureCollision(pieceRect, other)) continue;

                Rectangle otherRect = other->getSurface();
                if (!CheckCollisionRecs(pieceRect, otherRect)) continue;

                float overlapLeft = (pieceRect.x + pieceRect.width) - otherRect.x;
                float overlapRight = (otherRect.x + otherRect.width) - pieceRect.x;
                float overlapTop = (pieceRect.y + pieceRect.height) - otherRect.y;
                float overlapBottom = (otherRect.y + otherRect.height) - pieceRect.y;
                float overlapX = std::min(overlapLeft, overlapRight);
                float overlapY = std::min(overlapTop, overlapBottom);

                Vector2 pieceCenter = rectCenter(pieceRect);
                Vector2 otherCenter = rectCenter(otherRect);
                Vector2 separation{0.0f, 0.0f};
                if (overlapX < overlapY) {
                    separation.x = pieceCenter.x < otherCenter.x ? -overlapX - 0.01f : overlapX + 0.01f;
                } else {
                    separation.y = pieceCenter.y < otherCenter.y ? -overlapY - 0.01f : overlapY + 0.01f;
                }

                if (piece.targetSide) {
                    Rectangle targetRect = transit.targetFullRect;
                    targetRect.x += separation.x;
                    targetRect.y += separation.y;
                    entityBody->setSurface(transformRectBetweenFrames(targetRect, *targetFrame, *sourceFrame));
                } else {
                    Rectangle sourceRect = entityBody->getSurface();
                    sourceRect.x += separation.x;
                    sourceRect.y += separation.y;
                    entityBody->setSurface(sourceRect);
                }

                moved = true;
                separated = true;
                break;
            }

            if (moved) break;
        }

        if (!moved) break;
    }

    refreshTransitPieces(transit, entity, entityBody->getSurface());
    return separated;
}

bool Portal::syncTransit(GObject* entity, const std::vector<CollisionRect*>& carriedBodies) {
    if (!entity) return false;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return false;

    PortalTransit& transit = it->second;
    CollisionRect* entityBody = entity->getCollisionBody();
    if (!entityBody || !transit.source || !transit.target) {
        activeTransits.erase(it);
        return false;
    }

    Rectangle mainRect = entityBody->getSurface();
    refreshTransitPieces(transit, entity, mainRect);

    if (!transit.sourcePiece.has_value()) {
        for (CollisionRect* carriedBody : carriedBodies) {
            applyCarriedExitState(transit, carriedBody);
        }
        applyExitState(transit, entity, entityBody);
        activeTransits.erase(it);
        return true;
    }

    auto sourceFrame = makeFrame(transit.source);
    if (sourceFrame.has_value() &&
        !transit.targetPiece.has_value() &&
        !overlapsAperture(mainRect, *sourceFrame)) {
        activeTransits.erase(it);
        return false;
    }

    transit.previousFullRect = entityBody->getSurface();
    return false;
}

std::optional<Vector2> Portal::getTransitGravityStep(
    GObject* entity,
    GravityDirection currentDirection,
    double gravityAcceleration,
    float delta
) {
    if (!entity || delta <= 0.0f) return std::nullopt;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return std::nullopt;

    PortalTransit& transit = it->second;
    CollisionRect* entityBody = entity->getCollisionBody();
    if (!entityBody || !transit.source || !transit.target) return std::nullopt;

    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) return std::nullopt;

    Vector2 logicalCenter = rectCenter(entityBody->getSurface());
    if (signedDistanceToOpenSide(logicalCenter, *sourceFrame) >= 0.0f) {
        Vector2 sourceGravity = gravityVector(currentDirection, gravityAcceleration);
        return Vector2{sourceGravity.x * delta, sourceGravity.y * delta};
    }

    GravityDirection targetDirection = currentDirection;
    if (transit.target->forcesGravity() && transit.target->getDirection().has_value()) {
        targetDirection = gravityDirectionFromPortalDirection(*transit.target->getDirection());
    }

    Vector2 targetGravity = gravityVector(targetDirection, gravityAcceleration);
    Vector2 targetGravityInSourceFrame = rotateVectorBetweenFrames(targetGravity, *targetFrame, *sourceFrame);
    return Vector2{targetGravityInSourceFrame.x * delta, targetGravityInSourceFrame.y * delta};
}

void Portal::updateTransits() {
    std::vector<int> toRemove;

    for (auto& [entityId, transit] : activeTransits) {
        auto it = Object_m::level_ents_.find(entityId);
        if (it == Object_m::level_ents_.end()) {
            toRemove.push_back(entityId);
            continue;
        }

        GObject* entity = it->second.get();
        CollisionRect* entityBody = entity ? entity->getCollisionBody() : nullptr;
        if (!entity || !entityBody || !transit.source || !transit.target) {
            toRemove.push_back(entityId);
            continue;
        }

        Rectangle mainRect = entityBody->getSurface();
        refreshTransitPieces(transit, entity, mainRect);

        if (!transit.sourcePiece.has_value()) {
            applyExitState(transit, entity, entityBody);
            toRemove.push_back(entityId);
            continue;
        }

        auto sourceFrame = makeFrame(transit.source);
        if (sourceFrame.has_value() &&
            !transit.targetPiece.has_value() &&
            !overlapsAperture(mainRect, *sourceFrame)) {
            toRemove.push_back(entityId);
            continue;
        }

        transit.previousFullRect = mainRect;
    }

    for (int entityId : toRemove) {
        activeTransits.erase(entityId);
    }
}

void Portal::clearTransits() {
    activeTransits.clear();
}

void Portal::cancelTransit(GObject* entity) {
    if (!entity) return;
    activeTransits.erase(entity->id_);
}

std::optional<Rectangle> Portal::getTransitSourceSurface(GObject* entity) {
    if (!entity) return std::nullopt;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return std::nullopt;

    return it->second.sourceFullRect;
}

std::optional<Rectangle> Portal::getTransitTargetSurface(GObject* entity) {
    if (!entity) return std::nullopt;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return std::nullopt;

    return it->second.targetFullRect;
}

std::optional<Vector2> Portal::getTransitVisibleCenter(GObject* entity) {
    if (!entity) return std::nullopt;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return std::nullopt;

    const PortalTransit& transit = it->second;
    std::optional<Rectangle> visibleRect;
    float visibleArea = 0.0f;

    auto pickVisibleRect = [&](std::optional<Rectangle> rect) {
        if (!rect.has_value()) return;
        float area = rect->width * rect->height;
        if (area <= 0.0f || area < visibleArea) return;
        visibleRect = rect;
        visibleArea = area;
    };

    pickVisibleRect(transit.sourcePiece);
    pickVisibleRect(transit.targetPiece);
    if (!visibleRect.has_value()) return std::nullopt;

    return Vector2{
        visibleRect->x + visibleRect->width / 2.0f,
        visibleRect->y + visibleRect->height / 2.0f
    };
}

bool Portal::isTransitSourceVisible(GObject* entity, Rectangle rect) {
    if (!entity) return false;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return true;

    PortalTransit& transit = it->second;
    return transit.sourcePiece.has_value() && CheckCollisionRecs(rect, *transit.sourcePiece);
}

std::optional<Rectangle> Portal::transformTransitRect(GObject* entity, Rectangle rect) {
    if (!entity) return std::nullopt;

    auto it = activeTransits.find(entity->id_);
    if (it == activeTransits.end()) return std::nullopt;

    PortalTransit& transit = it->second;
    auto sourceFrame = makeFrame(transit.source);
    auto targetFrame = makeFrame(transit.target);
    if (!sourceFrame.has_value() || !targetFrame.has_value()) return std::nullopt;

    return transformRectBetweenFrames(rect, *sourceFrame, *targetFrame);
}

void Portal::releaseAllDisabledTiles() {
    for (auto& [tileId, disabledTile] : disabledPortalTiles) {
        auto tileIt = Object_m::level_tiles_.find(tileId);
        if (tileIt == Object_m::level_tiles_.end() || !tileIt->second) continue;

        CollisionRect* body = tileIt->second->getCollisionBody();
        if (body) body->setSolid(disabledTile.originalSolid);
    }
    disabledPortalTiles.clear();

    for (auto& [_, obj] : Object_m::level_ents_) {
        Portal* portal = dynamic_cast<Portal*>(obj.get());
        if (portal) portal->disabledTileIds_.clear();
    }
}

bool Portal::isEntityInTransit(GObject* entity) {
    return entity && activeTransits.find(entity->id_) != activeTransits.end();
}

void Portal::drawTransitsForLayer(int layer) {
    for (auto& [entityId, transit] : activeTransits) {
        auto it = Object_m::level_ents_.find(entityId);
        if (it == Object_m::level_ents_.end()) continue;
        GObject* entity = it->second.get();
        if (!entity || entity->layer_ != layer) continue;

        if (transit.sourcePiece.has_value()) {
            drawClipped(entity, transit.sourceFullRect, *transit.sourcePiece);
        }
        if (transit.targetPiece.has_value()) {
            drawClipped(entity, transit.targetFullRect, *transit.targetPiece);
        }
    }
}

bool Portal::shouldIgnoreTransitCollision(GObject* moving, CollisionRect* obstacle) {
    if (!moving || !obstacle) return false;

    GObject* obstacleOwner = obstacle->getFather();
    if (dynamic_cast<Portal*>(obstacleOwner)) return true;
    if (isActiveMainBody(obstacle)) return true;

    auto it = activeTransits.find(moving->id_);
    if (it != activeTransits.end()) {
        PortalTransit& transit = it->second;
        if (obstacle == transit.sourcePieceBody.get() || obstacle == transit.targetPieceBody.get()) return true;
        if (!obstacleOwner || obstacleOwner == moving) return true;

        Rectangle obstacleRect = obstacle->getSurface();
        bool touchesVisiblePiece = false;
        if (transit.sourcePiece.has_value() && CheckCollisionRecs(*transit.sourcePiece, obstacleRect)) {
            if (shouldIgnoreApertureCollision(*transit.sourcePiece, obstacle)) return true;
            touchesVisiblePiece = true;
        }
        if (transit.targetPiece.has_value() && CheckCollisionRecs(*transit.targetPiece, obstacleRect)) {
            if (shouldIgnoreApertureCollision(*transit.targetPiece, obstacle)) return true;
            touchesVisiblePiece = true;
        }
        return !touchesVisiblePiece;
    }

    CollisionRect* body = moving->getCollisionBody();
    if (!body) return false;
    return shouldIgnoreApertureCollision(body->getSurface(), obstacle);
}

bool Portal::performTeleportation(GObject* entity) {
    Portal* linkedPortal = getLinkedPortal();
    if (!linkedPortal || !linkedPortal->body_) return false;

    CollisionRect* entityBody = entity ? entity->getCollisionBody() : nullptr;
    if (!entityBody) return false;

    Rectangle entityRect = entityBody->getSurface();
    Rectangle sourcePortalRect = body_->getSurface();
    Rectangle targetPortalRect = linkedPortal->body_->getSurface();

    float relativeX = (entityRect.x + entityRect.width / 2.0f - sourcePortalRect.x) / sourcePortalRect.width;
    float relativeY = (entityRect.y + entityRect.height / 2.0f - sourcePortalRect.y) / sourcePortalRect.height;
    relativeX = std::max(0.0f, std::min(1.0f, relativeX));
    relativeY = std::max(0.0f, std::min(1.0f, relativeY));

    RigidBody* rigidBody = dynamic_cast<RigidBody*>(entityBody);
    Vector2 spd = rigidBody ? rigidBody->getSpeed() : Vector2{0.0f, 0.0f};
    float newX = targetPortalRect.x + targetPortalRect.width / 2.0f - entityRect.width / 2.0f;
    float newY = targetPortalRect.y + targetPortalRect.height / 2.0f - entityRect.height / 2.0f;

    if (linkedPortal->direction_.has_value()) {
        switch (linkedPortal->direction_.value()) {
            case PortalDirection::UP:
                newX = targetPortalRect.x + relativeX * targetPortalRect.width - entityRect.width / 2.0f;
                newY = targetPortalRect.y - entityRect.height;
                spd.y = -std::abs(spd.y);
                break;
            case PortalDirection::DOWN:
                newX = targetPortalRect.x + relativeX * targetPortalRect.width - entityRect.width / 2.0f;
                newY = targetPortalRect.y + targetPortalRect.height;
                spd.y = std::abs(spd.y);
                break;
            case PortalDirection::LEFT:
                newX = targetPortalRect.x - entityRect.width;
                newY = targetPortalRect.y + relativeY * targetPortalRect.height - entityRect.height / 2.0f;
                spd.x = -std::abs(spd.x);
                break;
            case PortalDirection::RIGHT:
                newX = targetPortalRect.x + targetPortalRect.width;
                newY = targetPortalRect.y + relativeY * targetPortalRect.height - entityRect.height / 2.0f;
                spd.x = std::abs(spd.x);
                break;
        }
    } else {
        spd.y = -spd.y;
    }

    Rectangle newRect = entityRect;
    newRect.x = newX;
    newRect.y = newY;

    PortalDirection safetyDirection = linkedPortal->direction_.value_or(PortalDirection::UP);
    if (!findSafePosition(newRect, entityBody, targetPortalRect, safetyDirection)) {
        newRect.x = targetPortalRect.x + targetPortalRect.width / 2.0f - entityRect.width / 2.0f;
        newRect.y = targetPortalRect.y + targetPortalRect.height / 2.0f - entityRect.height / 2.0f;
        if (!findSafePosition(newRect, entityBody, targetPortalRect, safetyDirection)) return false;
    }

    entityBody->setSurface(newRect);

    if (rigidBody && linkedPortal->forceGravity_ && linkedPortal->direction_.has_value()) {
        switch (linkedPortal->direction_.value()) {
            case PortalDirection::UP:
                rigidBody->setGravityDirection(GravityDirection::UP);
                break;
            case PortalDirection::DOWN:
                rigidBody->setGravityDirection(GravityDirection::DOWN);
                break;
            case PortalDirection::LEFT:
                rigidBody->setGravityDirection(GravityDirection::LEFT);
                break;
            case PortalDirection::RIGHT:
                rigidBody->setGravityDirection(GravityDirection::RIGHT);
                break;
        }
    }
    if (rigidBody) {
        rigidBody->setSpeed(spd);
    }

    overlappingEntities_.insert(entity->id_);
    linkedPortal->overlappingEntities_.insert(entity->id_);
    return true;
}

void Portal::onPortalLink(const std::string& sourceId, const std::string& message, void* data) {
}

void Portal::setupPortalLinks() {
    for (auto& [id, obj] : Object_m::level_ents_) {
        Portal* portal = dynamic_cast<Portal*>(obj.get());
        if (!portal) continue;

        disableTilesUnderPortal(portal, portal->disabledTileIds_);

        if (!portal->targetLdtkId_.empty()) {
            int targetEngineId = Ldtk_m::getEngineIdFromLdtkId(portal->targetLdtkId_);
            if (targetEngineId != -1) {
                std::string targetIdStr = std::to_string(targetEngineId);
                portal->linkable_->addTargetId(targetIdStr);
            }
        }
    }
}
