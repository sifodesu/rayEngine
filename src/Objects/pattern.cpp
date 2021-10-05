#include "pattern.h"
#include "object_m.h"
#include "math.h"

void Pattern::circle(Vector2 center, int nb_bullet, int speed) {
    for (double curve = 0; curve < 360; curve += 360.0 / nb_bullet) {
        float rad = PI * curve / 180;
        float tempX = -sin(rad) * speed;
        float tempY = cos(rad) * speed;
        Bullet* bullet = (Bullet*)Object_m::createObj(BULLET);
        bullet->body_->setSpeed({ tempX, tempY });
        bullet->body_->setCoord(center);
    }
}

void Pattern::line(Vector2 origin, Vector2 direction, int nb_bullet, int speed) {
    float dirNorm = sqrt(pow(direction.x, 2) + pow(direction.y, 2));
    Vector2 unitDir = { direction.x / dirNorm, direction.y / dirNorm };
    float rad = PI * 0.5;
    Vector2 vecSpeed = { -sin(rad) * speed, cos(rad) * speed};
    for (int i = 0; i < dirNorm; i += dirNorm / nb_bullet) {
        Vector2 pos = { origin.x + (unitDir.x * i), origin.y + (unitDir.y * i) };
        Bullet* bullet = (Bullet*)Object_m::createObj(BULLET);
        bullet->body_->setSpeed(vecSpeed);
        bullet->body_->setCoord(pos);
    }
}