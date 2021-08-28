#pragma once
#include <map>
#include <string>
#include "raylib.h"
#include "definitions.h"

class Texture_m {
public:
    static void load(std::string path = TEXTURES_PATH);
    static void unload();
    static Texture2D getTexture(std::string filename);

private:
    static std::map<std::string, Texture2D> textures;
};

