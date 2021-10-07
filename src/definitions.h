#pragma once
#define LEVELS_PATH  "../Data/Levels/"
#define TEXTURES_PATH "../Data/Textures/"
#define POLICES_PATH  "../Data/Polices/"
#define BLUEPRINTS_PATH  "../Data/Entities/blueprints.json"
#define SAVE_PATH "../Data/Saves/"
#define MUSICS_PATH "../Data/Music/"

#define t(...) typeid(__VA_ARGS__).name()

template <typename T> int sgn(T val) {
    return (T(0) < val) - (val < T(0));
}