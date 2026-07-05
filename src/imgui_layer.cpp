// ImGui debug layer for runtime tuning
#include "imgui.h"
#include "imgui_stdlib.h"
#include "rlImGui.h"

#include <algorithm>
#include <set>
#include <vector>

#include "Components/collisionRect.h"
#include "Managers/object_m.h" // adjust include path if differs in project
#include "Managers/particle_m.h"
#include "Managers/light_m.h"
#include "Managers/raycam_m.h"
#include "Managers/shader_m.h"
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
    ImGui::SliderFloat("Inversion couleurs", &params.colorInvert, 0.0f, 1.0f, "%.2f");
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

static void drawCRTWindow() {
    static const char* saveStatus = "";

    if (!ImGui::Begin("CRT Debug")) {
        ImGui::End();
        return;
    }

    Shader_m::CRTParams& params = Shader_m::crtParams();
    ImGui::SliderFloat("Courbure", &params.curvature, 0.0f, 0.45f, "%.3f");
    ImGui::SliderFloat("Vignette", &params.vignette, 0.0f, 1.4f, "%.2f");
    ImGui::SliderFloat("Bord doux", &params.edgeSoftness, 0.0f, 0.18f, "%.3f");
    ImGui::SliderFloat("Glow", &params.glow, 0.0f, 2.4f, "%.2f");
    ImGui::SliderFloat("Dot mask", &params.dotMask, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Flou dots", &params.dotBlur, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Bleed", &params.bleed, 0.0f, 2.5f, "%.2f");
    ImGui::SliderFloat("Dot grid size", &params.dotGridSize, 0.5f, 8.0f, "%.2f");
    bool hexGrid = params.hexGrid >= 0.5f;
    if (ImGui::Checkbox("Hexagonal grid", &hexGrid)) {
        params.hexGrid = hexGrid ? 1.0f : 0.0f;
    }
    ImGui::SliderFloat("Decalage lignes", &params.alternateLineShift, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Scanlines", &params.scanline, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Aberration RGB", &params.chromaticAberration, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Luminosite", &params.brightness, 0.25f, 2.25f, "%.2f");
    ImGui::SeparatorText("CRTSim");
    ImGui::SliderFloat("Sharpness", &params.sharpness, 0.0f, 1.5f, "%.2f");
    ImGui::SeparatorText("Phosphore");
    ImGui::SliderFloat("Trainee phosphore", &params.phosphorTrail, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Rouge -> 10%", &params.phosphorDecayR, 0.0005f, 0.0800f, "%.4fs");
    ImGui::SliderFloat("Vert -> 10%", &params.phosphorDecayG, 0.0005f, 0.0800f, "%.4fs");
    ImGui::SliderFloat("Bleu -> 10%", &params.phosphorDecayB, 0.0005f, 0.0800f, "%.4fs");
    ImGui::SliderFloat("Diffusion phosphore", &params.phosphorSpread, 0.0f, 2.5f, "%.2f");
    ImGui::SliderFloat("Persistence", &params.persistence, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("NTSC artifacts", &params.ntscArtifacts, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Overscan", &params.overscan, 0.75f, 1.35f, "%.2f");
    ImGui::SliderFloat("Saturation", &params.saturation, 0.0f, 2.5f, "%.2f");
    ImGui::SliderFloat("Mask brightness", &params.maskBrightness, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Mask opacity", &params.maskOpacity, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Mask scale", &params.maskScale, 0.25f, 8.0f, "%.2f");
    ImGui::SliderFloat("Bloom intensity", &params.bloomIntensity, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Bloom spread", &params.bloomSpread, 0.0f, 0.12f, "%.3f");
    ImGui::SliderFloat("Bloom power", &params.bloomPower, 0.25f, 4.0f, "%.2f");

    if (ImGui::Button("Save parameters")) {
        saveStatus = Shader_m::saveCRTParams() ? "Saved crt_params.json" : "Save failed";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load parameters")) {
        saveStatus = Shader_m::loadCRTParams() ? "Loaded crt_params.json" : "Load failed";
    }
    if (saveStatus[0] != '\0') {
        ImGui::TextUnformatted(saveStatus);
    }

    if (ImGui::Button("Reset CRT")) {
        Shader_m::resetCRTParams();
        saveStatus = "";
    }

    ImGui::End();
}

static void drawParticleWindow()
{
  if (!ImGui::Begin("Particles Debug")) {
    ImGui::End();
    return;
  }

  Particle_m::Params& params = Particle_m::params();
  ImGui::Text("Particules actives: %d", Particle_m::activeCount());

  ImGui::Checkbox("Enabled", &params.enabled);
  ImGui::SameLine();
  if (ImGui::Button("Clear")) {
    Particle_m::clear();
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset")) {
    Particle_m::resetParams();
  }

  ImGui::SeparatorText("Global");
  ImGui::SliderFloat("Density", &params.density, 0.0f, 4.0f, "%.2f");
  ImGui::SliderFloat("Size", &params.sizeScale, 0.10f, 4.0f, "%.2f");
  ImGui::SliderFloat("Lifetime", &params.lifetimeScale, 0.05f, 4.0f, "%.2f");
  ImGui::SliderFloat("Dust speed", &params.dustSpeedScale, 0.0f, 4.0f, "%.2f");
  ImGui::SliderFloat("Water speed", &params.waterSpeedScale, 0.0f, 4.0f, "%.2f");
  ImGui::SliderFloat("Gravity scale", &params.gravityScale, 0.0f, 4.0f, "%.2f");

  ImGui::SeparatorText("Triggers");
  ImGui::SliderFloat("Land min fall speed", &params.landDustMinFallSpeed, 0.0f, 500.0f, "%.0f");
  ImGui::SliderFloat("Water min fall speed", &params.waterSplashMinFallSpeed, 0.0f, 500.0f, "%.0f");
  ImGui::SliderFloat("Still water grace", &params.stillWaterTouchGrace, 0.0f, 0.25f, "%.3fs");
  ImGui::SliderFloat("Splash cooldown", &params.waterSplashCooldown, 0.0f, 1.0f, "%.3fs");

  ImGui::SeparatorText("Counts");
  ImGui::SliderInt("Jump dust", &params.jumpDustCount, 0, 64);
  ImGui::SliderInt("Land base", &params.landDustBaseCount, 0, 64);
  ImGui::SliderInt("Land strength", &params.landDustStrengthCount, 0, 64);
  ImGui::SliderInt("Splash base", &params.splashBaseCount, 0, 80);
  ImGui::SliderInt("Splash strength", &params.splashStrengthCount, 0, 80);

  ImGui::SeparatorText("Colors");
  drawColorControl("Dust", params.dustColor);
  drawColorControl("Dust highlight", params.dustHighlightColor);
  drawColorControl("Water", params.waterColor);
  drawColorControl("Foam", params.foamColor);

  if (auto *player = findPlayer()) {
    ImGui::SeparatorText("Test");
    Rectangle body = player->getRect();
    GravityDirection gravityDir = player->body_ ? player->body_->getGravityDirection() : GravityDirection::DOWN;
    if (ImGui::Button("Emit jump dust")) {
      Particle_m::emitJumpDust(body, gravityDir);
    }
    ImGui::SameLine();
    if (ImGui::Button("Emit land dust")) {
      Particle_m::emitLandDust(body, gravityDir, 1.2f);
    }
    if (ImGui::Button("Emit splash")) {
      Particle_m::emitWaterSplash(body, body, gravityDir, 1.2f);
    }
  }

  ImGui::End();
}

static void drawStillWaterWindow()
{
  static const char* saveStatus = "";

  if (!ImGui::Begin("Water Debug")) {
    ImGui::End();
    return;
  }

  Shader_m::WaterParams& params = Shader_m::waterParams();
  if (ImGui::Button("Save parameters")) {
    saveStatus = Shader_m::saveWaterParams() ? "Saved water_params.json" : "Save failed";
  }
  ImGui::SameLine();
  if (ImGui::Button("Load parameters")) {
    saveStatus = Shader_m::loadWaterParams() ? "Loaded water_params.json" : "Load failed";
  }
  if (saveStatus[0] != '\0') {
    ImGui::TextUnformatted(saveStatus);
  }

  if (ImGui::Button("Reset water")) {
    Shader_m::resetWaterParams();
    saveStatus = "";
  }

  ImGui::SeparatorText("Still Water");
  ImGui::SliderFloat("Shift amplitude", &params.stillReflectionShiftAmplitude, 0.0f, 16.0f, "%.2f");
  ImGui::SliderFloat("Ripple slow scale", &params.stillRippleSlowScale, 0.0f, 3.0f, "%.3f");
  ImGui::SliderFloat("Ripple fast scale", &params.stillRippleFastScale, 0.0f, 4.0f, "%.3f");
  ImGui::SliderFloat("Ripple slow speed", &params.stillRippleSlowSpeed, -12.0f, 12.0f, "%.2f");
  ImGui::SliderFloat("Ripple fast speed", &params.stillRippleFastSpeed, -18.0f, 18.0f, "%.2f");
  ImGui::SliderFloat("Ripple slow weight", &params.stillRippleSlowWeight, -2.0f, 2.0f, "%.2f");
  ImGui::SliderFloat("Ripple fast weight", &params.stillRippleFastWeight, -2.0f, 2.0f, "%.2f");
  ImGui::SliderFloat("Mirror line offset", &params.stillReflectionLineOffset, 0.0f, 8.0f, "%.2f");

  ImGui::SeparatorText("Colors");
  drawColorControl("Occlusion", params.stillOcclusionColor);

  ImGui::SeparatorText("Waterfall");
  ImGui::SliderFloat("Column shift", &params.waterfallShiftAmplitude, 0.0f, 16.0f, "%.2f");
  ImGui::SliderFloat("Column segment height", &params.waterfallSegmentHeight, 1.0f, 32.0f, "%.2f");
  ImGui::SliderFloat("Fall speed", &params.waterfallFlowSpeed, 0.0f, 220.0f, "%.1f");
  ImGui::SliderFloat("Line spacing", &params.waterfallLineSpacing, 1.0f, 24.0f, "%.2f");
  ImGui::SliderFloat("Line width", &params.waterfallLineWidth, 0.5f, 6.0f, "%.2f");
  ImGui::SliderFloat("Line length", &params.waterfallLineLength, 1.0f, 48.0f, "%.2f");
  ImGui::SliderFloat("Line period", &params.waterfallLinePeriod, 2.0f, 80.0f, "%.2f");
  ImGui::SliderFloat("Line intensity", &params.waterfallLineIntensity, 0.0f, 1.0f, "%.2f");
  ImGui::DragFloat("Line seed", &params.waterfallLineRandomSeed, 0.1f, -1000.0f, 1000.0f, "%.1f");

  ImGui::End();
}

static void drawFogWindow()
{
  static const char* saveStatus = "";

  if (!ImGui::Begin("Fog Debug")) {
    ImGui::End();
    return;
  }

  Shader_m::FogParams& params = Shader_m::fogParams();
  if (ImGui::Button("Save parameters")) {
    saveStatus = Shader_m::saveFogParams() ? "Saved fog_params.json" : "Save failed";
  }
  ImGui::SameLine();
  if (ImGui::Button("Load parameters")) {
    saveStatus = Shader_m::loadFogParams() ? "Loaded fog_params.json" : "Load failed";
  }
  if (saveStatus[0] != '\0') {
    ImGui::TextUnformatted(saveStatus);
  }

  if (ImGui::Button("Reset fog")) {
    Shader_m::resetFogParams();
    saveStatus = "";
  }

  ImGui::SeparatorText("Shape");
  ImGui::SliderFloat("Max opacity", &params.opacity, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Cloud size", &params.scale, 0.001f, 0.02f, "%.4f");
  ImGui::SliderFloat("Cloud coverage", &params.contrast, 0.0f, 1.0f, "%.2f");
  ImGui::SliderFloat("Edge softness", &params.softness, 0.0f, 96.0f, "%.1f");

  ImGui::SeparatorText("Motion");
  ImGui::SliderFloat("Speed X", &params.speedX, -32.0f, 32.0f, "%.2f");
  ImGui::SliderFloat("Speed Y", &params.speedY, -32.0f, 32.0f, "%.2f");

  ImGui::SeparatorText("Color");
  drawColorControl("Fog color", params.color);

  ImGui::End();
}

static void drawLightingWindow()
{
  if (!ImGui::Begin("Lighting Debug")) {
    ImGui::End();
    return;
  }

  Light_m::Params& params = Light_m::params();
  ImGui::Text("Active lights: %d", Light_m::activeCount());
  ImGui::Checkbox("Enabled", &params.enabled);
  ImGui::SameLine();
  ImGui::Checkbox("Debug draw", &params.debugDraw);

  if (ImGui::Button("Reset lighting")) {
    Light_m::resetParams();
  }

  ImGui::SeparatorText("Post Process");
  ImGui::SliderFloat("Ambient", &params.ambient, 0.0f, 1.5f, "%.2f");
  ImGui::SliderFloat("Global intensity", &params.globalIntensity, 0.0f, 4.0f, "%.2f");
  ImGui::SliderFloat("Falloff", &params.falloff, 0.1f, 5.0f, "%.2f");
  ImGui::SeparatorText("Light Disks");
  ImGui::Checkbox("Concentric disks", &params.ditherEnabled);
  ImGui::SliderFloat("Disk blend", &params.ditherAmount, 0.0f, 1.0f, "%.2f");
  ImGui::SliderInt("Disk count", &params.ditherLevels, 1, 24);

  ImGui::End();
}

static void drawPlayerWindow()
{
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

      bool maxFall = player->body_->isMaxFallSpeedEnabled();
      if (ImGui::Checkbox("Max fall speed", &maxFall)) {
        player->body_->setMaxFallSpeedEnabled(maxFall);
      }

      float maxFallSpeed = static_cast<float>(player->body_->getMaxFallSpeed());
      if (ImGui::SliderFloat("Max fall speed value", &maxFallSpeed, 50.0f,
                             2000.0f, "%.0f")) {
        player->body_->setMaxFallSpeed(maxFallSpeed);
      }

    } else {
      ImGui::TextUnformatted("Player not found");
    }
  }
  ImGui::End();
}

void BeginFrame() { rlImGuiBegin(); }
void EndFrame() { rlImGuiEnd(); }


void DrawWindows() {
    // drawPlayerWindow();
    // drawVisibleModelWindow();
    // drawGlitchSpriteWindow();
    // drawCRTWindow();
    // Shader_m::resetCRTParams();
    // drawParticleWindow();
    // drawStillWaterWindow();
    // drawFogWindow();
    drawLightingWindow();
}

} // namespace ImGuiLayer
