#pragma once
#include "basicEnt.h"
#include <vector>
#include <string>
class Character;
class AdiComponent;

// Receptacle stores deposited adi charges; when it has at least 1 it is considered active.
class Receptacle : public BasicEnt {
public:
    explicit Receptacle(const SpawnData& data);
    ~Receptacle();
    void draw() override;
    
    // Override pour le système optional
    std::optional<AdiComponent*> getAdiComponent() override { 
        return adiComponent_ ? std::optional<AdiComponent*>{adiComponent_} : std::nullopt; 
    }
    
private:
    AdiComponent* adiComponent_;
};
