#pragma once

#include "hub_assets.h"

#include <memory>
#include <optional>

namespace voxel {

struct AssetSurfaceHit {
    glm::vec3 point;
    glm::vec3 normal;
    float distance;
};

// Collision uses the same triangles as the visible models, including their
// rotation and variation. Build once when placements change, then query the
// spatial index during play. Copies share the immutable collision geometry.
class AssetCollision {
public:
    void rebuild(std::span<const HubAsset> assets);
    void clear();
    bool empty() const;

    // Touching is allowed. Closed model interiors also count as occupied.
    bool overlaps(glm::vec3 minimum, glm::vec3 maximum) const;
    // Return the signed distance available before the first surface contact.
    // Continuous sweeps cannot tunnel through thin models or large frame steps.
    float sweep(glm::vec3 minimum, glm::vec3 maximum, int axis, float distance) const;
    // Direction need not be normalized. Normals face back toward the ray.
    std::optional<AssetSurfaceHit> raycast(glm::vec3 origin, glm::vec3 direction,
                                         float maxDistance) const;

private:
    struct Data;
    std::shared_ptr<const Data> data_;
};

} // namespace voxel
