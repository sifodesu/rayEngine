#include <filesystem>
#include "texture_m.h"
#include "raylib.h"

Texture_m::Texture_m(std::string path) {
    if (!path.empty())
        load(path);
}

void Texture_m::load(std::string path) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!std::filesystem::is_regular_file(entry) || entry.path().extension() != ".png") {
            if (std::filesystem::is_directory(entry))
                load(entry.path());

            continue;
        }
        textures[entry.path().filename()] = LoadTexture(entry.path().c_str());
    }
}