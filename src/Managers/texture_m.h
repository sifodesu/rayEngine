#pragma once
#include <map>
#include <string>
#include "raylib.h"

class Texture_m {
public:
    Texture_m();
    void load(std::string path);
    ~Texture_m();

private:
    std::map<std::string, Texture2D> textures;
};