#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <memory>

namespace voxel {
class EncounterSession;

// Optional positional feedback. Missing audio support or a device leaves the
// game silent; encounter state and rendering never depend on sound playback.
class EncounterAudio {
public:
    EncounterAudio();
    ~EncounterAudio();
    EncounterAudio(const EncounterAudio&) = delete;
    EncounterAudio& operator=(const EncounterAudio&) = delete;
    void update(const EncounterSession& encounters, glm::vec3 listener, glm::vec3 forward);
private:
    struct Device;
    std::unique_ptr<Device> device_;
    std::uint64_t lastActivation_ = 0;
};
} // namespace voxel
