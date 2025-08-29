#pragma once
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>
#include "spawn.h"

class Character;

/**
 * Component modulaire pour gérer les interactions ADI
 * Peut être configuré pour différents comportements :
 * - Recevoir des ADI du joueur
 * - Être triggered par d'autres objets  
 * - Trigger d'autres objets (si targetIds non vide)
 */
class AdiComponent {
public:
    // Configuration du comportement
    bool canReceiveAdi = false;      // Peut recevoir des ADI du joueur
    bool canBeTriggered = false;     // Peut être triggered par d'autres objets
    
    // Paramètres de réception d'ADI
    int maxCapacity = 1;             // Capacité max (-1 = illimité)
    int currentAdi = 0;              // ADI actuellement stockés
    
    // Paramètres d'activation (état interne)
    bool isActivated = false;        // État d'activation actuel (son propre état)
    std::vector<std::string> targetIds; // IDs des objets à trigger
    int activationThreshold = 1;     // Nombre d'ADI requis pour s'activer
    
    // Constructeur et destructeur
    AdiComponent() = default;
    explicit AdiComponent(const AdiComponentDesc& desc);
    ~AdiComponent();
    
    // Méthodes principales
    bool depositAdi(Character& character);
    bool withdrawAdi(Character& character);
    
    // Gestion de l'état interne
    void setActivated(bool activated);
    bool getActivated() const { return isActivated; }
    
    // Gestion du trigger externe (venant d'autres objets)
    void setTriggered(bool triggered);
    
    // Routine pour gérer les updates
    void routine();
    
    // Utilitaires
    bool canReceiveMoreAdi() const;
    bool hasAdi() const { return currentAdi > 0; }
    int getStoredAdi() const { return currentAdi; }
    
    // Callbacks optionnels
    std::function<void(int oldAdi, int newAdi)> onAdiChanged;
    std::function<void(bool activated)> onActivationChanged;
    std::function<void(bool triggered)> onTriggered;
    
    // Fonctions utilitaires pour les interactions entre objets
    static void registerForTrigger(const std::string& id, AdiComponent* component);
    static void unregisterForTrigger(const std::string& id);
    static void clearTriggerRegistry();
    
private:
    void updateActivation();  // Met à jour son propre état selon les ADI
    void triggerTargets(bool activated);  // Trigger les objets cibles
    
    // État interne pour détecter les changements
    bool wasActivated = false;
    
    // ID LDtk pour auto-désenregistrement
    std::optional<std::string> ldtkId_;
    
    // Registre minimal pour les interactions (juste les IDs vers composants)
    static std::unordered_map<std::string, AdiComponent*> triggerRegistry_;
};
