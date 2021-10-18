#include <filesystem>
#include "definitions.h"
#include "sound_m.h"

std::map<std::string, Music> Sound_m::sounds;

void Sound_m::load(std::string path) {
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (!std::filesystem::is_regular_file(entry) || entry.path().extension() != ".mp3") {
            if (std::filesystem::is_directory(entry))
                load(entry.path().string());
            continue;
        }
        sounds[entry.path().filename().string()] = LoadMusicStream(entry.path().string().c_str());
    }
}

void Sound_m::unload() {
    for (auto& [name, snd] : sounds) {
        UnloadMusicStream(snd);
    }
}

Music Sound_m::getSound(std::string filename) {
    if (!sounds.contains(filename))
        return sounds.begin()->second;
    return sounds[filename];
}
