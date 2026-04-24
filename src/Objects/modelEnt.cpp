#include "modelEnt.h"

#include <cmath>

#include "clock.h"
#include "definitions.h"
#include "model_m.h"
#include "rlgl.h"

int ModelEnt::instanceCount_ = 0;
Shader ModelEnt::litShader_ = {};
bool ModelEnt::litShaderLoaded_ = false;
int ModelEnt::lightDirLoc_ = -1;
int ModelEnt::ambientLoc_ = -1;

void ModelEnt::loadLitShader() {
    if (litShaderLoaded_) return;

    litShader_ = LoadShader((std::string(SHADERS_PATH) + "model_lit.vs").c_str(),
                            (std::string(SHADERS_PATH) + "model_lit.fs").c_str());
    litShaderLoaded_ = (litShader_.id != 0);
    if (!litShaderLoaded_) {
        TraceLog(LOG_WARNING, "ModelEnt: failed to load model_lit shader from %s", SHADERS_PATH);
        return;
    }

    lightDirLoc_ = GetShaderLocation(litShader_, "lightDir");
    ambientLoc_ = GetShaderLocation(litShader_, "ambient");
}

void ModelEnt::unloadLitShader() {
    if (!litShaderLoaded_) return;
    UnloadShader(litShader_);
    litShader_ = {};
    litShaderLoaded_ = false;
    lightDirLoc_ = -1;
    ambientLoc_ = -1;
}

void ModelEnt::applyLitShader(Model* model) {
    if (!model || !litShaderLoaded_) return;
    for (int i = 0; i < model->materialCount; ++i) {
        model->materials[i].shader = litShader_;
    }
}

ModelEnt::ModelEnt(const SpawnData& data) : GObject(data.id) {
    if (instanceCount_++ == 0) {
        loadLitShader();
    }

    CollisionDesc col = data.physics.collision.value_or(CollisionDesc{});
    body_ = new CollisionRect(col, this);

    scale_ = {1.0f, 1.0f, 1.0f};

    if (data.model.has_value()) {
        const ModelDesc& model = *data.model;
        primitive_ = model.primitive;
        rotation_ = model.rotation;
        scale_ = model.scale;
        spinSpeedDegPerSec_ = model.spin;
        tint_ = model.tint;

        if (!model.modelFile.empty()) {
            modelFile_ = model.modelFile;
            sharedModel_ = Model_m::getModel(modelFile_);
        }
    }

    if (!sharedModel_) {
        rebuildPrimitiveModel();
    }
}

ModelEnt::~ModelEnt() {
    delete body_;

    if (ownsPrimitiveModel_) {
        UnloadModel(primitiveModel_);
    }

    if (--instanceCount_ <= 0) {
        instanceCount_ = 0;
        unloadLitShader();
    }
}

Rectangle ModelEnt::getRect() {
    return body_ ? body_->getSurface() : Rectangle{0.0f, 0.0f, 0.0f, 0.0f};
}

const char* ModelEnt::getDebugPrimitiveName() const {
    switch (primitive_) {
        case ModelPrimitive::Sphere: return "Sphere";
        case ModelPrimitive::Cylinder: return "Cylinder";
        case ModelPrimitive::Plane: return "Plane";
        case ModelPrimitive::Cube:
        default:
            return "Cube";
    }
}

int ModelEnt::getDebugPrimitiveIndex() const {
    switch (primitive_) {
        case ModelPrimitive::Sphere: return 1;
        case ModelPrimitive::Cylinder: return 2;
        case ModelPrimitive::Plane: return 3;
        case ModelPrimitive::Cube:
        default:
            return 0;
    }
}

void ModelEnt::setDebugPrimitiveIndex(int index) {
    ModelPrimitive next = ModelPrimitive::Cube;
    switch (index) {
        case 1: next = ModelPrimitive::Sphere; break;
        case 2: next = ModelPrimitive::Cylinder; break;
        case 3: next = ModelPrimitive::Plane; break;
        case 0:
        default:
            next = ModelPrimitive::Cube;
            break;
    }

    if (next == primitive_) return;
    primitive_ = next;

    if (!sharedModel_) {
        rebuildPrimitiveModel();
    }
}

