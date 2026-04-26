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
    ImGui::SliderFloat("Dot grid size", &params.dotGridSize, 0.1f, 1.0f, "%.2f");
    bool hexGrid = params.hexGrid >= 0.5f;
    if (ImGui::Checkbox("RGB phosphor grid", &hexGrid)) {
        params.hexGrid = hexGrid ? 1.0f : 0.0f;
    }
    ImGui::SliderFloat("Decalage lignes", &params.alternateLineShift, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Scanlines", &params.scanline, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Aberration RGB", &params.chromaticAberration, 0.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Luminosite", &params.brightness, 0.25f, 2.25f, "%.2f");
    ImGui::SeparatorText("True CRT");
    bool trueCRT = params.trueCRT >= 0.5f;
    if (ImGui::Checkbox("True CRT simulation", &trueCRT)) {
        params.trueCRT = trueCRT ? 1.0f : 0.0f;
    }
    const char* debugViews[] = {"Final", "Signal", "Beam", "Phosphor emission", "Afterglow"};
    int debugView = std::clamp(static_cast<int>(params.trueCRTDebugView), 0, 4);
    if (ImGui::Combo("Debug view", &debugView, debugViews, IM_ARRAYSIZE(debugViews))) {
        params.trueCRTDebugView = static_cast<float>(debugView);
    }
    ImGui::SeparatorText("Signal");
    const char* signalModes[] = {"RGB", "Component", "Composite"};
    int signalMode = std::clamp(static_cast<int>(params.signalMode), 0, 2);
    if (ImGui::Combo("Signal mode", &signalMode, signalModes, IM_ARRAYSIZE(signalModes))) {
        params.signalMode = static_cast<float>(signalMode);
    }
    ImGui::SliderFloat("Luma bandwidth", &params.lumaBandwidth, 0.05f, 1.0f, "%.2f");
    ImGui::SliderFloat("Chroma bandwidth", &params.chromaBandwidth, 0.05f, 1.0f, "%.2f");
    ImGui::SliderFloat("Chroma delay", &params.chromaDelay, -4.0f, 4.0f, "%.2f");
    ImGui::SliderFloat("NTSC phase", &params.ntscPhase, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Dot crawl", &params.dotCrawl, 0.0f, 1.0f, "%.2f");
    ImGui::SeparatorText("Beam");
    ImGui::SliderFloat("Beam width X", &params.beamWidthX, 0.0f, 4.0f, "%.2f");
    ImGui::SliderFloat("Beam width Y", &params.beamWidthY, 0.0f, 4.0f, "%.2f");
    ImGui::SliderFloat("Beam focus", &params.beamFocus, 0.1f, 2.0f, "%.2f");
    ImGui::SliderFloat("Beam bloom", &params.beamBloom, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Beam scanlines", &params.beamScanlineStrength, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Edge defocus", &params.edgeDefocus, 0.0f, 1.0f, "%.2f");
    drawVector3Control("Convergence R", params.convergenceR, 0.05f, -4.0f, 4.0f);
    drawVector3Control("Convergence G", params.convergenceG, 0.05f, -4.0f, 4.0f);
    drawVector3Control("Convergence B", params.convergenceB, 0.05f, -4.0f, 4.0f);
    ImGui::SeparatorText("Phosphors");
    const char* layouts[] = {"None", "RGB Dot Triad", "Aperture Grille", "Slot Mask", "Shadow Mask"};
    int layout = std::clamp(static_cast<int>(params.phosphorLayout), 0, 4);
    if (ImGui::Combo("Phosphor layout", &layout, layouts, IM_ARRAYSIZE(layouts))) {
        params.phosphorLayout = static_cast<float>(layout);
    }
    ImGui::SliderFloat("Phosphor pitch px", &params.phosphorPitchPx, 2.0f, 24.0f, "%.2f");
    ImGui::SliderFloat("Phosphor roundness", &params.phosphorRoundness, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Phosphor gap", &params.phosphorGap, 0.0f, 0.9f, "%.2f");
    ImGui::SliderFloat("Phosphor gain", &params.phosphorGain, 0.1f, 4.0f, "%.2f");
    ImGui::SliderFloat("Black matrix", &params.blackMatrix, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Phosphor bleed", &params.phosphorBleed, 0.0f, 2.0f, "%.2f");
    ImGui::SeparatorText("Persistence");
    float afterglowRgb[3] = { params.afterglowR, params.afterglowG, params.afterglowB };
    if (ImGui::SliderFloat3("Afterglow RGB", afterglowRgb, 0.0f, 0.5f, "%.3f")) {
        params.afterglowR = afterglowRgb[0];
        params.afterglowG = afterglowRgb[1];
        params.afterglowB = afterglowRgb[2];
    }
    ImGui::SliderFloat("Afterglow threshold", &params.afterglowThreshold, 0.0f, 0.25f, "%.3f");
    ImGui::SeparatorText("Optics");
    ImGui::SliderFloat("Glass diffusion", &params.glassDiffusion, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("Halation", &params.halation, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("Tube glow", &params.tubeGlow, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("White point", &params.whitePoint, 0.5f, 1.5f, "%.2f");
    ImGui::SliderFloat("Input gamma", &params.inputGamma, 1.0f, 3.0f, "%.2f");
    ImGui::SliderFloat("Output gamma", &params.outputGamma, 1.0f, 3.0f, "%.2f");
    ImGui::SeparatorText("CRTSim");
    ImGui::SliderFloat("Sharpness", &params.sharpness, 0.0f, 1.5f, "%.2f");
    float persistenceRgb[3] = { params.persistenceR, params.persistenceG, params.persistenceB };
    if (ImGui::SliderFloat3("Persistence RGB", persistenceRgb, 0.0f, 1.0f, "%.2f")) {
        params.persistenceR = persistenceRgb[0];
        params.persistenceG = persistenceRgb[1];
        params.persistenceB = persistenceRgb[2];
        params.persistence = std::max({params.persistenceR, params.persistenceG, params.persistenceB});
    }
    ImGui::SliderFloat("NTSC artifacts", &params.ntscArtifacts, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("NTSC alternance", &params.ntscAlternation, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Overscan", &params.overscan, 0.75f, 1.35f, "%.2f");
    ImGui::SliderFloat("Pixel ratio", &params.pixelRatio, 0.5f, 1.5f, "%.3f");
    ImGui::SliderFloat("Dimming", &params.dimming, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Saturation", &params.saturation, 0.0f, 2.5f, "%.2f");
    ImGui::SliderFloat("Mask brightness", &params.maskBrightness, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Mask opacity", &params.maskOpacity, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Mask scale", &params.maskScale, 0.25f, 8.0f, "%.2f");
    ImGui::SliderFloat("Bloom intensity", &params.bloomIntensity, 0.0f, 2.0f, "%.2f");
    if (ImGui::SliderFloat("Bloom down spread", &params.bloomDownsampleSpread, 0.0f, 0.12f, "%.3f")) {
        params.bloomSpread = params.bloomDownsampleSpread;
    }
    ImGui::SliderFloat("Bloom up spread", &params.bloomUpsampleSpread, 0.0f, 0.12f, "%.3f");
    ImGui::SliderFloat("Bloom power", &params.bloomPower, 0.25f, 4.0f, "%.2f");

    ImGui::SeparatorText("Frame");
    bool geometry3D = params.geometry3D >= 0.5f;
    if (ImGui::Checkbox("3D CRTSim geometry", &geometry3D)) {
        params.geometry3D = geometry3D ? 1.0f : 0.0f;
    }
    bool frameEnabled = params.frameEnabled >= 0.5f;
    if (ImGui::Checkbox("Frame enabled", &frameEnabled)) {
        params.frameEnabled = frameEnabled ? 1.0f : 0.0f;
    }
    ImGui::SliderFloat("Reflection", &params.reflectionScalar, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Diffuse", &params.diffuseBrightness, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Specular", &params.specBrightness, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Spec power", &params.specPower, 1.0f, 160.0f, "%.1f");
    ImGui::SliderFloat("Fresnel", &params.fresnelBrightness, 0.0f, 2.0f, "%.2f");
    drawVector3Control("Light pos", params.lightPos, 0.1f, -20.0f, 20.0f);
    drawColorControl("Frame color", params.frameColor);

    if (ImGui::Button("Save parameters")) {
        saveStatus = Shader_m::saveCRTParams() ? "Saved crt_params.json" : "Save failed";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load parameters")) {
        saveStatus = Shader_m::loadCRTParams() ? "Loaded crt_params.json" : "Load failed";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load CRTSim preset")) {
        Shader_m::loadCRTSimPreset();
        saveStatus = "Loaded CRTSim preset";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load True CRT preset")) {
        Shader_m::loadTrueCRTPreset();
        saveStatus = "Loaded True CRT preset";
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
    drawCRTWindow();
}

} // namespace ImGuiLayer
