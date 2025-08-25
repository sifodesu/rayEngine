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
            if (ImGui::SliderFloat("Jump Speed", &jump, 100.0f, 300.0f, "%.0f")) {
                player->setDebugJumpSpeed(jump);
            }

            double mass = player->body_->getMass();
            float massF = static_cast<float>(mass);
            if (ImGui::SliderFloat("Gravity (mass)", &massF, 0.0f, 2000.0f, "%.0f")) {
                player->body_->setMass(massF);
            }
        
        } else {
            ImGui::TextUnformatted("Player not found");
        }
    }
    ImGui::End();
}

} // namespace ImGuiLayer
