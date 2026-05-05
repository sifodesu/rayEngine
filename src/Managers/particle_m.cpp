#include "particle_m.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "raymath.h"

namespace {

constexpr size_t MAX_PARTICLES = 700;

enum class ParticleStyle {
    Circle,
    Streak
};

struct Particle {
    Vector2 pos;
    Vector2 vel;
    Vector2 accel;
    float age;
    float life;
    float startSize;
    float endSize;
    float length;
    Color color;
    ParticleStyle style;
};

std::vector<Particle> particles;
Particle_m::Params particleParams;

float rand01() {
    return static_cast<float>(GetRandomValue(0, 10000)) / 10000.0f;
}

float randRange(float minValue, float maxValue) {
    return minValue + (maxValue - minValue) * rand01();
}

Color withAlpha(Color color, float alpha) {
    color.a = static_cast<unsigned char>(std::clamp(alpha, 0.0f, 255.0f));
    return color;
}

Vector2 gravityVector(GravityDirection dir) {
    switch (dir) {
        case GravityDirection::DOWN:
            return {0.0f, 1.0f};
        case GravityDirection::UP:
            return {0.0f, -1.0f};
        case GravityDirection::LEFT:
            return {-1.0f, 0.0f};
        case GravityDirection::RIGHT:
            return {1.0f, 0.0f};
    }
    return {0.0f, 1.0f};
}

Vector2 surfaceNormal(GravityDirection dir) {
    Vector2 g = gravityVector(dir);
    return {-g.x, -g.y};
}

Vector2 surfaceTangent(GravityDirection dir) {
    switch (dir) {
        case GravityDirection::DOWN:
        case GravityDirection::UP:
            return {1.0f, 0.0f};
        case GravityDirection::LEFT:
        case GravityDirection::RIGHT:
            return {0.0f, 1.0f};
    }
    return {1.0f, 0.0f};
}

Vector2 pointOnContactSide(Rectangle rect, GravityDirection dir, float along) {
    along = std::clamp(along, 0.0f, 1.0f);
    switch (dir) {
        case GravityDirection::DOWN:
            return {rect.x + rect.width * along, rect.y + rect.height + 0.5f};
        case GravityDirection::UP:
            return {rect.x + rect.width * along, rect.y - 0.5f};
        case GravityDirection::LEFT:
            return {rect.x - 0.5f, rect.y + rect.height * along};
        case GravityDirection::RIGHT:
            return {rect.x + rect.width + 0.5f, rect.y + rect.height * along};
    }
    return {rect.x + rect.width * along, rect.y + rect.height};
}

Vector2 pointOnWaterEntry(Rectangle source, Rectangle water, GravityDirection dir) {
    const float sourceCenterX = source.x + source.width * 0.5f;
    const float sourceCenterY = source.y + source.height * 0.5f;

    switch (dir) {
        case GravityDirection::DOWN:
            return {
                std::clamp(sourceCenterX, water.x, water.x + water.width),
                water.y + 0.5f
            };
        case GravityDirection::UP:
            return {
                std::clamp(sourceCenterX, water.x, water.x + water.width),
                water.y + water.height - 0.5f
            };
        case GravityDirection::LEFT:
            return {
                water.x + water.width - 0.5f,
                std::clamp(sourceCenterY, water.y, water.y + water.height)
            };
        case GravityDirection::RIGHT:
            return {
                water.x + 0.5f,
                std::clamp(sourceCenterY, water.y, water.y + water.height)
            };
    }
    return {sourceCenterX, water.y};
}

Color pickDustColor() {
    const float t = rand01();
    const Color a = particleParams.dustColor;
    const Color b = particleParams.dustHighlightColor;
    return {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t)
    };
}

Color pickWaterColor() {
    const float t = rand01();
    const Color a = particleParams.waterColor;
    const Color b = particleParams.foamColor;
    return {
        static_cast<unsigned char>(a.r + (b.r - a.r) * t),
        static_cast<unsigned char>(a.g + (b.g - a.g) * t),
        static_cast<unsigned char>(a.b + (b.b - a.b) * t),
        static_cast<unsigned char>(a.a + (b.a - a.a) * t)
    };
}

int scaledCount(int count) {
    if (count <= 0 || particleParams.density <= 0.0f) return 0;
    return std::max(1, static_cast<int>(std::round(count * particleParams.density)));
}

void pushParticle(const Particle& particle) {
    if (particles.size() >= MAX_PARTICLES) {
        particles.erase(particles.begin(), particles.begin() + static_cast<long>(std::min<size_t>(particles.size(), 24)));
    }
    particles.push_back(particle);
}

