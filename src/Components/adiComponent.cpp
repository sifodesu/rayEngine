#include "adiComponent.h"
#include "character.h"

// Registre statique minimal pour les interactions
std::unordered_map<std::string, AdiComponent*> AdiComponent::triggerRegistry_;

AdiComponent::AdiComponent(const AdiComponentDesc& desc, const std::string& ldtkId)
    : canReceiveAdi(desc.canReceiveAdi)
    , canBeTriggered(desc.canBeTriggered)
    , maxCapacity(desc.maxCapacity)
    , activationThreshold(desc.activationThreshold)
    , targetIds(desc.targetIds)
    , ldtkId_(ldtkId)
{
    // S'auto-enregistrer dans le registre si un ID LDtk est fourni
    if (!ldtkId_.empty()) {
        registerForTrigger(ldtkId_, this);
    }
}

AdiComponent::~AdiComponent() {
    // S'auto-dés-enregistrer du registre si on était enregistré
    if (!ldtkId_.empty()) {
        unregisterForTrigger(ldtkId_);
    }
}

bool AdiComponent::depositAdi(Character& character) {
    if (!canReceiveAdi) return false;
    if (!canReceiveMoreAdi()) return false;
    if (!character.canDepositAdi()) return false;
    if (!character.depositOneAdi()) return false;
    
    int oldAdi = currentAdi;
    currentAdi++;
    
    // Notifier le changement
    if (onAdiChanged) {
        onAdiChanged(oldAdi, currentAdi);
    }
    
    // Mettre à jour l'état d'activation
    updateActivation();
    
    return true;
}

bool AdiComponent::withdrawAdi(Character& character) {
    if (!canReceiveAdi) return false;
    if (currentAdi <= 0) return false;
    if (!character.retrieveOneAdi()) return false;
    
    int oldAdi = currentAdi;
    currentAdi--;
    
    // Notifier le changement
    if (onAdiChanged) {
        onAdiChanged(oldAdi, currentAdi);
    }
    
    // Mettre à jour l'état d'activation
    updateActivation();
    
    return true;
}

void AdiComponent::setActivated(bool activated) {
    if (isActivated != activated) {
        isActivated = activated;
        
        // Notifier le changement d'activation
        if (onActivationChanged) {
            onActivationChanged(activated);
        }
    }
}

int AdiComponent::clearStoredAdi(Character* refundTarget) {
    int oldAdi = currentAdi;
    if (oldAdi <= 0) return 0;

    currentAdi = 0;
    if (refundTarget) {
        for (int i = 0; i < oldAdi; ++i) {
            refundTarget->retrieveOneAdi();
        }
    }

    if (onAdiChanged) {
        onAdiChanged(oldAdi, currentAdi);
    }

    updateActivation();
    return oldAdi;
}

void AdiComponent::setTriggered(bool triggered) {
    setTriggeredBy("", triggered);
}

void AdiComponent::setTriggeredBy(const std::string& sourceId, bool triggered) {
    if (canBeTriggered) {
        // Notifier qu'on a été triggered
        if (onTriggeredBy) {
            onTriggeredBy(sourceId, triggered);
        }
        if (onTriggered) {
            onTriggered(triggered);
        }
    }
}

void AdiComponent::routine() {
    // Détecter si l'état d'activation a changé depuis la dernière frame
    if (isActivated != wasActivated) {
        wasActivated = isActivated;
    }
}

bool AdiComponent::canReceiveMoreAdi() const {
    if (maxCapacity == -1) return true; // Capacité illimitée
    return currentAdi < maxCapacity;
}

void AdiComponent::updateActivation() {
    // Vérifie si cet objet devrait être activé selon ses ADI
    bool shouldBeActivated = (currentAdi >= activationThreshold);
    
    // Si son état d'activation change
    if (shouldBeActivated != isActivated) {
        setActivated(shouldBeActivated);  // Change SON état
        triggerTargets(shouldBeActivated); // Trigger les autres objets
    }
}

void AdiComponent::triggerTargets(bool activated) {
    // Trigger directement les objets cibles via le registre minimal
    for (const std::string& targetId : targetIds) {
        auto it = triggerRegistry_.find(targetId);
        if (it != triggerRegistry_.end() && it->second) {
            AdiComponent* target = it->second;
            if (target->canBeTriggered) {
                target->setTriggeredBy(ldtkId_, activated);
            }
        }
    }
}

// Fonctions utilitaires pour les interactions
void AdiComponent::registerForTrigger(const std::string& id, AdiComponent* component) {
    if (component) {
        triggerRegistry_[id] = component;
    }
}

void AdiComponent::unregisterForTrigger(const std::string& id) {
    triggerRegistry_.erase(id);
}

AdiComponent* AdiComponent::findRegistered(const std::string& id) {
    auto it = triggerRegistry_.find(id);
    if (it == triggerRegistry_.end()) return nullptr;
    return it->second;
}

void AdiComponent::clearTriggerRegistry() {
    triggerRegistry_.clear();
}
