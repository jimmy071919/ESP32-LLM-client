# XiaoZhi 韌體（AtomS3R + Echo Base 精簡版）

本 repo 由 [78/xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) fork 而來，已精簡為**只支援 M5Stack AtomS3R + Echo Base（ESP32-S3、8MB Flash、8MB Octal PSRAM）**。

- 網路：Wi-Fi（熱點配網）
- 傳輸協定：[WebSocket](docs/websocket.md) 與 [MQTT + UDP](docs/mqtt-udp.md)（由 OTA 伺服器下發決定）
- 語音：ES8311 codec、ESP-SR AFE（AEC/VAD），喚醒詞關閉
- 對話方式（`BUTTON_ONLY_CONVERSATION`）：
  - 待機時按一下按鈕開始收音，講完自動偵測結束並送出
  - 收音中按一下：立即結束收音並送出
  - 回答中按一下：打斷回答並開始收音（只有按鈕能打斷，語音不會打斷）
  - 回答完回到待機，但連線（session）保持；再按一下即可接著聊
  - 待機或收音超過 `CONVERSATION_SESSION_TIMEOUT_SECONDS`（預設 120 秒）沒有動作才關閉 session
- 顯示：128×128 LCD（GC9107 / ST7735 自動偵測）、機器人臉表情
- 裝置端 MCP 工具：[docs/mcp-usage.md](docs/mcp-usage.md)、[docs/mcp-protocol.md](docs/mcp-protocol.md)
- 介面語言：繁體中文（預設）、英文

## 建置與燒錄

需要 ESP-IDF v6.1（最低 v6.0.1）。

```bash
source ~/.espressif/tools/activate_idf_v6.1.sh
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

所有預設設定（晶片、Flash 大小、分割表、繁中、螢幕旋轉 180°、按鈕對話）都在 `sdkconfig.defaults`，刪掉 `sdkconfig` 後重新建置即可恢復。個人化調整用 `idf.py menuconfig` → `Xiaozhi Assistant`。

VS Code 操作步驟見 [FLASH_GUIDE.md](FLASH_GUIDE.md)。

Echo Base 正常運作時請由底座的 USB-C 供電；AtomS3R 本體的 USB-C 主要用於燒錄。

## 架構

| 路徑 | 職責 |
|---|---|
| `main/application.*` | 主事件迴圈、協定生命週期 |
| `main/device_state_machine.*` | 裝置狀態轉換 |
| `main/audio/` | ES8311 codec、AFE 音訊引擎、喚醒詞、Opus 編解碼佇列 |
| `main/protocols/` | 傳輸無關 API + WebSocket / MQTT-UDP 實作 |
| `main/display/` | LVGL LCD 顯示、字型、表情、Markdown、簡轉繁 |
| `main/boards/common/` | `Board` 介面、Wi-Fi 板、按鍵、背光、I2C |
| `main/boards/m5stack/atoms3r-echo-base/` | 本板腳位、LCD 偵測、機器人臉 |
| `main/mcp_server.*` | 裝置端 MCP 工具 |
| `main/assets/` | 語系字串與提示音 |
| `scripts/` | 建置時使用的 `gen_lang.py`、`build_default_assets.py`，以及主機端測試 |

## 授權

MIT，見 [LICENSE](LICENSE)。
