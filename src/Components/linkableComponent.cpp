#include "linkableComponent.h"
#include "gObject.h"
#include <algorithm>

// Static registry
std::unordered_map<std::string, GObject*> LinkableComponent::objectRegistry_;

LinkableComponent::LinkableComponent(GObject* owner, const std::string& linkId)
    : owner_(owner), linkId_(linkId) {
    if (!linkId_.empty() && owner_) {
        updateRegistry();
    }
}

LinkableComponent::~LinkableComponent() {
    if (!linkId_.empty()) {
        unregisterObject(linkId_);
    }
}

void LinkableComponent::setLinkId(const std::string& id) {
    // Unregister old ID
    if (!linkId_.empty()) {
        unregisterObject(linkId_);
    }
    
    linkId_ = id;
    
    // Register new ID
    if (!linkId_.empty() && owner_) {
        updateRegistry();
    }
}

void LinkableComponent::addTargetId(const std::string& targetId) {
    if (std::find(targetIds_.begin(), targetIds_.end(), targetId) == targetIds_.end()) {
        targetIds_.push_back(targetId);
    }
}

void LinkableComponent::removeTargetId(const std::string& targetId) {
    targetIds_.erase(std::remove(targetIds_.begin(), targetIds_.end(), targetId), targetIds_.end());
}

void LinkableComponent::clearTargets() {
    targetIds_.clear();
}

std::vector<GObject*> LinkableComponent::getLinkedObjects() const {
    std::vector<GObject*> objects;
    for (const std::string& targetId : targetIds_) {
        GObject* obj = getObjectById(targetId);
        if (obj) {
            objects.push_back(obj);
        }
    }
    return objects;
}

GObject* LinkableComponent::getLinkedObject(const std::string& targetId) const {
    return getObjectById(targetId);
}

void LinkableComponent::triggerTargets(const std::string& message, void* data) {
    for (const std::string& targetId : targetIds_) {
        GObject* target = getObjectById(targetId);
        if (target) {
            // Try to get the linkable component from the target
            if (auto linkableOpt = target->getLinkableComponent()) {
                LinkableComponent* linkable = *linkableOpt;
                linkable->onTriggered(linkId_, message, data);
            }
        }
    }
}

void LinkableComponent::onTriggered(const std::string& sourceId, const std::string& message, void* data) {
    if (onTriggerReceived) {
        onTriggerReceived(sourceId, message, data);
    }
}

// Static methods
void LinkableComponent::registerObject(const std::string& id, GObject* object) {
    if (object && !id.empty()) {
        objectRegistry_[id] = object;
    }
}

void LinkableComponent::unregisterObject(const std::string& id) {
    objectRegistry_.erase(id);
}

GObject* LinkableComponent::getObjectById(const std::string& id) {
    auto it = objectRegistry_.find(id);
    return (it != objectRegistry_.end()) ? it->second : nullptr;
}

void LinkableComponent::clearRegistry() {
    objectRegistry_.clear();
}

void LinkableComponent::linkTwoObjects(const std::string& id1, const std::string& id2) {
    GObject* obj1 = getObjectById(id1);
    GObject* obj2 = getObjectById(id2);
    
    if (obj1 && obj2) {
        if (auto linkable1Opt = obj1->getLinkableComponent()) {
            (*linkable1Opt)->addTargetId(id2);
        }
        if (auto linkable2Opt = obj2->getLinkableComponent()) {
            (*linkable2Opt)->addTargetId(id1);
        }
    }
}

void LinkableComponent::linkObjects(const std::vector<std::string>& ids) {
    // Link all objects to each other
    for (size_t i = 0; i < ids.size(); ++i) {
        for (size_t j = i + 1; j < ids.size(); ++j) {
            linkTwoObjects(ids[i], ids[j]);
        }
    }
}

void LinkableComponent::linkPortals(const std::string& portalId1, const std::string& portalId2) {
    // Simple wrapper for portal linking - same as linkTwoObjects but with clearer intent
    linkTwoObjects(portalId1, portalId2);
}

void LinkableComponent::updateRegistry() {
    registerObject(linkId_, owner_);
}