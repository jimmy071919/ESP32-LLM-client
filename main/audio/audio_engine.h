#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include <model_path.h>

#include "audio_codec.h"

class AudioEngine {
public:
    virtual ~AudioEngine() = default;

    virtual bool Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) = 0;
    virtual void Feed(std::vector<int16_t>&& data) = 0;

    virtual void EnableVoiceProcessing(bool enable) = 0;
    virtual void EnableDeviceAec(bool enable) = 0;

    virtual bool IsVoiceProcessingEnabled() const = 0;
    virtual size_t GetFeedSize() const = 0;

    virtual void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) = 0;
    virtual void OnVadStateChange(std::function<void(bool speaking)> callback) = 0;
};

#endif
