#pragma once
#define LEVELS_PATH  "./Data/Levels/"
#undef TILED_PATH
#define LDTK_PATH    "./Data/Ldtk/"
#define TEXTURES_PATH "./Data/Textures/"
#define POLICES_PATH  "./Data/Polices/"
#define BLUEPRINTS_PATH  "./Data/Entities/blueprints.json"
#define SAVE_PATH "./Data/Saves/"
#define MUSICS_PATH "./Data/Musics/"

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}

#define NATIVE_RES_WIDTH  320
#define NATIVE_RES_HEIGHT  240