#pragma once

namespace voxel {
// Requires a native OpenGL context. Quality changes must preserve cave
// occlusion, authored geometry, and renderer caches while reducing hub detail.
void verifyGraphicsQuality();
}
