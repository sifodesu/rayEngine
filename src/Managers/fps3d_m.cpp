#include "fps3d_m.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "character.h"
#include "basicEnt.h"
#include "collisionRect.h"
#include "definitions.h"
#include "door.h"
#include "input.h"
#include "ldtk_m.h"
#include "lightComponent.h"
#include "object_m.h"
#include "portal.h"
#include "raycam_m.h"
#include "raylib.h"
#include "rigidBody.h"
#include "rlgl.h"
#include "shader_m.h"
#include "texture_m.h"
#include "triggerButton.h"
#include "water.h"

namespace {

enum class WallSide {
    North,
    South,
    West,
    East
};

struct WallFace {
    bool horizontal{true}; // horizontal in map-space: X varies, Z fixed
    float fixed{0.0f};
    float start{0.0f};
    float end{0.0f};
    WallSide side{WallSide::North};
};

struct Interval {
    float start{0.0f};
    float end{0.0f};
};

struct TextureRegion {
    Texture2D texture{};
    Rectangle source{0.0f, 0.0f, 0.0f, 0.0f};
    bool flipX{false};
    bool flipY{false};
};

struct QuadUvs {
    Vector2 topLeft{0.0f, 0.0f};
    Vector2 topRight{1.0f, 0.0f};
    Vector2 bottomRight{1.0f, 1.0f};
    Vector2 bottomLeft{0.0f, 1.0f};
};

struct TexturedRect {
    Rectangle rect{0.0f, 0.0f, 0.0f, 0.0f};
    TextureRegion region{};
    Color tint{WHITE};
};

std::vector<WallFace> wallFaces;
std::vector<Rectangle> solidCaps;
std::vector<TexturedRect> solidCapTiles;
Rectangle worldBounds{0.0f, 0.0f, 0.0f, 0.0f};
Camera3D camera{};
Vector3 position{0.0f, 24.0f, 0.0f};
float yaw = 0.0f;
float pitch = 0.0f;
bool enabled = false;
bool initialized = false;
float transitionProgress = 0.0f; // 0 = flat 2D view, 1 = full FPS view

constexpr float kEyeHeight = 26.0f;
constexpr float kWallHeight = 56.0f;
constexpr float kCeilingHeight = 76.0f;
constexpr float kCollisionRadius = 5.5f;
constexpr float kWalkSpeed = 96.0f;
constexpr float kSprintMultiplier = 1.8f;
constexpr float kMouseSensitivity = 0.0028f;
constexpr float kMaxPitch = 1.35f;
constexpr float kDrawDistance = 900.0f;
constexpr float kBoundsPadding = 96.0f;
constexpr float kMergeEpsilon = 0.01f;
constexpr float kProbeDepth = 0.35f;
constexpr float kProbeInset = 0.02f;
constexpr float kMinFaceLength = 0.5f;
constexpr float kObjectBillboardMaxSize = 34.0f;
constexpr float kObjectBillboardMinSize = 10.0f;
constexpr float kTransitionUpDuration = 5.95f;
constexpr float kTransitionDownDuration = 5.85f;
constexpr float kTopDownCameraHeight = 520.0f;
constexpr float kFlatMapLift = 0.10f;

bool nearlyEqual(float a, float b)
{
    return std::fabs(a - b) <= kMergeEpsilon;
}

float rectRight(Rectangle rect)
{
    return rect.x + rect.width;
}

float rectBottom(Rectangle rect)
{
    return rect.y + rect.height;
}

bool validRect(Rectangle rect)
{
    return rect.width > 0.0f && rect.height > 0.0f;
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float smooth01(float value)
{
    value = clamp01(value);
    return value * value * (3.0f - 2.0f * value);
}

float visualProgress()
{
    return smooth01(transitionProgress);
}

bool fully3D()
{
    return enabled && transitionProgress >= 0.999f;
}

bool transitionVisible()
{
    return transitionProgress > 0.001f;
}

Color alphaScaled(Color color, float scale)
{
    color.a = static_cast<unsigned char>(std::clamp(static_cast<float>(color.a) * clamp01(scale), 0.0f, 255.0f));
    return color;
}

float overlayAlpha()
{
    return transitionVisible() ? 1.0f : 0.0f;
}

void beginLocalAlphaBlend()
{
    BeginBlendMode(BLEND_ALPHA);
}

void endLocalAlphaBlend()
{
    EndBlendMode();
}

float morphedWallHeight()
{
    return std::max(1.0f, kWallHeight * visualProgress());
}

Vector3 normalizeVec(Vector3 v);
TextureRegion textureRegionForObject(GObject* object);
Color tintForObject(GObject* object);
Color textureTintForObject(GObject* object);
void beginLocalAlphaBlend();
void endLocalAlphaBlend();

void expandBounds(Rectangle rect)
{
    if (!validRect(rect)) return;

    if (!validRect(worldBounds)) {
        worldBounds = rect;
        return;
    }

    const float minX = std::min(worldBounds.x, rect.x);
    const float minY = std::min(worldBounds.y, rect.y);
    const float maxX = std::max(rectRight(worldBounds), rectRight(rect));
    const float maxY = std::max(rectBottom(worldBounds), rectBottom(rect));
    worldBounds = Rectangle{minX, minY, maxX - minX, maxY - minY};
}

std::vector<Rectangle> mergeHorizontal(std::vector<Rectangle> rects)
{
    std::sort(rects.begin(), rects.end(), [](Rectangle a, Rectangle b) {
        if (!nearlyEqual(a.y, b.y)) return a.y < b.y;
        if (!nearlyEqual(a.height, b.height)) return a.height < b.height;
        return a.x < b.x;
    });

    std::vector<Rectangle> merged;
    merged.reserve(rects.size());

    for (Rectangle rect : rects) {
        if (merged.empty()) {
            merged.push_back(rect);
            continue;
        }

        Rectangle& last = merged.back();
        const bool sameBand = nearlyEqual(last.y, rect.y) && nearlyEqual(last.height, rect.height);
        const bool touches = rect.x <= rectRight(last) + kMergeEpsilon;
        if (sameBand && touches) {
            const float right = std::max(rectRight(last), rectRight(rect));
            last.width = right - last.x;
        } else {
            merged.push_back(rect);
        }
    }

    return merged;
}

std::vector<Rectangle> mergeVertical(std::vector<Rectangle> rects)
{
    std::sort(rects.begin(), rects.end(), [](Rectangle a, Rectangle b) {
        if (!nearlyEqual(a.x, b.x)) return a.x < b.x;
        if (!nearlyEqual(a.width, b.width)) return a.width < b.width;
        return a.y < b.y;
    });

    std::vector<Rectangle> merged;
    merged.reserve(rects.size());

    for (Rectangle rect : rects) {
        if (merged.empty()) {
            merged.push_back(rect);
            continue;
        }

        Rectangle& last = merged.back();
        const bool sameColumn = nearlyEqual(last.x, rect.x) && nearlyEqual(last.width, rect.width);
        const bool touches = rect.y <= rectBottom(last) + kMergeEpsilon;
        if (sameColumn && touches) {
            const float bottom = std::max(rectBottom(last), rectBottom(rect));
            last.height = bottom - last.y;
        } else {
            merged.push_back(rect);
        }
    }

    return merged;
}

std::vector<Rectangle> mergeSolidRects(std::vector<Rectangle> rects)
{
    if (rects.empty()) return rects;
    return mergeVertical(mergeHorizontal(std::move(rects)));
}

RigidBody* currentTarget()
{
    return Raycam_m::getTarget();
}

bool shouldIgnoreBody(CollisionRect* body)
{
    if (!body) return true;
    if (body == currentTarget()) return true;
    if (body->isRenderProxy()) return true;
    return dynamic_cast<Character*>(body->getFather()) != nullptr;
}

bool collidesAt(float x, float z)
{
    Rectangle footprint{
        x - kCollisionRadius,
        z - kCollisionRadius,
        kCollisionRadius * 2.0f,
        kCollisionRadius * 2.0f
    };

    for (CollisionRect* body : CollisionRect::query(footprint, true)) {
        if (shouldIgnoreBody(body)) continue;
        if (CheckCollisionRecs(footprint, body->getSurface())) {
            return true;
        }
    }
    return false;
}

Vector2 initialMapPosition()
{
    if (RigidBody* target = currentTarget()) {
        return target->getCenterCoord();
    }
    if (validRect(worldBounds)) {
        return Vector2{worldBounds.x + worldBounds.width * 0.5f,
                       worldBounds.y + worldBounds.height * 0.5f};
    }
    return Vector2{NATIVE_RES_WIDTH * 0.5f, NATIVE_RES_HEIGHT * 0.5f};
}

Vector2 findOpenMapPosition(Vector2 preferred)
{
    if (!collidesAt(preferred.x, preferred.y)) return preferred;

    constexpr float step = 8.0f;
    constexpr int maxRing = 24;
    for (int ring = 1; ring <= maxRing; ++ring) {
        for (int yStep = -ring; yStep <= ring; ++yStep) {
            for (int xStep = -ring; xStep <= ring; ++xStep) {
                if (std::abs(xStep) != ring && std::abs(yStep) != ring) continue;
                Vector2 candidate{
                    preferred.x + static_cast<float>(xStep) * step,
                    preferred.y + static_cast<float>(yStep) * step
                };
                if (!collidesAt(candidate.x, candidate.y)) return candidate;
            }
        }
    }

    return preferred;
}

void resetCameraFromPlayer()
{
    Vector2 mapPos = findOpenMapPosition(initialMapPosition());
    position = Vector3{mapPos.x, kEyeHeight, mapPos.y};
    yaw = 0.0f;
    pitch = 0.0f;
}

void updateCameraTarget()
{
    const float cp = std::cos(pitch);
    Vector3 forward{
        std::cos(yaw) * cp,
        std::sin(pitch),
        std::sin(yaw) * cp
    };

    camera.position = position;
    camera.target = Vector3{
        position.x + forward.x,
        position.y + forward.y,
        position.z + forward.z
    };
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 70.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

Camera3D transitionCamera()
{
    if (fully3D()) return camera;

    Camera2D cam2d = Raycam_m::getCam();
    Vector3 topPosition{cam2d.target.x, kTopDownCameraHeight, cam2d.target.y};
    Vector3 topTarget{cam2d.target.x, 0.0f, cam2d.target.y};
    Vector3 topUp{0.0f, 0.0f, -1.0f};

    const float t = visualProgress();
    Camera3D out{};
    out.position = Vector3{
        topPosition.x + (camera.position.x - topPosition.x) * t,
        topPosition.y + (camera.position.y - topPosition.y) * t,
        topPosition.z + (camera.position.z - topPosition.z) * t
    };
    out.target = Vector3{
        topTarget.x + (camera.target.x - topTarget.x) * t,
        topTarget.y + (camera.target.y - topTarget.y) * t,
        topTarget.z + (camera.target.z - topTarget.z) * t
    };
    out.up = normalizeVec(Vector3{
        topUp.x + (camera.up.x - topUp.x) * t,
        topUp.y + (camera.up.y - topUp.y) * t,
        topUp.z + (camera.up.z - topUp.z) * t
    });
    out.fovy = 26.0f + (camera.fovy - 26.0f) * t;
    out.projection = CAMERA_PERSPECTIVE;
    return out;
}

void moveWithCollision(Vector2 delta)
{
    if (delta.x == 0.0f && delta.y == 0.0f) return;

    const float nextX = position.x + delta.x;
    if (!collidesAt(nextX, position.z)) {
        position.x = nextX;
    }

    const float nextZ = position.z + delta.y;
    if (!collidesAt(position.x, nextZ)) {
        position.z = nextZ;
    }
}

float distanceSqToRectXZ(Rectangle rect, Vector3 point)
{
    const float clampedX = std::clamp(point.x, rect.x, rectRight(rect));
    const float clampedZ = std::clamp(point.z, rect.y, rectBottom(rect));
    const float dx = point.x - clampedX;
    const float dz = point.z - clampedZ;
    return dx * dx + dz * dz;
}

float distanceSqToFaceXZ(const WallFace& face, Vector3 point)
{
    if (face.horizontal) {
        const float clampedX = std::clamp(point.x, face.start, face.end);
        const float dx = point.x - clampedX;
        const float dz = point.z - face.fixed;
        return dx * dx + dz * dz;
    }

    const float clampedZ = std::clamp(point.z, face.start, face.end);
    const float dx = point.x - face.fixed;
    const float dz = point.z - clampedZ;
    return dx * dx + dz * dz;
}

Color wallTintFor(WallSide side)
{
    switch (side) {
        case WallSide::North: return Color{75, 82, 93, 255};
        case WallSide::South: return Color{63, 70, 81, 255};
        case WallSide::West: return Color{58, 66, 78, 255};
        case WallSide::East: return Color{86, 91, 99, 255};
    }
    return Color{70, 76, 86, 255};
}

Color trimTintFor(WallSide side)
{
    Color base = wallTintFor(side);
    return Color{
        static_cast<unsigned char>(std::min(255, base.r + 42)),
        static_cast<unsigned char>(std::min(255, base.g + 38)),
        static_cast<unsigned char>(std::min(255, base.b + 32)),
        255
    };
}

Texture2D wallTexture()
{
    return Texture_m::getTexture("blob_tileset.png");
}

Texture2D objectTexture()
{
    return Texture_m::getTexture("inv.png");
}

Texture2D portalTexture()
{
    return Texture_m::getTexture("gateway.png");
}

Texture2D doorTexture()
{
    return Texture_m::getTexture("door.png");
}

Texture2D buttonTexture()
{
    return Texture_m::getTexture("choice.png");
}

bool validTexture(Texture2D texture)
{
    return texture.id > 0 && texture.width > 0 && texture.height > 0;
}

Rectangle fullTextureSource(Texture2D texture)
{
    return Rectangle{
        0.0f,
        0.0f,
        static_cast<float>(std::max(texture.width, 1)),
        static_cast<float>(std::max(texture.height, 1))
    };
}

TextureRegion fullTextureRegion(Texture2D texture)
{
    return TextureRegion{texture, fullTextureSource(texture), false, false};
}

TextureRegion atlasTextureRegion(Texture2D texture, Rectangle source)
{
    if (!validTexture(texture) || source.width == 0.0f || source.height == 0.0f) {
        return fullTextureRegion(texture);
    }
    return TextureRegion{texture, source, false, false};
}

TextureRegion spriteTextureRegion(const Sprite* sprite)
{
    if (!sprite) return fullTextureRegion(objectTexture());
    return TextureRegion{
        sprite->getTexture(),
        sprite->getCurrentSourceRect(),
        sprite->getFlipX(),
        sprite->getFlipY()
    };
}

TextureRegion wallTextureRegion()
{
    return atlasTextureRegion(wallTexture(), Rectangle{0.0f, 0.0f, 8.0f, 8.0f});
}

TextureRegion floorTextureRegion()
{
    return atlasTextureRegion(wallTexture(), Rectangle{0.0f, 0.0f, 8.0f, 8.0f});
}

TextureRegion capTextureRegion()
{
    return atlasTextureRegion(wallTexture(), Rectangle{0.0f, 0.0f, 8.0f, 8.0f});
}

QuadUvs uvsForRegion(const TextureRegion& region)
{
    if (!validTexture(region.texture)) return QuadUvs{};

    Rectangle src = region.source;
    if (src.width == 0.0f || src.height == 0.0f) {
        src = fullTextureSource(region.texture);
    }

    const float texW = std::max(static_cast<float>(region.texture.width), 1.0f);
    const float texH = std::max(static_cast<float>(region.texture.height), 1.0f);
    const float inset = 0.01f;
    const float sourceWidth = std::max(std::fabs(src.width), 1.0f);
    const float sourceHeight = std::max(std::fabs(src.height), 1.0f);

    float left = src.x + (sourceWidth > inset * 2.0f ? inset : 0.0f);
    float right = src.x + sourceWidth - (sourceWidth > inset * 2.0f ? inset : 0.0f);
    float top = src.y + (sourceHeight > inset * 2.0f ? inset : 0.0f);
    float bottom = src.y + sourceHeight - (sourceHeight > inset * 2.0f ? inset : 0.0f);

    if ((src.width < 0.0f) != region.flipX) std::swap(left, right);
    if ((src.height < 0.0f) != region.flipY) std::swap(top, bottom);

    left = std::clamp(left / texW, 0.0f, 1.0f);
    right = std::clamp(right / texW, 0.0f, 1.0f);
    top = std::clamp(top / texH, 0.0f, 1.0f);
    bottom = std::clamp(bottom / texH, 0.0f, 1.0f);

    return QuadUvs{
        Vector2{left, top},
        Vector2{right, top},
        Vector2{right, bottom},
        Vector2{left, bottom}
    };
}

bool beginSceneShader(float ambient, bool forceOpaque = false)
{
    const char* shaderName = Shader_m::has("fps_world") ? "fps_world" : "model_lit";
    if (!Shader_m::has(shaderName)) return false;

    Shader shader = Shader_m::get(shaderName);
    Vector3 lightDir{-0.35f, -1.0f, -0.45f};
    Vector4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
    int textureSlot = 0;

    int loc = GetShaderLocation(shader, "lightDir");
    if (loc >= 0) SetShaderValue(shader, loc, &lightDir, SHADER_UNIFORM_VEC3);
    loc = GetShaderLocation(shader, "ambient");
    if (loc >= 0) SetShaderValue(shader, loc, &ambient, SHADER_UNIFORM_FLOAT);
    loc = GetShaderLocation(shader, "colDiffuse");
    if (loc >= 0) SetShaderValue(shader, loc, &diffuse, SHADER_UNIFORM_VEC4);
    loc = GetShaderLocation(shader, "texture0");
    if (loc >= 0) SetShaderValue(shader, loc, &textureSlot, SHADER_UNIFORM_INT);
    int opaqueValue = forceOpaque ? 1 : 0;
    loc = GetShaderLocation(shader, "forceOpaque");
    if (loc >= 0) SetShaderValue(shader, loc, &opaqueValue, SHADER_UNIFORM_INT);

    BeginShaderMode(shader);
    return true;
}

void endSceneShader(bool active)
{
    if (active) EndShaderMode();
}

void drawUntexturedQuad(Vector3 a, Vector3 b, Vector3 c, Vector3 d, Color color)
{
    DrawTriangle3D(a, b, c, color);
    DrawTriangle3D(a, c, d, color);
    DrawTriangle3D(c, b, a, color);
    DrawTriangle3D(d, c, a, color);
}

void submitTexturedVertex(Vector3 position, Vector3 normal, Vector2 uv, Color tint)
{
    rlColor4ub(tint.r, tint.g, tint.b, tint.a);
    rlNormal3f(normal.x, normal.y, normal.z);
    rlTexCoord2f(uv.x, uv.y);
    rlVertex3f(position.x, position.y, position.z);
}

void submitTexturedTriangle(Vector3 a,
                            Vector3 b,
                            Vector3 c,
                            Vector3 normal,
                            Vector2 uvA,
                            Vector2 uvB,
                            Vector2 uvC,
                            Color tint)
{
    submitTexturedVertex(a, normal, uvA, tint);
    submitTexturedVertex(b, normal, uvB, tint);
    submitTexturedVertex(c, normal, uvC, tint);
}

void submitTexturedQuadOneSided(Texture2D texture,
                                Vector3 a,
                                Vector3 b,
                                Vector3 c,
                                Vector3 d,
                                Vector3 normal,
                                Vector2 uvA,
                                Vector2 uvB,
                                Vector2 uvC,
                                Vector2 uvD,
                                Color tint)
{
    rlBegin(RL_TRIANGLES);
        rlSetTexture(texture.id);
        submitTexturedTriangle(a, b, c, normal, uvA, uvB, uvC, tint);
        submitTexturedTriangle(a, c, d, normal, uvA, uvC, uvD, tint);
    rlEnd();
    rlSetTexture(0);
}

void drawTexturedQuad(Texture2D texture,
                      Vector3 a,
                      Vector3 b,
                      Vector3 c,
                      Vector3 d,
                      Vector3 normal,
                      Vector2 uvA,
                      Vector2 uvB,
                      Vector2 uvC,
                      Vector2 uvD,
                      Color tint)
{
    if (texture.id <= 0) {
        drawUntexturedQuad(a, b, c, d, tint);
        return;
    }

    submitTexturedQuadOneSided(texture, a, b, c, d, normal, uvA, uvB, uvC, uvD, tint);
    submitTexturedQuadOneSided(texture, d, c, b, a, Vector3{-normal.x, -normal.y, -normal.z}, uvD, uvC, uvB, uvA, tint);
}

void drawTexturedQuadStretch(const TextureRegion& region,
                             Vector3 topLeft,
                             Vector3 topRight,
                             Vector3 bottomRight,
                             Vector3 bottomLeft,
                             Vector3 normal,
                             Color tint)
{
    const QuadUvs uvs = uvsForRegion(region);
    drawTexturedQuad(
        region.texture,
        topLeft,
        topRight,
        bottomRight,
        bottomLeft,
        normal,
        uvs.topLeft,
        uvs.topRight,
        uvs.bottomRight,
        uvs.bottomLeft,
        tint);
}

void drawTexturedVerticalQuadStretch(const TextureRegion& region,
                                     Vector3 bottomLeft,
                                     Vector3 bottomRight,
                                     Vector3 topRight,
                                     Vector3 topLeft,
                                     Vector3 normal,
                                     Color tint)
{
    const QuadUvs uvs = uvsForRegion(region);
    drawTexturedQuad(
        region.texture,
        bottomLeft,
        bottomRight,
        topRight,
        topLeft,
        normal,
        uvs.bottomLeft,
        uvs.bottomRight,
        uvs.topRight,
        uvs.topLeft,
        tint);
}

void addInterval(std::vector<Interval>& intervals, float start, float end, float minBound, float maxBound)
{
    start = std::clamp(start, minBound, maxBound);
    end = std::clamp(end, minBound, maxBound);
    if (end - start <= kMergeEpsilon) return;
    intervals.push_back(Interval{start, end});
}

std::vector<Interval> mergeIntervals(std::vector<Interval> intervals)
{
    if (intervals.empty()) return intervals;

    std::sort(intervals.begin(), intervals.end(), [](Interval a, Interval b) {
        if (!nearlyEqual(a.start, b.start)) return a.start < b.start;
        return a.end < b.end;
    });

    std::vector<Interval> merged;
    merged.reserve(intervals.size());
    for (Interval interval : intervals) {
        if (merged.empty() || interval.start > merged.back().end + kMergeEpsilon) {
            merged.push_back(interval);
        } else {
            merged.back().end = std::max(merged.back().end, interval.end);
        }
    }
    return merged;
}

void addExposedGaps(bool horizontal, float fixed, float start, float end, WallSide side, std::vector<Interval> covered)
{
    covered = mergeIntervals(std::move(covered));

    float cursor = start;
    for (Interval interval : covered) {
        if (interval.start - cursor > kMinFaceLength) {
            wallFaces.push_back(WallFace{horizontal, fixed, cursor, interval.start, side});
        }
        cursor = std::max(cursor, interval.end);
    }

    if (end - cursor > kMinFaceLength) {
        wallFaces.push_back(WallFace{horizontal, fixed, cursor, end, side});
    }
}

void collectHorizontalEdge(Rectangle rect, float z, float probeY, WallSide side)
{
    const float start = rect.x;
    const float end = rectRight(rect);
    Rectangle probe{
        rect.x + kProbeInset,
        probeY,
        std::max(0.0f, rect.width - kProbeInset * 2.0f),
        kProbeDepth
    };

    std::vector<Interval> covered;
    for (CollisionRect* body : CollisionRect::query(probe, true)) {
        if (shouldIgnoreBody(body)) continue;
        Rectangle other = body->getSurface();
        addInterval(covered, other.x, rectRight(other), start, end);
    }

    addExposedGaps(true, z, start, end, side, std::move(covered));
}

void collectVerticalEdge(Rectangle rect, float x, float probeX, WallSide side)
{
    const float start = rect.y;
    const float end = rectBottom(rect);
    Rectangle probe{
        probeX,
        rect.y + kProbeInset,
        kProbeDepth,
        std::max(0.0f, rect.height - kProbeInset * 2.0f)
    };

    std::vector<Interval> covered;
    for (CollisionRect* body : CollisionRect::query(probe, true)) {
        if (shouldIgnoreBody(body)) continue;
        Rectangle other = body->getSurface();
        addInterval(covered, other.y, rectBottom(other), start, end);
    }

    addExposedGaps(false, x, start, end, side, std::move(covered));
}

void buildWallFacesFromCaps()
{
    wallFaces.clear();

    for (Rectangle rect : solidCaps) {
        collectHorizontalEdge(rect, rect.y, rect.y - kProbeDepth, WallSide::North);
        collectHorizontalEdge(rect, rectBottom(rect), rectBottom(rect), WallSide::South);
        collectVerticalEdge(rect, rect.x, rect.x - kProbeDepth, WallSide::West);
        collectVerticalEdge(rect, rectRight(rect), rectRight(rect), WallSide::East);
    }

    std::sort(wallFaces.begin(), wallFaces.end(), [](WallFace a, WallFace b) {
        if (a.horizontal != b.horizontal) return a.horizontal > b.horizontal;
        if (a.side != b.side) return static_cast<int>(a.side) < static_cast<int>(b.side);
        if (!nearlyEqual(a.fixed, b.fixed)) return a.fixed < b.fixed;
        return a.start < b.start;
    });

    std::vector<WallFace> merged;
    merged.reserve(wallFaces.size());
    for (WallFace face : wallFaces) {
        if (merged.empty()) {
            merged.push_back(face);
            continue;
        }

        WallFace& last = merged.back();
        const bool sameLine = last.horizontal == face.horizontal &&
                              last.side == face.side &&
                              nearlyEqual(last.fixed, face.fixed);
        if (sameLine && face.start <= last.end + kMergeEpsilon) {
            last.end = std::max(last.end, face.end);
        } else {
            merged.push_back(face);
        }
    }
    wallFaces = std::move(merged);
}

Vector3 normalFor(WallSide side)
{
    switch (side) {
        case WallSide::North: return Vector3{0.0f, 0.0f, -1.0f};
        case WallSide::South: return Vector3{0.0f, 0.0f, 1.0f};
        case WallSide::West: return Vector3{-1.0f, 0.0f, 0.0f};
        case WallSide::East: return Vector3{1.0f, 0.0f, 0.0f};
    }
    return Vector3{0.0f, 0.0f, 1.0f};
}

void drawWallFace(const WallFace& face)
{
    const float y0 = 0.0f;
    const float y1 = morphedWallHeight();
    const Color wallTint = alphaScaled(wallTintFor(face.side), overlayAlpha());

    Vector3 a{};
    Vector3 b{};
    Vector3 c{};
    Vector3 d{};

    if (face.horizontal) {
        a = Vector3{face.start, y0, face.fixed};
        b = Vector3{face.end, y0, face.fixed};
        c = Vector3{face.end, y1, face.fixed};
        d = Vector3{face.start, y1, face.fixed};
    } else {
        a = Vector3{face.fixed, y0, face.start};
        b = Vector3{face.fixed, y0, face.end};
        c = Vector3{face.fixed, y1, face.end};
        d = Vector3{face.fixed, y1, face.start};
    }

    drawTexturedVerticalQuadStretch(
        wallTextureRegion(),
        a,
        b,
        c,
        d,
        normalFor(face.side),
        wallTint);
}

void drawWallFaceAccents(const WallFace& face)
{
    if (visualProgress() < 0.12f) return;

    const float y0 = 0.0f;
    const float y1 = morphedWallHeight();
    const Color trimTint = alphaScaled(trimTintFor(face.side), overlayAlpha());

    Vector3 a{};
    Vector3 b{};
    Vector3 c{};
    Vector3 d{};

    if (face.horizontal) {
        a = Vector3{face.start, y0, face.fixed};
        b = Vector3{face.end, y0, face.fixed};
        c = Vector3{face.end, y1, face.fixed};
        d = Vector3{face.start, y1, face.fixed};
    } else {
        a = Vector3{face.fixed, y0, face.start};
        b = Vector3{face.fixed, y0, face.end};
        c = Vector3{face.fixed, y1, face.end};
        d = Vector3{face.fixed, y1, face.start};
    }

    DrawLine3D(d, c, trimTint);
    DrawLine3D(a, b, alphaScaled(Color{32, 36, 43, 255}, overlayAlpha()));

    constexpr float seamStep = 64.0f;
    float seam = std::ceil(face.start / seamStep) * seamStep;
    while (seam < face.end - kMergeEpsilon) {
        if (face.horizontal) {
            DrawLine3D(Vector3{seam, y0 + 5.0f, face.fixed}, Vector3{seam, y1 - 4.0f, face.fixed}, alphaScaled(Color{48, 54, 64, 255}, overlayAlpha()));
        } else {
            DrawLine3D(Vector3{face.fixed, y0 + 5.0f, seam}, Vector3{face.fixed, y1 - 4.0f, seam}, alphaScaled(Color{48, 54, 64, 255}, overlayAlpha()));
        }
        seam += seamStep;
    }
}

void drawSolidCap(Rectangle rect)
{
    const float y = morphedWallHeight() + 0.03f;
    drawTexturedQuadStretch(
        capTextureRegion(),
        Vector3{rect.x, y, rect.y},
        Vector3{rectRight(rect), y, rect.y},
        Vector3{rectRight(rect), y, rectBottom(rect)},
        Vector3{rect.x, y, rectBottom(rect)},
        Vector3{0.0f, 1.0f, 0.0f},
        alphaScaled(Color{175, 178, 178, 255}, overlayAlpha()));
}

void drawSolidCapTile(const TexturedRect& tile)
{
    if (!validRect(tile.rect)) return;

    const float y = morphedWallHeight() + 0.04f;
    drawTexturedQuadStretch(
        validTexture(tile.region.texture) ? tile.region : capTextureRegion(),
        Vector3{tile.rect.x, y, tile.rect.y},
        Vector3{rectRight(tile.rect), y, tile.rect.y},
        Vector3{rectRight(tile.rect), y, rectBottom(tile.rect)},
        Vector3{tile.rect.x, y, rectBottom(tile.rect)},
        Vector3{0.0f, 1.0f, 0.0f},
        alphaScaled(tile.tint, overlayAlpha()));
}

void drawWorldFloor()
{
    drawTexturedQuadStretch(
        floorTextureRegion(),
        Vector3{worldBounds.x, 0.0f, worldBounds.y},
        Vector3{rectRight(worldBounds), 0.0f, worldBounds.y},
        Vector3{rectRight(worldBounds), 0.0f, rectBottom(worldBounds)},
        Vector3{worldBounds.x, 0.0f, rectBottom(worldBounds)},
        Vector3{0.0f, 1.0f, 0.0f},
        Color{135, 142, 146, 255});
}

void drawSpriteFlatQuad(const Sprite* sprite, Rectangle rect, float alpha)
{
    if (!sprite || !validRect(rect)) return;

    TextureRegion region = spriteTextureRegion(sprite);
    if (!validTexture(region.texture)) return;
    const Color tint = alphaScaled(sprite->getTint(), alpha);

    drawTexturedQuadStretch(
        region,
        Vector3{rect.x, kFlatMapLift, rect.y},
        Vector3{rectRight(rect), kFlatMapLift, rect.y},
        Vector3{rectRight(rect), kFlatMapLift, rectBottom(rect)},
        Vector3{rect.x, kFlatMapLift, rectBottom(rect)},
        Vector3{0.0f, 1.0f, 0.0f},
        tint);
}

void drawFlatObject(GObject* object, float alpha)
{
    if (!object || object->is3DRenderable()) return;
    if (dynamic_cast<Character*>(object) && visualProgress() > 0.2f) return;

    Rectangle rect = object->getRect();
    if (!validRect(rect)) return;
    if (distanceSqToRectXZ(rect, position) > kDrawDistance * kDrawDistance) return;

    if (const BasicEnt* basic = dynamic_cast<const BasicEnt*>(object)) {
        drawSpriteFlatQuad(basic->getSprite(), rect, alpha);
        return;
    }

    TextureRegion region = textureRegionForObject(object);
    const Color tint = alphaScaled(tintForObject(object), alpha);
    drawTexturedQuadStretch(
        region,
        Vector3{rect.x, kFlatMapLift, rect.y},
        Vector3{rectRight(rect), kFlatMapLift, rect.y},
        Vector3{rectRight(rect), kFlatMapLift, rectBottom(rect)},
        Vector3{rect.x, kFlatMapLift, rectBottom(rect)},
        Vector3{0.0f, 1.0f, 0.0f},
        tint);
}

void drawFlat2DMap()
{
    const float alpha = 1.0f - smooth01(std::clamp(transitionProgress * 1.35f, 0.0f, 1.0f));
    if (alpha <= 0.01f) return;

    beginLocalAlphaBlend();
    const bool shaderActive = beginSceneShader(0.92f);
    for (auto& [id, object] : Object_m::level_tiles_) {
        drawFlatObject(object.get(), alpha);
    }
    for (auto& [id, object] : Object_m::level_ents_) {
        drawFlatObject(object.get(), alpha);
    }
    endSceneShader(shaderActive);
    endLocalAlphaBlend();
}

void drawFloorGuides()
{
    constexpr float step = 64.0f;
    constexpr float range = 512.0f;
    const float minX = std::max(worldBounds.x, std::floor((position.x - range) / step) * step);
    const float maxX = std::min(rectRight(worldBounds), std::ceil((position.x + range) / step) * step);
    const float minZ = std::max(worldBounds.y, std::floor((position.z - range) / step) * step);
    const float maxZ = std::min(rectBottom(worldBounds), std::ceil((position.z + range) / step) * step);
    const Color gridColor{50, 55, 62, 255};

    for (float x = minX; x <= maxX; x += step) {
        DrawLine3D(Vector3{x, 0.04f, minZ}, Vector3{x, 0.04f, maxZ}, gridColor);
    }
    for (float z = minZ; z <= maxZ; z += step) {
        DrawLine3D(Vector3{minX, 0.04f, z}, Vector3{maxX, 0.04f, z}, gridColor);
    }
}

void drawCeilingHints()
{
    constexpr float step = 320.0f;
    const float minX = std::floor(worldBounds.x / step) * step;
    const float maxX = std::ceil(rectRight(worldBounds) / step) * step;
    const float minZ = std::floor(worldBounds.y / step) * step;
    const float maxZ = std::ceil(rectBottom(worldBounds) / step) * step;
    const Color lineColor{43, 47, 55, 255};

    for (float x = minX; x <= maxX; x += step) {
        if (std::fabs(x - position.x) > kDrawDistance) continue;
        DrawLine3D(Vector3{x, kCeilingHeight, std::max(minZ, position.z - kDrawDistance)},
                   Vector3{x, kCeilingHeight, std::min(maxZ, position.z + kDrawDistance)},
                   lineColor);
    }
    for (float z = minZ; z <= maxZ; z += step) {
        if (std::fabs(z - position.z) > kDrawDistance) continue;
        DrawLine3D(Vector3{std::max(minX, position.x - kDrawDistance), kCeilingHeight, z},
                   Vector3{std::min(maxX, position.x + kDrawDistance), kCeilingHeight, z},
                   lineColor);
    }
}

Vector3 addVec(Vector3 a, Vector3 b)
{
    return Vector3{a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 subVec(Vector3 a, Vector3 b)
{
    return Vector3{a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 scaleVec(Vector3 v, float scale)
{
    return Vector3{v.x * scale, v.y * scale, v.z * scale};
}

Vector3 normalizeVec(Vector3 v)
{
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (length <= 0.0001f) return Vector3{0.0f, 0.0f, 1.0f};
    const float invLength = 1.0f / length;
    return Vector3{v.x * invLength, v.y * invLength, v.z * invLength};
}

TextureRegion textureRegionForObject(GObject* object)
{
    if (const BasicEnt* basic = dynamic_cast<const BasicEnt*>(object)) {
        return spriteTextureRegion(basic->getSprite());
    }
    if (dynamic_cast<Portal*>(object)) return fullTextureRegion(portalTexture());
    if (dynamic_cast<Door*>(object)) return fullTextureRegion(doorTexture());
    if (dynamic_cast<TriggerButton*>(object)) return fullTextureRegion(buttonTexture());
    return fullTextureRegion(objectTexture());
}

Color tintForObject(GObject* object)
{
    if (dynamic_cast<Portal*>(object)) return Color{190, 90, 255, 255};
    if (dynamic_cast<Door*>(object)) return Color{185, 205, 218, 255};
    if (dynamic_cast<TriggerButton*>(object)) return Color{255, 218, 82, 255};
    if (dynamic_cast<Water*>(object)) return Color{80, 170, 255, 135};
    if (object && object->hasKillOnCollision()) return Color{255, 70, 70, 255};
    if (object && object->getLightComponent()) return object->getLightComponent()->desc().color;
    return Color{210, 218, 224, 255};
}

Color textureTintForObject(GObject* object)
{
    if (const BasicEnt* basic = dynamic_cast<const BasicEnt*>(object)) {
        if (const Sprite* sprite = basic->getSprite()) {
            return sprite->getTint();
        }
    }
    return tintForObject(object);
}

void drawBillboardProxy(Rectangle rect, const TextureRegion& region, Color tint)
{
    if (!validRect(rect)) return;

    const float t = std::max(0.06f, visualProgress());
    const float width = std::clamp(std::max(rect.width, rect.height), kObjectBillboardMinSize, kObjectBillboardMaxSize) * t;
    const float height = std::clamp(width * 1.35f, kObjectBillboardMinSize * t, kObjectBillboardMaxSize * 1.6f);
    const Vector3 center{
        rect.x + rect.width * 0.5f,
        height * 0.5f + 2.0f * t,
        rect.y + rect.height * 0.5f
    };

    const Vector3 right{std::cos(yaw + 1.57079632679f), 0.0f, std::sin(yaw + 1.57079632679f)};
    const Vector3 up{0.0f, 1.0f, 0.0f};
    const Vector3 halfRight = scaleVec(right, width * 0.5f);
    const Vector3 halfUp = scaleVec(up, height * 0.5f);
    const Vector3 normal = normalizeVec(subVec(camera.position, center));

    drawTexturedVerticalQuadStretch(
        region,
        subVec(subVec(center, halfRight), halfUp),
        addVec(subVec(center, halfUp), halfRight),
        addVec(addVec(center, halfRight), halfUp),
        addVec(subVec(center, halfRight), halfUp),
        normal,
        alphaScaled(tint, overlayAlpha()));
}

void drawWaterProxy(Water* water, Rectangle rect)
{
    if (!water || !validRect(rect)) return;

    const float t = visualProgress();
    beginLocalAlphaBlend();
        if (water->getKind() == WaterVisualKind::Waterfall) {
            const Vector3 center{
                rect.x + rect.width * 0.5f,
                kWallHeight * 0.45f * t,
                rect.y + rect.height * 0.5f
            };
            DrawCubeV(center, Vector3{std::max(rect.width, 2.0f), kWallHeight * 0.9f * t, std::max(rect.height, 2.0f)}, alphaScaled(Color{65, 170, 255, 95}, overlayAlpha()));
            DrawCubeWiresV(center, Vector3{std::max(rect.width, 2.0f), kWallHeight * 0.9f * t, std::max(rect.height, 2.0f)}, alphaScaled(Color{120, 220, 255, 180}, overlayAlpha()));
        } else {
            DrawPlane(
                Vector3{rect.x + rect.width * 0.5f, 1.2f * t, rect.y + rect.height * 0.5f},
                Vector2{rect.width, rect.height},
                alphaScaled(Color{50, 145, 230, 105}, overlayAlpha()));
            DrawCubeWiresV(
                Vector3{rect.x + rect.width * 0.5f, 1.5f * t, rect.y + rect.height * 0.5f},
                Vector3{rect.width, 2.0f * t, rect.height},
                alphaScaled(Color{120, 220, 255, 160}, overlayAlpha()));
        }
    endLocalAlphaBlend();
}

void drawPortalFrame(Rectangle rect, Color tint)
{
    const Vector3 center{
        rect.x + rect.width * 0.5f,
        kObjectBillboardMaxSize * 0.55f * visualProgress(),
        rect.y + rect.height * 0.5f
    };
    DrawCubeWiresV(center, Vector3{std::max(rect.width, 8.0f), kObjectBillboardMaxSize * std::max(0.06f, visualProgress()), std::max(rect.height, 4.0f)}, alphaScaled(tint, overlayAlpha()));
}

void drawLightProxy(GObject* object, Rectangle rect)
{
    LightComponent* light = object ? object->getLightComponent() : nullptr;
    if (!light || !light->isEnabled()) return;

    const LightDesc& desc = light->desc();
    const Vector2 lightPos = light->worldPosition();
    const float radius = std::clamp(desc.radius * 0.07f, 4.0f, 18.0f) * std::max(0.08f, visualProgress());
    const Vector3 center{lightPos.x, kEyeHeight * 0.8f * visualProgress(), lightPos.y};

    beginLocalAlphaBlend();
        DrawSphere(center, radius, alphaScaled(Color{desc.color.r, desc.color.g, desc.color.b, static_cast<unsigned char>(std::min(160.0f, 70.0f + desc.intensity * 50.0f))}, overlayAlpha()));
        DrawSphereWires(center, radius * 1.35f, 8, 8, alphaScaled(Color{desc.color.r, desc.color.g, desc.color.b, 180}, overlayAlpha()));
    endLocalAlphaBlend();
}

void drawEntityProxy(GObject* object)
{
    if (!object || dynamic_cast<Character*>(object)) return;

    if (object->is3DRenderable()) {
        if (visualProgress() < 0.72f) return;
        object->draw3D();
        return;
    }

    Rectangle rect = object->getRect();
    if (!validRect(rect)) return;
    if (distanceSqToRectXZ(rect, position) > kDrawDistance * kDrawDistance) return;

    if (Water* water = dynamic_cast<Water*>(object)) {
        drawWaterProxy(water, rect);
        return;
    }

    const Color tint = textureTintForObject(object);
    beginLocalAlphaBlend();
    const bool shaderActive = beginSceneShader(0.55f);
    drawBillboardProxy(rect, textureRegionForObject(object), tint);
    endSceneShader(shaderActive);
    endLocalAlphaBlend();

    if (dynamic_cast<Portal*>(object)) {
        drawPortalFrame(rect, tintForObject(object));
    }
    drawLightProxy(object, rect);
}

void drawWorldObjects()
{
    for (auto& [id, object] : Object_m::level_ents_) {
        drawEntityProxy(object.get());
    }
}

void clearDepthBufferOnly()
{
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
    rlColorMask(false, false, false, false);
    rlClearScreenBuffers();
    rlColorMask(true, true, true, true);
}

void setEnabled(bool value)
{
    if (enabled == value) return;
    enabled = value;

    if (enabled) {
        if (!initialized) {
            Fps3D_m::initFromCurrentLevel();
        }
        resetCameraFromPlayer();
        DisableCursor();
    } else {
        EnableCursor();
    }
}

} // namespace

void Fps3D_m::initFromCurrentLevel()
{
    wallFaces.clear();
    solidCaps.clear();
    solidCapTiles.clear();
    worldBounds = Ldtk_m::getWorldBounds();

    std::vector<Rectangle> solids;
    solids.reserve(Object_m::level_tiles_.size());

    for (auto& [id, object] : Object_m::level_tiles_) {
        if (!object) continue;
        CollisionRect* body = object->getCollisionBody();
        if (!body || !body->isSolid()) continue;
        Rectangle rect = body->getSurface();
        if (!validRect(rect)) continue;

        solids.push_back(rect);
        solidCapTiles.push_back(TexturedRect{rect, textureRegionForObject(object.get()), textureTintForObject(object.get())});
        expandBounds(rect);
    }

    if (solids.empty()) {
        std::vector<CollisionRect*> bodies = CollisionRect::all(true);
        solids.reserve(bodies.size());
        for (CollisionRect* body : bodies) {
            if (shouldIgnoreBody(body)) continue;
            Rectangle rect = body->getSurface();
            if (!validRect(rect)) continue;

            solids.push_back(rect);
            solidCapTiles.push_back(TexturedRect{rect, capTextureRegion(), Color{175, 178, 178, 255}});
            expandBounds(rect);
        }
    }

    solidCaps = mergeSolidRects(std::move(solids));
    buildWallFacesFromCaps();

    if (validRect(worldBounds)) {
        worldBounds.x -= kBoundsPadding;
        worldBounds.y -= kBoundsPadding;
        worldBounds.width += kBoundsPadding * 2.0f;
        worldBounds.height += kBoundsPadding * 2.0f;
    } else {
        worldBounds = Rectangle{-kBoundsPadding, -kBoundsPadding, kBoundsPadding * 2.0f, kBoundsPadding * 2.0f};
    }

    resetCameraFromPlayer();
    updateCameraTarget();
    initialized = true;
}

void Fps3D_m::unload()
{
    setEnabled(false);
    wallFaces.clear();
    solidCaps.clear();
    solidCapTiles.clear();
    initialized = false;
    transitionProgress = 0.0f;
    worldBounds = Rectangle{0.0f, 0.0f, 0.0f, 0.0f};
}

void Fps3D_m::toggle()
{
    setEnabled(!enabled);
}

bool Fps3D_m::isEnabled()
{
    return enabled;
}

bool Fps3D_m::isVisible()
{
    return transitionVisible();
}

bool Fps3D_m::isFully3D()
{
    return fully3D();
}

void Fps3D_m::update(float dt)
{
    if (!enabled && transitionProgress <= 0.0f) return;
    if (!initialized) initFromCurrentLevel();

    dt = std::clamp(dt, 0.0f, 0.05f);
    const float target = enabled ? 1.0f : 0.0f;
    const float duration = enabled ? kTransitionUpDuration : kTransitionDownDuration;
    const float step = dt / duration;
    if (transitionProgress < target) {
        transitionProgress = std::min(target, transitionProgress + step);
    } else if (transitionProgress > target) {
        transitionProgress = std::max(target, transitionProgress - step);
    }

    if (!enabled || transitionProgress < 0.98f) {
        updateCameraTarget();
        return;
    }

    Vector2 mouseDelta = GetMouseDelta();
    yaw += mouseDelta.x * kMouseSensitivity;
    pitch = std::clamp(pitch - mouseDelta.y * kMouseSensitivity, -kMaxPitch, kMaxPitch);

    const Vector2 forward{std::cos(yaw), std::sin(yaw)};
    const Vector2 right{-forward.y, forward.x};
    Vector2 intent{0.0f, 0.0f};

    if (InputMap::checkDown("up")) {
        intent.x += forward.x;
        intent.y += forward.y;
    }
    if (InputMap::checkDown("down")) {
        intent.x -= forward.x;
        intent.y -= forward.y;
    }
    if (InputMap::checkDown("right")) {
        intent.x += right.x;
        intent.y += right.y;
    }
    if (InputMap::checkDown("left")) {
        intent.x -= right.x;
        intent.y -= right.y;
    }

    const float lengthSq = intent.x * intent.x + intent.y * intent.y;
    if (lengthSq > 0.0001f) {
        const float invLength = 1.0f / std::sqrt(lengthSq);
        const float speed = kWalkSpeed * (IsKeyDown(KEY_LEFT_SHIFT) ? kSprintMultiplier : 1.0f);
        intent.x *= invLength * speed * dt;
        intent.y *= invLength * speed * dt;
        moveWithCollision(intent);
    }

    updateCameraTarget();
}

void Fps3D_m::draw()
{
    if (!transitionVisible()) return;
    if (!initialized) initFromCurrentLevel();

    ClearBackground(Color{13, 16, 20, 255});

    clearDepthBufferOnly();

    BeginMode3D(transitionCamera());
        if (!fully3D()) {
            drawFlat2DMap();
        }

        const bool shaderActive = beginSceneShader(0.42f, true);
        drawWorldFloor();

        const float drawDistanceSq = kDrawDistance * kDrawDistance;
        if (!solidCapTiles.empty()) {
            for (const TexturedRect& cap : solidCapTiles) {
                if (distanceSqToRectXZ(cap.rect, position) > drawDistanceSq) continue;
                drawSolidCapTile(cap);
            }
        } else {
            for (Rectangle cap : solidCaps) {
                if (distanceSqToRectXZ(cap, position) > drawDistanceSq) continue;
                drawSolidCap(cap);
            }
        }

        for (const WallFace& face : wallFaces) {
            if (distanceSqToFaceXZ(face, position) > drawDistanceSq) continue;
            drawWallFace(face);
        }
        endSceneShader(shaderActive);

        for (const WallFace& face : wallFaces) {
            if (distanceSqToFaceXZ(face, position) > drawDistanceSq) continue;
            drawWallFaceAccents(face);
        }
        drawFloorGuides();
        drawCeilingHints();
        drawWorldObjects();
    EndMode3D();

    DrawRectangle(0, 0, NATIVE_RES_WIDTH, 18, Color{0, 0, 0, 150});
    DrawText("F3 2D/3D  textures objects shaders", 6, 5, 8, Color{214, 219, 226, 255});
}
