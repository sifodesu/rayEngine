#include "pattern.h"
#include "object_m.h"
#include "math.h"
void Pattern::circle(Vector2 pos) {
    Vector2 speed = { 0, -50 };
    double nb_bullet = 20;
    std::vector<GObject*> bullets;

    for (double curve = 0; curve < 360; curve += 360 / nb_bullet) {
        float rad = PI * curve / 180;
        float tempX = cos(rad) * speed.x - sin(rad) * speed.y;
        float tempY = sin(rad) * speed.x + cos(rad) * speed.y;
        Bullet* bullet = (Bullet*)Object_m::createObj(BULLET);
        bullet->body_->setSpeed({ tempX, tempY });
        bullets.push_back(bullet);
    }

    //fix for wrong position due to memory allocation delay
    for(auto obj : bullets){
        ((Bullet*)obj)->body_->setCoord(pos);
    }

}

void Pattern::line(Vector2 origin, Vector2 direction){
    Vector2 speed = { 0, -100 };
    double nb_bullet = 15;

    // for (int i = 0; i < nb_bullet; i++) {




    //     float rad = 3.1415 * curve / 180;
    //     float tempX = cos(rad) * speed.x - sin(rad) * speed.y;
    //     float tempY = sin(rad) * speed.x + cos(rad) * speed.y;
    //     Bullet* bullet = (Bullet*)Object_m::createObj(BULLET);
    //     bullet->body_->setSpeed({ tempX, tempY });
    //     bullet->body_->setCoord(pos);
    // }
}