void emitSurfaceDust(Rectangle source, GravityDirection dir, int count, float strength, bool landing) {
    if (!particleParams.enabled) return;
    count = scaledCount(count);
    if (count <= 0) return;

    Vector2 normal = surfaceNormal(dir);
    Vector2 tangent = surfaceTangent(dir);
    Vector2 gravity = gravityVector(dir);

    for (int i = 0; i < count; ++i) {
        Vector2 base = pointOnContactSide(source, dir, randRange(0.12f, 0.88f));
        base.x += tangent.x * randRange(-1.5f, 1.5f);
        base.y += tangent.y * randRange(-1.5f, 1.5f);

        const float tangentSpeed = (landing ? randRange(-70.0f, 70.0f) : randRange(-48.0f, 48.0f)) * particleParams.dustSpeedScale;
        const float normalSpeed = (landing ? randRange(8.0f, 24.0f) : randRange(14.0f, 38.0f)) * particleParams.dustSpeedScale;
        Particle p{};
        p.pos = base;
        p.vel = {
            tangent.x * tangentSpeed + normal.x * normalSpeed,
            tangent.y * tangentSpeed + normal.y * normalSpeed
        };
        p.accel = {gravity.x * 180.0f * particleParams.gravityScale, gravity.y * 180.0f * particleParams.gravityScale};
        p.age = 0.0f;
        p.life = randRange(0.18f, 0.34f) * std::clamp(strength, 0.75f, 1.7f) * particleParams.lifetimeScale;
        p.startSize = randRange(0.8f, landing ? 1.7f : 1.35f) * particleParams.sizeScale;
        p.endSize = 0.1f;
        p.length = 0.0f;
        p.color = pickDustColor();
        p.style = ParticleStyle::Circle;
        pushParticle(p);
    }
}

} // namespace

void Particle_m::update(float dt) {
    if (dt <= 0.0f) return;
    dt = std::min(dt, 0.05f);

    for (Particle& p : particles) {
        p.age += dt;
        p.vel.x += p.accel.x * dt;
        p.vel.y += p.accel.y * dt;
        p.pos.x += p.vel.x * dt;
        p.pos.y += p.vel.y * dt;
    }

    particles.erase(
        std::remove_if(particles.begin(), particles.end(), [](const Particle& p) {
            return p.age >= p.life;
        }),
        particles.end());
}

void Particle_m::draw() {
    for (const Particle& p : particles) {
        if (p.life <= 0.0f) continue;
        const float t = std::clamp(p.age / p.life, 0.0f, 1.0f);
        const float fade = (1.0f - t) * (1.0f - t);
        const float size = p.startSize + (p.endSize - p.startSize) * t;
        Color color = withAlpha(p.color, static_cast<float>(p.color.a) * fade);

        if (p.style == ParticleStyle::Streak) {
            Vector2 dir = Vector2Normalize(p.vel);
            if (std::fabs(dir.x) < 0.001f && std::fabs(dir.y) < 0.001f) {
                dir = {0.0f, 1.0f};
            }
            Vector2 tail = {
                p.pos.x - dir.x * p.length * (0.7f + 0.3f * fade),
                p.pos.y - dir.y * p.length * (0.7f + 0.3f * fade)
            };
            DrawLineEx(tail, p.pos, std::max(1.0f, size), color);
        } else {
            DrawCircleV(p.pos, std::max(0.25f, size), color);
        }
    }
}

void Particle_m::clear() {
    particles.clear();
}

int Particle_m::activeCount() {
    return static_cast<int>(particles.size());
}

Particle_m::Params& Particle_m::params() {
    return particleParams;
}

void Particle_m::resetParams() {
    particleParams = Params{};
}

void Particle_m::emitJumpDust(Rectangle source, GravityDirection gravityDir) {
    emitSurfaceDust(source, gravityDir, particleParams.jumpDustCount, 1.0f, false);
}

void Particle_m::emitLandDust(Rectangle source, GravityDirection gravityDir, float strength) {
    const int count = particleParams.landDustBaseCount +
        static_cast<int>(std::round(std::clamp(strength, 0.0f, 1.8f) * particleParams.landDustStrengthCount));
    emitSurfaceDust(source, gravityDir, count, strength, true);
}

void Particle_m::emitWaterSplash(Rectangle source, Rectangle waterRect, GravityDirection gravityDir, float strength) {
    if (!particleParams.enabled) return;

    Vector2 base = pointOnWaterEntry(source, waterRect, gravityDir);
    Vector2 normal = surfaceNormal(gravityDir);
    Vector2 tangent = surfaceTangent(gravityDir);
    Vector2 gravity = gravityVector(gravityDir);

    const int count = scaledCount(particleParams.splashBaseCount +
        static_cast<int>(std::round(std::clamp(strength, 0.0f, 2.0f) * particleParams.splashStrengthCount)));
    for (int i = 0; i < count; ++i) {
        Particle p{};
        p.pos = {
            base.x + tangent.x * randRange(-4.5f, 4.5f),
            base.y + tangent.y * randRange(-4.5f, 4.5f)
        };
        const float tangentSpeed = randRange(-78.0f, 78.0f) * std::clamp(strength, 0.65f, 1.6f) * particleParams.waterSpeedScale;
        const float normalSpeed = randRange(42.0f, 98.0f) * std::clamp(strength, 0.65f, 1.8f) * particleParams.waterSpeedScale;
        p.vel = {
            tangent.x * tangentSpeed + normal.x * normalSpeed,
            tangent.y * tangentSpeed + normal.y * normalSpeed
        };
        p.accel = {gravity.x * 230.0f * particleParams.gravityScale, gravity.y * 230.0f * particleParams.gravityScale};
        p.age = 0.0f;
        p.life = randRange(0.28f, 0.56f) * particleParams.lifetimeScale;
        p.startSize = randRange(0.75f, 1.55f) * particleParams.sizeScale;
        p.endSize = 0.2f;
        p.length = randRange(2.5f, 5.5f);
        p.color = pickWaterColor();
        p.style = (i % 3 == 0) ? ParticleStyle::Streak : ParticleStyle::Circle;
        pushParticle(p);
    }
}
