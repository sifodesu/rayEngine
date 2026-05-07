#pragma once

#include "basicEnt.h"

#include <string>
#include <unordered_map>

class Character;

class TriggerButton : public BasicEnt {
public:
    explicit TriggerButton(const SpawnData& data);
    ~TriggerButton() override;

    void routine() override;
    void onCollision(GObject* other) override;
    void draw() override;

    bool isActivated() const { return activated_; }

    static bool isButtonActivated(const std::string& id);
    static bool getButtonRect(const std::string& id, Rectangle& out);
    static void clearPersistentState();

private:
    void updateActivated(bool activated);
    bool detectPressedCharacter() const;
    bool isCharacterPressing(const Character& character) const;
    void registerButton();
    void unregisterButton();

    bool activated_{false};
    bool staysActivated_{false};
    float contactTolerance_{2.0f};
    std::string registryId_;

    static std::unordered_map<std::string, TriggerButton*> registry_;
    static std::unordered_map<std::string, bool> persistentActivations_;
};
