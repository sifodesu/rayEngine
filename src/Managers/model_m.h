#pragma once
#include <map>
#include <string>
#include "raylib.h"
#include "definitions.h"

class Model_m {
public:
    static void load(std::string path = MODELS_PATH);
    static void unload();
    static Model* getModel(const std::string& filename);

private:
    static std::map<std::string, Model> models_;
    // basename -> unique relative key; empty means ambiguous basename
    static std::map<std::string, std::string> basenameToKey_;
};
