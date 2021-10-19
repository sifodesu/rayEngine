#pragma once
#include <string>
#include "objComponent.h"
#include "raylib.h"


class TextAnim : public ObjComponent {
public:
    TextAnim(std::string text, std::string font = "");

    void routine();
    void trigger();

private:
    std::string text;
    Rectangle rect;
    Font font;
    int speed;
};