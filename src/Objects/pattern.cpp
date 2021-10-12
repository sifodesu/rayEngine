#include "pattern.h"
#include "object_m.h"
#include "math.h"
#include "rigidBullet.h"
#include "bullet_m.h"

void Pattern::circle(RigidBullet* bp, int nb_bullets) {
    for (double curve = 0; curve < 360; curve += 360.0 / nb_bullets) {
        float rad = PI * curve / 180;
        float tempX = -sin(rad) * bp->body_->getSpeed().x;
        float tempY = cos(rad) * bp->body_->getSpeed().y;
        RigidBullet* bullet = (RigidBullet*)Bullet_m::createBullet(RIGID, bp->no_dmg_);
        *bullet = *bp;
        bullet->body_->setSpeed({ tempX, tempY });
    }
}

void Pattern::line(RigidBullet* bp, Vector2 origin, Vector2 direction, double center_acc, int nb_bullet) {
    float dirNorm = sqrt(pow(direction.x, 2) + pow(direction.y, 2));
    Vector2 unitDir = { direction.x / dirNorm, direction.y / dirNorm };
    float rad = PI * 0.5;
    Vector2 vecSpeed = { cos(rad) * unitDir.x - sin(rad) * unitDir.y, sin(rad) * unitDir.x + cos(rad) * unitDir.y };
    vecSpeed.x *= bp->body_->getSpeed().x;
    vecSpeed.y *= bp->body_->getSpeed().y;

    // float tempX = cos(curve_ * delta) * speed_.x - sin(curve_ * delta) * speed_.y;
    // float tempY = sin(curve_ * delta) * speed_.x + cos(curve_ * delta) * speed_.y;

    for (float i = 0; i < dirNorm; i += dirNorm / nb_bullet) {
        Vector2 pos = { origin.x + (unitDir.x * i), origin.y + (unitDir.y * i) };
        RigidBullet* bullet = (RigidBullet*)Bullet_m::createBullet(RIGID, bp->no_dmg_);
        *bullet = *bp;
        bullet->body_->setCoord(pos);
        if (i / dirNorm <= 0.5) {
            float factor = 1 + (center_acc * 2 * i / dirNorm);
            bullet->body_->setSpeed({ vecSpeed.x * factor, vecSpeed.y* factor });
        }
        else {
            float factor = 1 + (2 * (1 - (i / dirNorm)) * center_acc);
            bullet->body_->setSpeed({ vecSpeed.x * factor, vecSpeed.y * factor });
        }
    }
}