void ModelEnt::refreshDebugModel() {
    sharedModel_ = modelFile_.empty() ? nullptr : Model_m::getModel(modelFile_);
    if (sharedModel_ && ownsPrimitiveModel_) {
        UnloadModel(primitiveModel_);
        primitiveModel_ = {};
        ownsPrimitiveModel_ = false;
    }
    if (!sharedModel_ && !ownsPrimitiveModel_) {
        rebuildPrimitiveModel();
    }
}

void ModelEnt::rebuildPrimitiveModel() {
    if (ownsPrimitiveModel_) {
        UnloadModel(primitiveModel_);
        primitiveModel_ = {};
        ownsPrimitiveModel_ = false;
    }

    primitiveModel_ = makePrimitiveModel(primitive_);
    ownsPrimitiveModel_ = primitiveModel_.meshCount > 0;
}

Model* ModelEnt::getActiveModel() {
    if (sharedModel_) return sharedModel_;
    if (ownsPrimitiveModel_) return &primitiveModel_;
    return nullptr;
}

Vector3 ModelEnt::getWorldPosition() const {
    const Vector2 center2D = body_->getCenterCoord();
    return {
        center2D.x,
        0.0f,
        center2D.y
    };
}

Model ModelEnt::makePrimitiveModel(ModelPrimitive primitive) {
    switch (primitive) {
        case ModelPrimitive::Sphere:
            return LoadModelFromMesh(GenMeshSphere(0.5f, 16, 16));
        case ModelPrimitive::Cylinder:
            return LoadModelFromMesh(GenMeshCylinder(0.5f, 1.0f, 16));
        case ModelPrimitive::Plane:
            return LoadModelFromMesh(GenMeshPlane(1.0f, 1.0f, 1, 1));
        case ModelPrimitive::Cube:
        default:
            return LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    }
}

void ModelEnt::drawControlAxes() {
    constexpr float axisLength = 48.0f;

    DrawLine3D({0.0f, 0.0f, 0.0f}, {axisLength, 0.0f, 0.0f}, RED);
    DrawLine3D({0.0f, 0.0f, 0.0f}, {0.0f, axisLength, 0.0f}, GREEN);
    DrawLine3D({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, axisLength}, BLUE);
}

void ModelEnt::applyModelShader(Model* model) {
    if (!litShaderLoaded_) return;

    applyLitShader(model);
    if (lightDirLoc_ >= 0) {
        Vector3 lightDir = { -0.35f, -1.0f, -0.5f };
        SetShaderValue(litShader_, lightDirLoc_, &lightDir, SHADER_UNIFORM_VEC3);
    }
    if (ambientLoc_ >= 0) {
        float ambient = 0.30f;
        SetShaderValue(litShader_, ambientLoc_, &ambient, SHADER_UNIFORM_FLOAT);
    }
}

void ModelEnt::applyModelRotation() const {
    rlRotatef(rotation_.x + spinAngleDeg_.x, 1.0f, 0.0f, 0.0f);
    rlRotatef(rotation_.y + spinAngleDeg_.y, 0.0f, 1.0f, 0.0f);
    rlRotatef(rotation_.z + spinAngleDeg_.z, 0.0f, 0.0f, 1.0f);
}

void ModelEnt::routine() {
    if (spinSpeedDegPerSec_.x == 0.0f && spinSpeedDegPerSec_.y == 0.0f && spinSpeedDegPerSec_.z == 0.0f) return;

    const float dt = static_cast<float>(Clock::getLap());
    auto advance = [dt](float angle, float speed) {
        angle = std::fmod(angle + speed * dt, 360.0f);
        if (angle < 0.0f) angle += 360.0f;
        return angle;
    };

    spinAngleDeg_.x = advance(spinAngleDeg_.x, spinSpeedDegPerSec_.x);
    spinAngleDeg_.y = advance(spinAngleDeg_.y, spinSpeedDegPerSec_.y);
    spinAngleDeg_.z = advance(spinAngleDeg_.z, spinSpeedDegPerSec_.z);
}

void ModelEnt::draw3D() {
    Model* model = getActiveModel();
    if (!model || !body_) return;

    applyModelShader(model);
    const Vector3 position = getWorldPosition();

    rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        rlPushMatrix();
            applyModelRotation();
            rlScalef(scale_.x, scale_.y, scale_.z);
            DrawModel(*model, Vector3{0.0f, 0.0f, 0.0f}, 1.0f, tint_);
        rlPopMatrix();
        if (showAxes_) {
            drawControlAxes();
        }
    rlPopMatrix();
}
