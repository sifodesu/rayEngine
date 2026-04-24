#pragma once

#include "collisionRect.h"
#include "gObject.h"
#include "spawn.h"

class ModelEnt : public GObject {
public:
    explicit ModelEnt(const SpawnData& data);
    ~ModelEnt() override;

    void routine() override;
    void draw3D() override;
    bool is3DRenderable() const override { return true; }
    Rectangle getRect() override;
    CollisionRect* getCollisionBody() override { return body_; }

    const char* getDebugPrimitiveName() const;
    int getDebugPrimitiveIndex() const;
    void setDebugPrimitiveIndex(int index);
    std::string& debugModelFile() { return modelFile_; }
    void refreshDebugModel();
    Vector3& debugRotation() { return rotation_; }
    Vector3& debugScale() { return scale_; }
    Vector3& debugSpin() { return spinSpeedDegPerSec_; }
    Color& debugTint() { return tint_; }
    bool& debugShowAxes() { return showAxes_; }

private:
    static Model makePrimitiveModel(ModelPrimitive primitive);
    static void drawControlAxes();
    Model* getActiveModel();
    Vector3 getWorldPosition() const;
    void rebuildPrimitiveModel();
    void applyModelShader(Model* model);
    void applyModelRotation() const;
    static void loadLitShader();
    static void unloadLitShader();
    static void applyLitShader(Model* model);

    CollisionRect* body_ = nullptr;

    Model* sharedModel_ = nullptr; // owned by Model_m
    std::string modelFile_;
    Model primitiveModel_{};
    bool ownsPrimitiveModel_ = false;

    ModelPrimitive primitive_ = ModelPrimitive::Cube;
    Vector3 rotation_{0.0f, 0.0f, 0.0f}; // degrees (XYZ Euler)
    Vector3 scale_{1.0f, 1.0f, 1.0f};
    Vector3 spinSpeedDegPerSec_{0.0f, 0.0f, 0.0f};
    Vector3 spinAngleDeg_{0.0f, 0.0f, 0.0f};
    Color tint_{WHITE};
    bool showAxes_ = false;

    static int instanceCount_;
    static Shader litShader_;
    static bool litShaderLoaded_;
    static int lightDirLoc_;
    static int ambientLoc_;
};
