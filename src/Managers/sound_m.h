#pragma once
#include <map>
#include "raylib.h"

class Sound_m {
public:
    static void load(std::string path = MUSICS_PATH);
    static void unload();
    static Music getSound(std::string filename);

private:
    static std::map<std::string, Music> sounds;
};