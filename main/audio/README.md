# Audio Service Architecture

`AudioService` owns one input engine and keeps codec I/O, Opus encoding/decoding,
and network queues independent from the chip-specific speech pipeline.

## Input engine

`AudioInputTask` reads PCM once and feeds `AfeAudioEngine`, which owns a single
FD AFE instance used for uplink voice processing (VAD, and AEC when device AEC is
enabled). There is no wake-word detection; listening is started by the button.

The AFE configuration currently uses `FD_LOW_COST` AEC with
`AEC_NLP_LEVEL_VERYAGGR`. Noise suppression is intentionally disabled because the
project does not ship an NSNet model. The AFE instance is created the first time
the device enters the idle state.

## Input data flow

### Board input layouts

Built-in boards default to one microphone: `M` without a playback reference, or
`MR` when the codec supplies a playback reference. `R` must contain playback
reference samples, not a second microphone. The channel declaration must match
the interleaved PCM returned by the codec.

Validate capture, playback, VAD, interruption, reconnect, and applicable AEC
modes on hardware before releasing an audio change.

```mermaid
flowchart LR
    Mic[Microphone] --> Codec[AudioCodec]
    Codec --> Input[AudioInputTask]
    Input --> Engine[One AudioEngine]
    Engine --> PCM[16 kHz mono PCM]
    PCM --> EncodeQueue[audio_encode_queue_]
    EncodeQueue --> Opus[OpusCodecTask]
    Opus --> SendQueue[audio_send_queue_]
    SendQueue --> App[Application / network]
```

AEC is active only during voice processing when device AEC is requested.

## Output data flow

```mermaid
flowchart LR
    App[Application / network] --> DecodeQueue[audio_decode_queue_]
    DecodeQueue --> Opus[OpusCodecTask]
    Opus --> PlaybackQueue[audio_playback_queue_]
    PlaybackQueue --> Output[AudioOutputTask]
    Output --> Codec[AudioCodec]
    Codec --> Speaker[Speaker]
```

## Tasks and power management

- `AudioInputTask` reads codec input and feeds the selected engine.
- `AudioOutputTask` drains decoded PCM to the codec output.
- `OpusCodecTask` encodes uplink PCM and decodes downlink packets.
- `AfeAudioEngine` has its own AFE fetch task.

The audio power timer still enables and disables codec ADC/DAC channels based on
activity; the engine refactor does not change that policy.

## Queue and allocation policy

The service queues use compile-time fixed-capacity storage. Their existing drop,
wait, and playback-drain policies remain unchanged, but queue growth no longer
allocates deque nodes. `AudioTask` values are stored directly, and Opus encoding
writes into the packet payload instead of allocating a second temporary output
buffer. Packet and PCM buffer pools should only be added after target-side heap
and latency measurements justify their additional ownership complexity.
