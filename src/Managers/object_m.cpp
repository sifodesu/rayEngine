#include <fstream>
#include "definitions.h"
#include "object_m.h"
#include "sprite.h"
#include "rigidBody.h"
#include "basicEnt.h"
#include "character.h"

using json = nlohmann::json;
using namespace std;

std::map<int, GObject *> Object_m::level_ents_;  // ents of the current level
std::map<int, GObject *> Object_m::level_tiles_; // won't call routines on them
int Object_m::idCounter = 0;

std::string Object_m::level_name_;

int Object_m::genID()
{
    return ++idCounter;
}

GObject *Object_m::createObjJson(json ojs)
{
    ojs["ID"] = genID();

    if (!ojs.contains("type"))
    {
        cout << "Error: object with no type" << endl;
        return NULL;
    }
    GObject *cur = NULL;
    if (ojs["type"] == "basic")
        cur = new BasicEnt(ojs);
    if (ojs["type"] == "tile")
        cur = new BasicEnt(ojs);
    if (ojs["type"] == "character")
        cur = new Character(ojs);
    if (cur)
    {
        if (ojs.contains("layer"))
            cur->layer_ = ojs["layer"];
        if (ojs["type"] == "tile")
            level_tiles_[ojs["ID"]] = cur;
        else
            level_ents_[ojs["ID"]] = cur;
    }
    return cur;
}

void Object_m::loadLevel(string level_name)
{
    std::ifstream file((LEVELS_PATH + level_name).c_str());
    if (!file)
    {
        cout << "Error: Couldn't load the level " << level_name << endl;
        return;
    }
    // level_ents_.clear();

    json objArray;
    file >> objArray;

    for (auto &ojs : objArray)
    {
        createObjJson(ojs);
    }
    file.close();
}

void Object_m::deleteObj(int id)
{
    if (level_ents_.contains(id))
    {
        delete (level_ents_[id]);
        level_ents_.erase(id);
    }
}

void Object_m::routine()
{
    int nb_ents = level_ents_.size() + level_tiles_.size();

    DrawText(std::to_string(nb_ents).c_str(), 200, 10, 20, BLACK);

    vector<int> toDelete;
    for (auto &[id, obj] : level_ents_)
    {
        obj->routine();
        if (obj->to_delete_)
        {
            toDelete.push_back(id);
        }
    }

    for (int id : toDelete)
    {
        deleteObj(id);
    }
}

void Object_m::unload()
{
    for (auto &[id, obj] : level_ents_)
        delete (obj);
    level_ents_.clear();
}