#pragma once

#include <string>

namespace voxel {

struct Options;

// Runs the optional hidden-window checks without mixing test logic into the
// interactive game loop. The caller reports any exception as a failed run.
int runGraphicsSmokeTest(const Options& options);

// Require a current OpenGL context. Throws on failures so smoke runs fail CI.
void verifyGpuResources();
void verifyRenderTransitions(int width, int height);
void verifyFrame(int width, int height, const std::string& screenshotPath);

} // namespace voxel
