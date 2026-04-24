// ImGui debug layer for runtime tuning
#include "imgui.h"
#include "imgui_stdlib.h"
#include "rlImGui.h"

#include <algorithm>
#include <set>
#include <vector>

#include "Components/collisionRect.h"
#include "Managers/object_m.h" // adjust include path if differs in project
#include "Managers/raycam_m.h"
#include "Objects/character.h"
#include "Objects/modelEnt.h"
#include "Components/sprite.h"

namespace ImGuiLayer {

static Character* findPlayer() {
    for (auto & kv : Object_m::level_ents_) {
        if (auto *c = dynamic_cast<Character*>(kv.second.get())) return c;
    }
    return nullptr;
}

static std::vector<ModelEnt*> findVisibleModelEnts() {
    std::vector<ModelEnt*> models;
    std::set<int> seenIds;

    const Rectangle cameraRect = Raycam_m::getRayCam().getRect();
    for (CollisionRect* body : CollisionRect::query(cameraRect)) {
        if (!body) continue;

        auto* model = dynamic_cast<ModelEnt*>(body->getFather());
        if (!model || !seenIds.insert(model->id_).second) continue;

        models.push_back(model);
    }

    return models;
}

struct VisibleGlitchedObject {
    GObject* object = nullptr;
    int spriteCount = 0;
};

static std::vector<VisibleGlitchedObject> findVisibleGlitchedObjects() {
    std::vector<VisibleGlitchedObject> objects;
    std::set<int> seenIds;

    const Rectangle cameraRect = Raycam_m::getRayCam().getRect();
    for (CollisionRect* body : CollisionRect::query(cameraRect)) {
        if (!body) continue;

        GObject* object = body->getFather();
        if (!object || object->is3DRenderable() || !seenIds.insert(object->id_).second) continue;

        std::vector<Sprite*> sprites;
        object->collectDebugSprites(sprites);

        int glitchedCount = 0;
        for (Sprite* sprite : sprites) {
            if (sprite && sprite->isGlitched()) ++glitchedCount;
        }

        if (glitchedCount > 0) {
            objects.push_back({ object, glitchedCount });
        }
    }

    return objects;
}

static void drawVector3Control(const char* label, Vector3& value, float speed, float minValue, float maxValue) {
    float values[3] = { value.x, value.y, value.z };
    if (ImGui::DragFloat3(label, values, speed, minValue, maxValue, "%.2f")) {
        value = { values[0], values[1], values[2] };
    }
}

static void drawRotationControl(Vector3& rotation) {
    ImGui::DragFloat("Rotation X", &rotation.x, 0.5f, -360.0f, 360.0f, "%.1f");
    ImGui::DragFloat("Rotation Y", &rotation.y, 0.5f, -360.0f, 360.0f, "%.1f");
    ImGui::DragFloat("Rotation Z", &rotation.z, 0.5f, -360.0f, 360.0f, "%.1f");
}

static void drawSpinControl(Vector3& spin) {
    ImGui::DragFloat("Spin X", &spin.x, 1.0f, -720.0f, 720.0f, "%.1f");
    ImGui::DragFloat("Spin Y", &spin.y, 1.0f, -720.0f, 720.0f, "%.1f");
    ImGui::DragFloat("Spin Z", &spin.z, 1.0f, -720.0f, 720.0f, "%.1f");
}

static void drawColorControl(const char* label, Color& color) {
    float values[4] = {
        static_cast<float>(color.r) / 255.0f,
        static_cast<float>(color.g) / 255.0f,
        static_cast<float>(color.b) / 255.0f,
        static_cast<float>(color.a) / 255.0f
    };

    if (ImGui::ColorEdit4(label, values)) {
        color = {
            static_cast<unsigned char>(std::clamp(values[0], 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(values[1], 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(values[2], 0.0f, 1.0f) * 255.0f),
            static_cast<unsigned char>(std::clamp(values[3], 0.0f, 1.0f) * 255.0f)
        };
    }
}

static void drawVisibleModelWindow() {
    if (!ImGui::Begin("Objets 3D visibles")) {
        ImGui::End();
        return;
    }

    std::vector<ModelEnt*> models = findVisibleModelEnts();
    ImGui::Text("Camera actuelle: %d objet(s)", static_cast<int>(models.size()));
    ImGui::Separator();

    if (models.empty()) {
        ImGui::TextUnformatted("Aucun objet 3D visible.");
        ImGui::End();
        return;
    }

    for (ModelEnt* model : models) {
        ImGui::PushID(model->id_);
        const bool open = ImGui::TreeNode("model3d", "Model3D #%d - %s", model->id_, model->getDebugPrimitiveName());
        if (open) {
            if (CollisionRect* body = model->getCollisionBody()) {
                Rectangle surface = body->getSurface();
                float pos[2] = { surface.x, surface.y };
                bool solid = body->isSolid();

                if (ImGui::DragFloat2("Position", pos, 1.0f, -10000.0f, 10000.0f, "%.1f")) {
                    body->setCoord({ pos[0], pos[1] });
                }
                if (ImGui::Checkbox("Solide", &solid)) {
                    body->setSolid(solid);
                }
            }

            bool killOnCol = model->hasKillOnCollision();
            if (ImGui::Checkbox("Kill collision", &killOnCol)) {
                model->setKillOnCollision(killOnCol);
            }

            std::string& modelFile = model->debugModelFile();
            if (ImGui::InputText("Modele", &modelFile)) {
                model->refreshDebugModel();
            }

            static const char* primitives[] = { "Cube", "Sphere", "Cylinder", "Plane" };
            int primitiveIndex = model->getDebugPrimitiveIndex();
            if (ImGui::Combo("Primitive", &primitiveIndex, primitives, 4)) {
                model->setDebugPrimitiveIndex(primitiveIndex);
            }

            drawRotationControl(model->debugRotation());
            drawVector3Control("Echelle", model->debugScale(), 0.25f, 0.01f, 10000.0f);
            drawSpinControl(model->debugSpin());
            drawColorControl("Teinte", model->debugTint());
            ImGui::Checkbox("Afficher axes", &model->debugShowAxes());

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    ImGui::End();
}

static void drawGlitchSpriteWindow() {
    if (!ImGui::Begin("Glitch Sprites visibles")) {
        ImGui::End();
        return;
    }

    std::vector<VisibleGlitchedObject> objects = findVisibleGlitchedObjects();
    ImGui::Text("Camera actuelle: %d objet(s)", static_cast<int>(objects.size()));

    SpriteGlitchParams& params = Sprite::glitchParams();
    ImGui::Separator();
    ImGui::SliderFloat("Intensite", &params.intensity, 0.0f, 4.0f, "%.2f");
    ImGui::SliderFloat("Vitesse", &params.speed, 0.0f, 8.0f, "%.2f");
    ImGui::SliderFloat("Pixel shift", &params.pixelShift, 0.0f, 12.0f, "%.2f");
    ImGui::SliderFloat("RGB shift", &params.colorShift, 0.0f, 8.0f, "%.2f");
    ImGui::SliderFloat("Jitter orientation", &params.orientationJitter, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Flip blocs 8x8", &params.blockFlip, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Frequence bandes", &params.bandFrequency, 1.0f, 48.0f, "%.1f");
    ImGui::DragFloat("Seed", &params.seed, 0.1f, -10000.0f, 10000.0f, "%.1f");
    if (ImGui::Button("Reset glitch")) {
        Sprite::resetGlitchParams();
    }

    ImGui::Separator();
    if (objects.empty()) {
        ImGui::TextUnformatted("Aucun sprite glitched visible.");
        ImGui::End();
        return;
    }

    for (const VisibleGlitchedObject& item : objects) {
        ImGui::Text("Objet #%d - %d sprite(s)", item.object->id_, item.spriteCount);
    }

    ImGui::End();
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

    drawVisibleModelWindow();
    drawGlitchSpriteWindow();
}

} // namespace ImGuiLayer
