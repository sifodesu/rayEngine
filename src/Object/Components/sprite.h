#pragma once
#include <string>
#include "objComponent.h"
#include "clock.h"



// class sprite_group {
// public:
//     sprite_group(std::vector<SDL_Texture*> t, int s, std::string n, bool playOnce = false) : speed(s), name(n), idle(false),
//         playOnce(playOnce), done(false), index(0) {
//         for (int i = 0; i < t.size(); i++) {
//             textures.push_back(t[i]);
//         }
//         timerA = SDL_GetTicks();
//     }
//     //~sprite_group() {
//     //	for (SDL_Texture* texture : textures) {
//     //		SDL_DestroyTexture(texture);
//     //	}
//     //}

//     std::vector<SDL_Texture*> textures;
//     int speed;
//     std::string name;
//     int idle;
//     bool playOnce;
//     bool done;
//     int index;
//     Uint32 timerA;
// };


// class Sprite {
// public:
//     Sprite();
//     SDL_Texture* currentFrame();
//     void setCurrentGroup(std::string group);
//     void addGroup(std::string sheet_name, int width_per_sprite, int height_per_sprite, int offsetX, int offsetY, int row_index, int nb_of_frames,
//         std::string group_name, int speed = 100, bool playOnce = false, SDL_Color alpha = { ALPHAR, ALPHAG, ALPHAB });
//     void setIdle(bool idle);
//     void setSingleFrame(std::string textureName);	//For objects with just a single texture
//     void clear();
//     std::string resource;
//     bool signalDone;	//true when the current playOnce sheet is done
// private:
//     int currentGroup;

//     std::vector<sprite_group> groups;
// };



class Sprite : public ObjComponent {
public:
    Sprite(std::string meta);
    void routine();

private:
    Clock clock;
};