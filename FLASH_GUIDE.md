# AtomS3R + Echo Base 燒錄步驟（VS Code ESP-IDF 擴充）

> 板子的預設設定（晶片、8MB Flash、旋轉 180°、繁中、按鈕對話）都寫在 `sdkconfig.defaults`。
> `sdkconfig` 被洗掉時會自動用這些預設值重建，不需要另外備份。

---

## 1. 確認狀態列（底部藍色列）

| 項目 | 應該顯示 | 不對時 |
|---|---|---|
| ESP-IDF 版本 | `v6.1` | 點它選 v6.1 |
| 晶片 | `esp32s3` | ⚠️ **不要點**（見下方「禁止」） |
| 連接埠 | `/dev/ttyACM0` | 點它重新選 |
| 燒錄方式 | `UART` | 點它選 UART |

---

## 2. 燒錄順序

### 一鍵完成（最常用）

1. 用 USB 接上 AtomS3R
2. 點狀態列 🔥 **Build, Flash and Monitor**
3. 等編譯 → 燒錄 → 自動打開串口 log
4. 螢幕顯示「待命」後，按一下按鈕說話，有回應就是成功
5. 按 `Ctrl+]` 離開 Monitor

### 分步驟

1. 🔧 **Build**：編譯（指令：`ESP-IDF: Build your Project`）
2. ⚡ **Flash**：燒錄（指令：`ESP-IDF: Flash your Project`）
3. 🖥️ **Monitor**：看 log（指令：`ESP-IDF: Monitor Device`），`Ctrl+]` 離開

> 燒錄會一起寫入韌體和資源（字型、表情），Wi-Fi 帳密會保留。

---

## 3. ⚠️ 注意事項

| 按鈕 / 指令 | 說明 |
|---|---|
| **Set Espressif Device Target** | 只能選 `esp32s3`；會用 `sdkconfig.defaults` 重建設定，你在齒輪裡做的額外修改會消失 |
| **SDK Configuration Editor**（齒輪） | 直接改 `sdkconfig`；想永久保留的設定請寫進 `sdkconfig.defaults` |
| **Erase Flash Memory from Device** | 清空晶片，Wi-Fi 帳密和啟用狀態消失，要重新配網 |
| **Full Clean**（垃圾桶） | 只刪 `build/`，設定保留。編譯怪怪的時候可以用 |

---

## 4. 設定跑掉時的救援

```bash
rm sdkconfig
```

然後依序點：🗑️ **Full Clean** → 🔥 **Build, Flash and Monitor**。

---

## 5. 常見問題

| 狀況 | 處理 |
|---|---|
| 找不到裝置 / 連接埠錯誤 | 換 USB 線或孔，點狀態列連接埠重選 `/dev/ttyACM*` |
| 燒錄卡在 `Connecting...` | 長按 AtomS3R 側邊的 Reset 鍵約 2 秒（內部綠燈亮起）進入下載模式後重試 |
| Monitor 一直亂碼 | 先 `Ctrl+]` 離開，再重開 Monitor |
| 螢幕上下顛倒 | 照第 4 步救援，或在齒輪中開啟 `Rotate AtomS3R display 180 degrees` |
