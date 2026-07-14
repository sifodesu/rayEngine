#pragma once
#include <filesystem>
#include "object_m.h"
#include "texture_m.h"

class Engine {
public:
    Engine(int argc = 0, char** argv = nullptr);
    void game_loop();
    ~Engine();

private:
    void loadGameContent();
    void unloadGameContent();
    void reloadGame();
    void render();
    void configureCaptureSuite(int argc, char** argv);
    void beginCaptureSuite();
    void beginCaptureStep();
    void queueCaptureForCurrentFrame();
    bool finishCaptureFrame();
    bool captureSuiteUsesCRT() const;
    void writeCaptureManifest(bool complete) const;

    bool captureSuiteEnabled_{false};
    std::filesystem::path captureSuiteDirectory_;
    int captureWidth_{3840};
    int captureHeight_{2160};
    int captureStepIndex_{-1};
    int captureAttempts_{0};
    int captureStepFrames_{0};
    double captureStepStartedAt_{0.0};
    bool captureQueued_{false};
    float captureOriginalTestPattern_{0.0f};
};
