#include <filesystem>
#include "texture_m.h"
#include "raylib.h"

std::map<std::string, Texture2D> Texture_m::textures;

void Texture_m::load(std::string path) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!std::filesystem::is_regular_file(entry) || entry.path().extension() != ".png") {
            if (std::filesystem::is_directory(entry))
                load(entry.path().string());

            continue;
        }
        textures[entry.path().filename().string()] = LoadTexture(entry.path().string().c_str());
    }
}

void Texture_m::unload() {
    for (auto& [name, tex] : textures)
        UnloadTexture(tex);
}

Texture2D Texture_m::getTexture(std::string filename) {
    if (!textures.contains(filename))
        return textures["inv.png"];
    return textures[filename];
}
