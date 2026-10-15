#pragma once

#include <string>

namespace voxel {

// Require a current OpenGL context. Throws on failures so smoke runs fail CI.
void verifyGpuResources();
void verifyRenderTransitions(int width, int height);
void verifyFrame(int width, int height, const std::string& screenshotPath);

} // namespace voxel
