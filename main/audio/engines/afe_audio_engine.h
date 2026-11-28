#ifndef AFE_AUDIO_ENGINE_H
#define AFE_AUDIO_ENGINE_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include <esp_afe_sr_models.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

#include "audio_engine.h"

class AfeAudioEngine : public AudioEngine {
public:
    AfeAudioEngine();
    ~AfeAudioEngine() override;

    bool Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) override;
    void Feed(std::vector<int16_t>&& data) override;

    void EnableVoiceProcessing(bool enable) override;
    void EnableDeviceAec(bool enable) override;

    bool IsVoiceProcessingEnabled() const override;
    size_t GetFeedSize() const override;

    void OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) override;
    void OnVadStateChange(std::function<void(bool speaking)> callback) override;

private:
    static constexpr EventBits_t kVoiceProcessingEnabled = 1 << 1;
    static constexpr EventBits_t kAfeActive = 1 << 2;
    static constexpr size_t kProcessingTaskStackSize = 4096;

    AudioCodec* codec_ = nullptr;
    srmodel_list_t* models_ = nullptr;
    bool owns_models_ = false;
    const esp_afe_sr_iface_t* afe_iface_ = nullptr;
    esp_afe_sr_data_t* afe_data_ = nullptr;
    EventGroupHandle_t event_group_ = nullptr;
    TaskHandle_t processing_task_ = nullptr;
    StaticTask_t* processing_task_buffer_ = nullptr;
    StackType_t* processing_task_stack_ = nullptr;
    int frame_samples_ = 0;
    bool is_speaking_ = false;
    std::atomic<bool> device_aec_enabled_{false};
    // Deferred AFE buffer reset, performed by ProcessingTask (see UpdateActiveState)
    std::atomic<bool> reset_pending_{false};
    // Deferred AEC toggles, applied by ProcessingTask (see ApplyAfeControls)
    std::atomic<bool> afe_control_dirty_{false};
    // Deferred output_buffer_ clear, performed by the output-producing task
    std::atomic<bool> output_reset_pending_{false};
    // Incremented whenever an active AFE session is invalidated. ProcessingTask
    // uses it to reject a fetch result produced before a disable/re-enable cycle.
    std::atomic<uint32_t> control_generation_{0};

    std::vector<int16_t> input_buffer_;
    std::vector<int16_t> output_buffer_;
    std::mutex input_buffer_mutex_;

    std::function<void(std::vector<int16_t>&&)> output_callback_;
    std::function<void(bool)> vad_state_change_callback_;

    void ProcessingTask();
    void UpdateActiveState();
    void UpdateAecState();
    void ApplyAfeControls();
    void ApplyPendingReset();
    void OutputRawAudio(const std::vector<int16_t>& data);
    void HandleVoiceResult(const afe_fetch_result_t* result);
};

#endif
