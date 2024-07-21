#include "tiled_m.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <vector>
#include "object_m.h"

using namespace std;
using json = nlohmann::json;

typedef struct {
    string texture_name;
    int x, y, w, h;
    bool solid;
} tile_blueprint;

void Tiled_m::loadLevel(std::string filename)
{
    std::ifstream file((TILED_PATH + filename).c_str());
    if (!file)
    {
        cout << "Error: Couldn't load the level " << filename << endl;
        return;
    }

    json tiledMap;
    file >> tiledMap;

    vector<tile_blueprint> tiles;
    map<string, int> tilesets; //indexes in tiles[]
    //Load tilesets
    for (auto& tileset : tiledMap["tilesets"])
    {
        int gid = tileset["firstgid"];
        string source = tileset["source"];
        std::ifstream tsj((TILED_PATH + source).c_str());
        tsj >> tileset;
        cout << "loading tileset " << tileset["name"] << endl;

        tilesets[tileset["name"]] = tiles.size();
        for (int i = 0, x = 0; i < tileset["tilecount"]; i++)
        {
            int columns = tileset["columns"];
            int y = int(i/columns);
            int w = tileset["tilewidth"];
            int h = tileset["tileheight"];
            string image = tileset["image"];
            size_t lastSlash = image.find_last_of('/');
            image = image.substr(lastSlash + 1);
            bool solid = true;
            
            if (tileset.contains("tiles"))
            {
                for (auto& prop : tileset["tiles"][i]["properties"])
                {
                    if (prop["name"] == "solid")
                    {
                        solid = prop["value"];
                    }
                }
            }

            tiles.push_back({image, x*w, y*h, w, h, solid});
            
            x = (x + 1)%columns;
        }
    }

    map<string, json> templates;
    int layer_num = 0;
    int tilewidth = tiledMap["tilewidth"];
    int tileheight = tiledMap["tileheight"];
    for (auto& layer : tiledMap["layers"])
    {
        if (layer["type"] == "tilelayer")
        {
            int x = layer["x"];
            int y = layer["y"];
            int w = layer["width"];
            int h = layer["height"];
            for (int j = 0; j < h; j++)
            {
                for (int i = 0; i < w; i++)
                {
                    int gid = layer["data"][j*w + i];
                    if (!gid)
                    {
                        continue;
                    }
                    tile_blueprint& bp = tiles[gid-1];
                    int posX = x + i*tilewidth;
                    int posY = y + j*tileheight;

                    json ent;
                    ent["type"] = "tile";
                    ent["sprite"]["filename"] = bp.texture_name;
                    ent["sprite"]["source"]["x"] = bp.x;
                    ent["sprite"]["source"]["y"] = bp.y; 
                    ent["sprite"]["source"]["w"] = bp.w;
                    ent["sprite"]["source"]["h"] = bp.h;
                    ent["collisionRect"]["x"] = posX;
                    ent["collisionRect"]["y"] = posY;
                    ent["collisionRect"]["w"] = tilewidth;
                    ent["collisionRect"]["h"] = tileheight;
                    ent["collisionRect"]["solid"] = bp.solid;
                    ent["layer"] = layer_num;
                    Object_m::createObjJson(ent);
                }
            }
        }
        if (layer["type"] == "objectgroup")
        {
            for (auto& obj : layer["objects"])
            {
                json ent;
                //template:
                // height, width, type(class)
                // instance:
                // x, y, type, height, width
                if (obj.contains("template"))
                {
                    string source = obj["template"];
                    if (!templates.contains(obj["template"]))
                    {
                        std::ifstream tj((TILED_PATH + source).c_str());
                        json templateJson;
                        tj >> templateJson;
                        templates[source] = templateJson;
                    }

                    json tj = templates[source];
                    ent["type"] = tj["object"]["type"];
                    ent["collisionRect"]["w"] = tj["object"]["width"];
                    ent["collisionRect"]["h"] = tj["object"]["height"];
                    int firstgid = tj["tileset"]["firstgid"];
                    int gid = tj["object"]["gid"];
                    int tileIndex = gid - firstgid;

                    tile_blueprint &bp = tiles[tilesets[tj["tileset"]["source"]] + tileIndex];

                    ent["sprite"]["filename"] = bp.texture_name;
                    ent["sprite"]["source"]["x"] = bp.x;
                    ent["sprite"]["source"]["y"] = bp.y; 
                    ent["sprite"]["source"]["w"] = bp.w;
                    ent["sprite"]["source"]["h"] = bp.h;
                }

                ent["collisionRect"]["x"] = obj["x"];
                ent["collisionRect"]["y"] = obj["y"];
                if (obj.contains("width"))
                    ent["collisionRect"]["w"] = obj["width"];
                if (obj.contains("height"))
                    ent["collisionRect"]["w"] = obj["height"];
                if (obj.contains("type"))
                    ent["type"] = obj["type"];
                
                ent["collisionRect"]["static"] = false; //objets are not static.
                ent["layer"] = layer_num;
                Object_m::createObjJson(ent);
            }
        }

        layer_num++;
    }

    file.close();
}