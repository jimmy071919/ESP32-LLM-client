# AGENTS.md

## Project

XiaoZhi voice-assistant firmware (ESP-IDF C/C++), trimmed from upstream 78/xiaozhi-esp32 to support a single board: M5Stack AtomS3R + Echo Base (ESP32-S3, 8MB flash, 8MB octal PSRAM, ES8311, 128x128 LCD, Wi-Fi only).

Use ESP-IDF v6.1 when possible. The minimum supported SDK is ESP-IDF v6.0.1.

## Architecture

- `main/application.*`: main event loop, protocol lifecycle, and high-level behavior.
- `main/device_state_machine.*`: legal runtime state transitions.
- `main/boards/common/`: `Board` interface, `WifiBoard`, button, backlight, I2C helpers.
- `main/boards/m5stack/atoms3r-echo-base/`: board pins, LCD detection, robot face display.
- `main/audio/`: ES8311 codec, AFE audio engine, wake words, and queues.
- `main/protocols/`: transport-neutral API plus WebSocket and MQTT/UDP.
- `main/display/`: LVGL LCD display, fonts, emoji, markdown, Simplified-to-Traditional conversion.
- `main/mcp_server.*`: device-side MCP tools and dispatch.
- `main/Kconfig.projbuild`: feature configuration.
- `main/CMakeLists.txt`: sources, locale, font, and asset generation.
- `sdkconfig.defaults`: complete default configuration for the device.

## Required Rules

- Preserve unrelated worktree changes and keep patches focused.
- Core code depends on `Board` interfaces, never the concrete board class or board `config.h`.
- Change runtime state through `Application::SetDeviceState()` and the state machine.
- Callbacks may run outside the main task. Schedule application mutations with `Application::Schedule()` or event bits.
- Do not block the main event loop or audio tasks. Avoid unbounded queues and repeated large allocations in audio paths.
- Keep shared message semantics in `Protocol`; verify both transports when changing its contract.
- Validate network input and preserve `cJSON` ownership. NVS keys are persistent API and require migration when changed.
- Do not manually edit generated/vendor output: `build/`, `managed_components/`, `sdkconfig`, `main/assets/lang_config.h`.
- Format only touched C/C++ files with the repository `.clang-format`.

## Commands

```sh
source ~/.espressif/tools/activate_idf_v6.1.sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor

python3 -m unittest discover -s scripts/tests -v

clang-format -i <files>
```

## Validation

- Run host tests and `idf.py build` after core, audio, protocol, display, Kconfig, or CMake changes.
- Audio changes: verify capture, playback, wake word/VAD, interruption, and reconnect on hardware.
- Always report what was tested and what still needs physical hardware. A successful build is not hardware validation.

## Documentation

- Overview: `README.md`, flashing: `FLASH_GUIDE.md`
- Audio design: `main/audio/README.md`
- Code style: `docs/code_style.md`
- Protocols: `docs/websocket.md`, `docs/mqtt-udp.md`, `docs/mcp-protocol.md`
