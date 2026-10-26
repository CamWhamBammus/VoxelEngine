#include "audio.h"
#include "encounters.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#ifdef VOXEL_HAVE_OPENAL
#ifdef __APPLE__
// macOS still ships this optional cross-platform backend.
#define OPENAL_DEPRECATED
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#endif

namespace voxel {

struct EncounterAudio::Device {
#ifdef VOXEL_HAVE_OPENAL
    ALCdevice* output = nullptr;
    ALCcontext* context = nullptr;
    ALuint buffer = 0;
    std::array<ALuint,8> voices{};
    std::size_t nextVoice = 0;
    bool ready = false;

    Device()
    {
        output=alcOpenDevice(nullptr);
        if (!output) return;
        context=alcCreateContext(output,nullptr);
        if (!context) return;
        auto* previous=alcGetCurrentContext();
        if (!alcMakeContextCurrent(context)) return;
        // A short, soft two-note chime, generated once with a quick attack and
        // exponential decay. Mono sources provide ordinary 3D direction cues.
        constexpr int rate=22050;
        std::vector<short> samples(rate*7/10);
        for (std::size_t i=0; i<samples.size(); ++i) {
            const float t=float(i)/rate;
            float value=0;
            for (int note=0; note<2; ++note) {
                const float age=t-note*.12f;
                if (age<0) continue;
                const float frequency=note ? 587.33f : 392.0f;
                const float envelope=std::min(age/.008f,1.0f)*std::exp(-age*9.0f);
                const float phase=6.28318530718f*frequency*age;
                value+=(std::sin(phase)+.18f*std::sin(phase*2.76f))*envelope*.23f;
            }
            samples[i]=static_cast<short>(std::clamp(value,-1.0f,1.0f)*32767);
        }
        alGetError();
        alGenBuffers(1,&buffer);
        alBufferData(buffer,AL_FORMAT_MONO16,samples.data(),static_cast<ALsizei>(samples.size()*sizeof(short)),rate);
        alGenSources(static_cast<ALsizei>(voices.size()),voices.data());
        ready=alGetError()==AL_NO_ERROR;
        if (ready) {
            alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
            alDopplerFactor(0);
            for (const auto voice : voices) {
                alSourcei(voice,AL_BUFFER,static_cast<ALint>(buffer));
                alSourcef(voice,AL_REFERENCE_DISTANCE,6);
                alSourcef(voice,AL_MAX_DISTANCE,48);
                alSourcef(voice,AL_ROLLOFF_FACTOR,1.2f);
                alSourcef(voice,AL_GAIN,.55f);
            }
        }
        alcMakeContextCurrent(previous);
    }
    ~Device()
    {
        if (context) {
            auto* previous=alcGetCurrentContext();
            if (alcMakeContextCurrent(context)) {
                for (const auto voice : voices) if (voice) {
                    alSourceStop(voice);
                    alDeleteSources(1,&voice);
                }
                if (buffer) alDeleteBuffers(1,&buffer);
                alcMakeContextCurrent(previous==context ? nullptr : previous);
            }
            alcDestroyContext(context);
        }
        if (output) alcCloseDevice(output);
    }
    void play(std::span<const EncounterActivation> events, glm::vec3 listener, glm::vec3 forward)
    {
        if (!ready) return;
        auto* previous=alcGetCurrentContext();
        if (!alcMakeContextCurrent(context)) return;
        alListener3f(AL_POSITION,listener.x,listener.y,listener.z);
        const std::array<float,6> orientation{forward.x,forward.y,forward.z,0,1,0};
        alListenerfv(AL_ORIENTATION,orientation.data());
        for (const auto& event : events) {
            if (event.age>.25f || glm::distance(listener,event.position)>48) continue;
            const auto voice=voices[nextVoice++%voices.size()];
            alSourceStop(voice);
            alSource3f(voice,AL_POSITION,event.position.x,event.position.y+1,event.position.z);
            alSourcef(voice,AL_PITCH,event.chained ? 1.12f : 1.0f);
            alSourcePlay(voice);
        }
        alcMakeContextCurrent(previous);
    }
#else
    void play(std::span<const EncounterActivation>, glm::vec3, glm::vec3) {}
#endif
};

EncounterAudio::EncounterAudio() : device_(std::make_unique<Device>()) {}
EncounterAudio::~EncounterAudio() = default;

void EncounterAudio::update(const EncounterSession& encounters, glm::vec3 listener, glm::vec3 forward)
{
    const auto serial=encounters.activationSerial();
    const auto count=serial>=lastActivation_ ? serial-lastActivation_ : serial;
    lastActivation_=serial;
    const auto events=encounters.activations();
    const auto recent=std::min<std::uint64_t>(count,events.size());
    device_->play(events.last(static_cast<std::size_t>(recent)),listener,forward);
}

} // namespace voxel
