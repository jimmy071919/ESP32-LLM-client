# AtomS3R + Echo Base 燒錄步驟（VS Code ESP-IDF 擴充）

> 所有個人設定（板子、旋轉 180°、繁中、喚醒詞）都存在 `sdkconfig`。
> 這個檔案 **不會推上 GitHub**，被洗掉就要重新設定，請照下面的順序操作。

---

## 0. 第一次使用前：備份設定（只要做一次）

在 VS Code 終端機執行：

```bash
cp sdkconfig ~/sdkconfig.atoms3r.backup
```

改過 SDK 設定（齒輪）之後，記得再備份一次。

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
4. 看到 `Startup wake word hint: 說「你好小智」就能喚醒我。` 就是成功
5. 按 `Ctrl+]` 離開 Monitor

### 分步驟

1. 🔧 **Build**：編譯（指令：`ESP-IDF: Build your Project`）
2. ⚡ **Flash**：燒錄（指令：`ESP-IDF: Flash your Project`）
3. 🖥️ **Monitor**：看 log（指令：`ESP-IDF: Monitor Device`），`Ctrl+]` 離開

> 燒錄會一起寫入韌體和資源（字型、喚醒模型），Wi-Fi 帳密會保留。

---

## 3. ❌ 禁止點擊

| 按鈕 / 指令 | 後果 |
|---|---|
| **Set Espressif Device Target**（狀態列的 `esp32s3`） | 刪掉 `sdkconfig` 並用預設值重建，**旋轉、繁中、板子選擇全部消失**。就算選一樣的 esp32s3 也會洗掉 |
| `python scripts/build.py ...` | 一樣會重建 `sdkconfig`，旋轉設定消失 |
| **Erase Flash Memory from Device** | 清空晶片，Wi-Fi 帳密和啟用狀態消失，要重新配網 |

## 4. ⚠️ 小心使用

| 按鈕 / 指令 | 說明 |
|---|---|
| **SDK Configuration Editor**（齒輪） | 可以用，但直接改 `sdkconfig`。改完記得重新備份 |
| **Full Clean**（垃圾桶） | 只刪 `build/`，設定保留。編譯怪怪的時候可以用，下次編譯會比較久 |

---

## 5. 設定被洗掉時的救援

```bash
cp ~/sdkconfig.atoms3r.backup sdkconfig
```

然後依序點：🗑️ **Full Clean** → 🔥 **Build, Flash and Monitor**。

---

## 6. 常見問題

| 狀況 | 處理 |
|---|---|
| 找不到裝置 / 連接埠錯誤 | 換 USB 線或孔，點狀態列連接埠重選 `/dev/ttyACM*` |
| 燒錄卡在 `Connecting...` | 長按 AtomS3R 側邊的 Reset 鍵約 2 秒（內部綠燈亮起）進入下載模式後重試 |
| Monitor 一直亂碼 | 先 `Ctrl+]` 離開，再重開 Monitor |
| 螢幕上下顛倒 | `sdkconfig` 被洗掉了，照第 5 步救援 |
