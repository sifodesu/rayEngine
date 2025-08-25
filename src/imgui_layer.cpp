// ImGui debug layer for runtime tuning
#include "imgui.h"
#include "rlImGui.h"

#include "Managers/object_m.h" // adjust include path if differs in project
#include "Objects/character.h"

namespace ImGuiLayer {

static Character* findPlayer() {
    for (auto & kv : Object_m::level_ents_) {
        if (auto *c = dynamic_cast<Character*>(kv.second.get())) return c;
    }
    return nullptr;
}

void BeginFrame() { rlImGuiBegin(); }
void EndFrame() { rlImGuiEnd(); }

void DrawWindows() {
    if (ImGui::Begin("Player Debug")) {
        if (auto *player = findPlayer()) {
            float jump = player->getDebugJumpSpeed();
            if (ImGui::SliderFloat("Jump Speed", &jump, 50.0f, 800.0f, "%.0f")) {
                player->setDebugJumpSpeed(jump);
            }
        } else {
            ImGui::TextUnformatted("Player not found");
        }
    }
    ImGui::End();
}

} // namespace ImGuiLayer
