#include "pattern.h"
#include "object_m.h"
#include "math.h"
#include "rigidBullet.h"
#include "bullet_m.h"
#include "raymath.h"

void Pattern::circle(RigidBullet* bp, int nb_bullets, double amplitude, Vector2 direction, double delay, float radius) {
    double angleStart = -90 + (atan(direction.y / direction.x) * 180 / PI) + (direction.x < 0 ? 180 : 0);
    for (double curve = 0; curve <= amplitude; curve += amplitude / (nb_bullets - 1)) {
        float rad = PI * (curve + angleStart - (amplitude / 2)) / 180;
        float tempX = -sin(rad) * bp->body_->getSpeed().x;
        float tempY = cos(rad) * bp->body_->getSpeed().y;
        RigidBullet* bullet = (RigidBullet*)Bullet_m::createBullet(RIGID, bp->no_dmg_, !curve ? 0 : delay);
        *bullet = *bp;
        bullet->body_->setSpeed({ tempX, tempY });

        Vector2 pos = bp->body_->getCoord();
        Vector2 unitDir = Vector2Normalize(bullet->body_->getSpeed());
        bullet->body_->setCoord({ pos.x + (unitDir.x * radius), pos.y + (unitDir.y * radius) });
    }
}

void Pattern::line(RigidBullet* bp, Vector2 origin, Vector2 direction, double center_acc, int nb_bullet) {
    float dirNorm = Vector2Length(direction);
    Vector2 unitDir = Vector2Normalize(direction);
    Vector2 vecSpeed = Vector2Rotate(unitDir, 90);
    vecSpeed = Vector2Multiply(vecSpeed, bp->body_->getSpeed());

    for (float i = 0; i <= dirNorm; i += dirNorm / (nb_bullet - 1)) {
        Vector2 pos = { origin.x + (unitDir.x * i), origin.y + (unitDir.y * i) };
        RigidBullet* bullet = (RigidBullet*)Bullet_m::createBullet(RIGID, bp->no_dmg_);
        *bullet = *bp;
        bullet->body_->setCoord(pos);
        if (i / dirNorm <= 0.5) {
            float factor = 1 + (center_acc * 2 * i / dirNorm);
            bullet->body_->setSpeed({ vecSpeed.x * factor, vecSpeed.y * factor });
        }
        else {
            float factor = 1 + (2 * (1 - (i / dirNorm)) * center_acc);
            bullet->body_->setSpeed({ vecSpeed.x * factor, vecSpeed.y * factor });
        }
    }
}