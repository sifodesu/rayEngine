#pragma once

#include "basicEnt.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class Door : public BasicEnt {
public:
    explicit Door(const SpawnData& data);

    void routine() override;
    void draw() override;

    static void clearPersistentState();

private:
    bool shouldOpen() const;
    void setOpen(bool open);
    bool isIndicatorLit(std::size_t index) const;
    void drawIndicators(Rectangle rect) const;

    std::vector<std::string> buttonIds_;
    std::string registryId_;
    bool open_{false};

    static std::unordered_map<std::string, bool> persistentOpen_;
};
