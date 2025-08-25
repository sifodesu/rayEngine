#pragma once

// Public interface for the engine's ImGui overlay layer
// Add additional debug window functions here as needed.
namespace ImGuiLayer {
    // Begin a new ImGui frame (must be called after BeginDrawing and before any ImGui widgets)
    void BeginFrame();
    // Draw all registered ImGui windows/tool panels
    void DrawWindows();
    // Finish the ImGui frame and render draw data
    void EndFrame();
}
