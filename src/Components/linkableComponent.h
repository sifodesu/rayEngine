#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <optional>

class GObject;

/**
 * Generic linkable component system for connecting objects by ID
 * Similar to ADI system but more flexible for different types of links
 */
class LinkableComponent {
public:
    // Constructor
    LinkableComponent(GObject* owner, const std::string& linkId = "");
    ~LinkableComponent();
    
    // Core linking functionality
    void setLinkId(const std::string& id);
    std::string getLinkId() const { return linkId_; }
    
    void addTargetId(const std::string& targetId);
    void removeTargetId(const std::string& targetId);
    void clearTargets();
    std::vector<std::string> getTargetIds() const { return targetIds_; }
    
    // Get linked objects
    std::vector<GObject*> getLinkedObjects() const;
    GObject* getLinkedObject(const std::string& targetId) const;
    
    // Trigger system - notify linked objects
    void triggerTargets(const std::string& message = "", void* data = nullptr);
    void onTriggered(const std::string& sourceId, const std::string& message = "", void* data = nullptr);
    
    // Callbacks
    std::function<void(const std::string& sourceId, const std::string& message, void* data)> onTriggerReceived;
    
    // Static registry management
    static void registerObject(const std::string& id, GObject* object);
    static void unregisterObject(const std::string& id);
    static GObject* getObjectById(const std::string& id);
    static void clearRegistry();
    
    // Utility methods for common patterns
    static void linkTwoObjects(const std::string& id1, const std::string& id2); // Bidirectional link
    static void linkObjects(const std::vector<std::string>& ids); // Link all objects in list to each other
    static void linkPortals(const std::string& portalId1, const std::string& portalId2); // Helper for portal linking
    
private:
    GObject* owner_;
    std::string linkId_;
    std::vector<std::string> targetIds_;
    
    // Static registry: linkId -> GObject*
    static std::unordered_map<std::string, GObject*> objectRegistry_;
    
    void updateRegistry();
};