#pragma once
#include <raylib.h>
#include <optional>
#include <string>

// Simple persistent save handling for last checkpoint.
// Stores a tiny JSON file (save.json) at runtime directory.
class SaveManager {
public:
    struct Data {
        Vector2 lastCheckpoint;
        bool hasCheckpoint;
        Data() : lastCheckpoint{0,0}, hasCheckpoint(false) {}
    };

    static void load();              // load save.json if present
    static void save();              // write current state
    static void registerCheckpoint(Vector2 pos); // update checkpoint + save
    static void applyToWorld();      // find Character and set its respawn + move it on start
    static const Data& get();
private:
    static inline Data data_{}; // zero-initialized via ctor
    static inline bool loaded_ = false;
    static std::string saveFilePath();
};
