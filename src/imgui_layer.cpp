// ImGui debug layer for runtime tuning
#include "imgui.h"
#include "imgui_stdlib.h"
#include "rlImGui.h"

#include <algorithm>
#include <cmath>
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

    ImGui::TextUnformatted("Hitachi CT-1358 / 240p natif 262 lignes / composite NTSC");
    ImGui::Text("Calibration: %s%s%s",
        params.calibrationStatus.c_str(),
        params.measuredSerial.empty() ? "" : " / serial ",
        params.measuredSerial.c_str());
    int testPattern = std::clamp((int)std::lround(params.testPattern), 0, 6);
    const char* testPatterns[] = {
        "Jeu", "Barres + PLUGE", "Convergence", "Multiburst",
        "Demi-ecran", "Zone plate", "Rampe niveaux"
    };
    if (ImGui::Combo("Mire", &testPattern, testPatterns, 7)) {
        params.testPattern = (float)testPattern;
    }
    ImGui::TextUnformatted("Signal: 262 total / 240 actives / source 320x192 en 5:3 / 60.05444 Hz");
    ImGui::SeparatorText("Decodeur composite sans comb filter");
    ImGui::SliderFloat("Source bande Y (Nyquist 3,04)",
        &params.ntscSourceLumaBandwidthMHz, 1.5f, 3.04f, "%.2f MHz");
    ImGui::SliderFloat("Source bande I", &params.ntscSourceIBandwidthMHz, 0.2f, 1.5f, "%.2f MHz");
    ImGui::SliderFloat("Source bande Q", &params.ntscSourceQBandwidthMHz, 0.2f, 0.8f, "%.2f MHz");
    ImGui::SliderFloat("Bande passante Y", &params.ntscLumaBandwidthMHz, 1.5f, 4.2f, "%.2f MHz");
    ImGui::SliderFloat("Bande passante I", &params.ntscChromaBandwidthIMHz, 0.2f, 1.5f, "%.2f MHz");
    ImGui::SliderFloat("Bande passante Q", &params.ntscChromaBandwidthQMHz, 0.2f, 1.5f, "%.2f MHz");
    ImGui::SliderFloat("Gain chroma", &params.ntscChromaGain, 0.0f, 1.8f, "%.2f");
    ImGui::SliderFloat("Retard chroma", &params.ntscChromaDelayNs, -250.0f, 250.0f, "%.0f ns");
    ImGui::SliderFloat("Suppression cross-color 2 lignes",
        &params.ntscLineCombStrength, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Peaking luminance", &params.ntscLumaPeaking, 0.0f, 0.35f, "%.3f");
    ImGui::SliderFloat("Gain differentiel", &params.ntscDifferentialGain, -0.20f, 0.20f, "%.3f");
    ImGui::SliderFloat("Phase differentielle", &params.ntscDifferentialPhaseDeg, -10.0f, 10.0f, "%.2f deg");
    ImGui::SliderFloat("Bruit composite", &params.ntscNoise, 0.0f, 0.03f, "%.4f");
    ImGui::SliderFloat("Ronflette secteur", &params.ntscHum, 0.0f, 0.02f, "%.4f");
    ImGui::SliderFloat("Constante AGC", &params.ntscAgcResponse, 0.010f, 0.500f, "%.3fs");
    ImGui::SliderFloat("Constante clamp", &params.ntscClampResponse, 0.001f, 0.050f, "%.3fs");
    ImGui::SliderFloat("Bande PLL burst", &params.ntscBurstPllBandwidthHz, 1.0f, 80.0f, "%.1f Hz");
    ImGui::SliderFloat("Bande PLL horizontale", &params.ntscHorizontalPllBandwidthHz, 10.0f, 500.0f, "%.0f Hz");
    ImGui::SliderFloat("Bande PLL verticale", &params.ntscVerticalPllBandwidthHz, 0.5f, 30.0f, "%.1f Hz");
    ImGui::SliderFloat("Constante ACC", &params.ntscAccResponse, 0.010f, 0.500f, "%.3fs");
    ImGui::SliderFloat("Seuil color killer", &params.ntscColorKillerThreshold, 0.0f, 0.8f, "%.2f");
    ImGui::SliderFloat3("Gain canons RGB", &params.videoGain.x, 0.75f, 1.25f, "%.3f");
    ImGui::SliderFloat3("Cutoff RGB", &params.videoCutoff.x, 0.0f, 0.08f, "%.3f");
    ImGui::SliderFloat3("Gamma canons RGB", &params.gunGamma.x, 1.8f, 2.8f, "%.2f");

    ImGui::SeparatorText("Canon electronique");
    ImGui::SliderFloat("Largeur faisceau min", &params.beamMinWidth, 0.08f, 0.8f, "%.3f");
    ImGui::SliderFloat("Largeur faisceau max", &params.beamMaxWidth, 0.10f, 1.0f, "%.3f");
    ImGui::SliderFloat("Forme faisceau", &params.beamShape, 1.0f, 5.0f, "%.2f");
    ImGui::SliderFloat("Poids intensite", &params.beamIntensityWeight, 0.05f, 1.5f, "%.2f");
    ImGui::SliderFloat("Force balayage", &params.beamScanlineStrength, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Flou horizontal", &params.beamHorizontalSigma, 0.15f, 2.5f, "%.2f px");
    ImGui::SliderFloat("Limite courant faisceau", &params.beamCurrentLimit, 0.20f, 2.0f, "%.2f");
    ImGui::SliderFloat("Compression courant", &params.beamCurrentCompression, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Bande ampli video", &params.videoOutputBandwidthMHz, 1.0f, 8.0f, "%.2f MHz");
    ImGui::SliderFloat("Marge cathodes", &params.cathodeDriveHeadroom, 0.5f, 2.0f, "%.2f");
    ImGui::SliderFloat("Charge d'espace", &params.spaceChargeCompression, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Bloom du spot", &params.spotBloom, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("Focus dynamique", &params.dynamicFocus, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Defocus aux bords", &params.focusEdgeSoftness, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("Astigmatisme", &params.astigmatism, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Defaut convergence", &params.misconvergence, 0.0f, 1.5f, "%.2f px");
    ImGui::SliderFloat("Jitter horizontal", &params.horizontalJitter, 0.0f, 1.0f, "%.2f px");

    ImGui::SeparatorText("Phosphores");
    ImGui::SliderFloat("Force masque", &params.maskStrength, 0.0f, 0.75f, "%.2f");
    ImGui::SliderFloat("Triades largeur tube", &params.maskTriadsAcross, 200.0f, 700.0f, "%.0f");
    int maskType = std::clamp((int)std::lround(params.maskType), 0, 2);
    const char* maskTypes[] = {"Aperture grille", "Slot mask", "Shadow mask"};
    if (ImGui::Combo("Disposition", &maskType, maskTypes, 3)) {
        params.maskType = (float)maskType;
    }
    ImGui::SliderFloat("Echauffement masque", &params.maskHeating, 0.0f, 0.30f, "%.3f");
    ImGui::SliderFloat("Inertie thermique masque", &params.maskThermalTau, 0.5f, 60.0f, "%.1fs");
    ImGui::SliderFloat("Diffusion thermique masque", &params.maskThermalDiffusion, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Doming masque", &params.maskDoming, 0.0f, 0.25f, "%.3f triade");
    ImGui::SliderFloat("Diaphotie masque", &params.maskCrosstalk, 0.0f, 0.15f, "%.3f");
    ImGui::SeparatorText("Phosphore");
    ImGui::SliderFloat3("Decay rapide RGB", &params.phosphorFastDecay.x, 0.00025f, 0.010f, "%.4fs");
    ImGui::SliderFloat3("Decay moyen RGB", &params.phosphorMediumDecay.x, 0.001f, 0.030f, "%.4fs");
    ImGui::SliderFloat3("Decay lent RGB", &params.phosphorSlowDecay.x, 0.002f, 0.080f, "%.4fs");
    ImGui::SliderFloat("Poids remanence moyenne", &params.phosphorMediumWeight, 0.0f, 0.30f, "%.3f");
    ImGui::SliderFloat("Poids remanence lente", &params.phosphorSlowWeight, 0.0f, 0.20f, "%.3f");
    ImGui::SliderFloat("Diffusion", &params.phosphorSpread, 0.0f, 2.5f, "%.2f px");
    ImGui::SliderFloat("Saturation phosphore", &params.phosphorSaturation, 0.25f, 8.0f, "%.2f");
    ImGui::SliderFloat("Integration oeil/camera", &params.observerIntegration, 0.0f, 0.050f, "%.3fs");

    ImGui::SeparatorText("Optique");
    ImGui::SliderFloat("Seuil bloom", &params.bloomThreshold, 0.0f, 2.0f, "%.2f");
    ImGui::SliderFloat("Bloom local", &params.bloomIntensity, 0.0f, 1.5f, "%.2f");
    ImGui::SliderFloat("Bloom large", &params.wideBloomIntensity, 0.0f, 1.0f, "%.3f");
    ImGui::SliderFloat("Rayon bloom", &params.bloomRadius, 0.0f, 4.0f, "%.2f");
    ImGui::SliderFloat3("Rayon bloom RGB", &params.bloomRadiusRGB.x, 0.5f, 1.8f, "%.2f");
    ImGui::SliderFloat("Halation", &params.halation, 0.0f, 0.5f, "%.3f");
    ImGui::SliderFloat("Reflet verre", &params.reflection, 0.0f, 0.25f, "%.3f");

    ImGui::SeparatorText("Tube et sortie");
    ImGui::SliderFloat("Courbure X", &params.curvatureX, 0.0f, 0.25f, "%.3f");
    ImGui::SliderFloat("Courbure Y", &params.curvatureY, 0.0f, 0.25f, "%.3f");
    ImGui::SliderFloat("Pincushion", &params.pincushion, 0.0f, 0.10f, "%.3f");
    ImGui::SliderFloat("Bloom haute tension", &params.highVoltageBloom, 0.0f, 0.08f, "%.3f");
    ImGui::SliderFloat("Reponse haute tension", &params.highVoltageResponse, 0.005f, 0.5f, "%.3fs");
    ImGui::SliderFloat("Chute haute tension", &params.highVoltageSag, 0.0f, 0.30f, "%.3f");
    ImGui::SliderFloat("Ripple haute tension", &params.highVoltageRipple, 0.0f, 0.02f, "%.4f");
    ImGui::SliderFloat("Reponse B+", &params.bPlusResponse, 0.002f, 0.25f, "%.3fs");
    ImGui::SliderFloat("Chute B+", &params.bPlusSag, 0.0f, 0.20f, "%.3f");
    ImGui::SliderFloat("Ripple B+", &params.bPlusRipple, 0.0f, 0.02f, "%.4f");
    ImGui::SliderFloat("Coins", &params.cornerRadius, 0.001f, 0.35f, "%.3f");
    ImGui::SliderFloat("Vignette", &params.vignette, 0.0f, 0.8f, "%.2f");
    ImGui::SliderFloat("Transmission verre", &params.glassTransmission, 0.3f, 1.0f, "%.2f");
    ImGui::SliderFloat3("Teinte verre RGB", &params.glassTint.x, 0.5f, 1.1f, "%.3f");
    ImGui::SliderFloat("Dispersion verre", &params.glassDispersion, 0.0f, 1.5f, "%.2f px");
    ImGui::SliderFloat("Indice verre", &params.glassRefractiveIndex, 1.35f, 1.75f, "%.3f");
    ImGui::SliderFloat("Epaisseur verre", &params.glassThicknessMm, 1.0f, 25.0f, "%.1f mm");
    ImGui::SliderFloat3("Absorption verre RGB", &params.glassAbsorption.x, 0.0f, 0.03f, "%.4f/mm");
    ImGui::SliderFloat("Reflexion interne", &params.internalReflection, 0.0f, 0.10f, "%.3f");
    ImGui::SliderFloat("Eclairement ambiant", &params.ambientIlluminance, 0.0f, 500.0f, "%.0f lux");
    ImGui::SliderFloat("Courbure faceplate X", &params.faceplateCurvatureX, 0.0f, 0.08f, "%.3f");
    ImGui::SliderFloat("Courbure faceplate Y", &params.faceplateCurvatureY, 0.0f, 0.08f, "%.3f");
    ImGui::SliderFloat3("Matrice tube R", &params.tubeColorMatrixR.x, -0.5f, 1.5f, "%.3f");
    ImGui::SliderFloat3("Matrice tube G", &params.tubeColorMatrixG.x, -0.5f, 1.5f, "%.3f");
    ImGui::SliderFloat3("Matrice tube B", &params.tubeColorMatrixB.x, -0.5f, 1.5f, "%.3f");
    ImGui::SliderFloat("Niveau noir", &params.blackLevel, 0.0f, 0.05f, "%.4f");
    ImGui::SliderFloat("Luminosite", &params.brightness, 0.25f, 2.5f, "%.2f");
    ImGui::SliderFloat("Saturation", &params.saturation, 0.0f, 2.5f, "%.2f");
    ImGui::SliderFloat("Gamma sortie", &params.outputGamma, 1.6f, 3.0f, "%.2f");
    ImGui::SliderFloat("Pic tube", &params.tubePeakNits, 40.0f, 250.0f, "%.0f nits");
    ImGui::SliderFloat("Pic ecran hote", &params.hostPeakNits, 80.0f, 1500.0f, "%.0f nits");
    ImGui::SliderFloat("Radiance blanc de reference",
        &params.referenceWhiteRadiance, 0.10f, 1.50f, "%.3f");
    ImGui::SliderFloat("Flicker", &params.flicker, 0.0f, 0.04f, "%.4f");
    ImGui::SliderFloat("Bruit", &params.noise, 0.0f, 0.04f, "%.4f");

    if (ImGui::Button("Screenshot sans overlays")) {
        Shader_m::requestScreenshot("manual");
    }
    if (!Shader_m::lastScreenshotPath().empty()) {
        ImGui::Text("Derniere capture: %s%s",
            Shader_m::lastScreenshotPath().string().c_str(),
            Shader_m::lastScreenshotSucceeded() ? "" : " (echec)");
    }

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
    drawCRTWindow();
    // Shader_m::resetCRTParams();
    // drawParticleWindow();
    // drawStillWaterWindow();
    // drawFogWindow();
    // drawLightingWindow();
}

} // namespace ImGuiLayer
