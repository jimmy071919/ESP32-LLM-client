#include "notify_channel.h"

#include "application.h"
#include "board.h"
#include "settings.h"
#include "system_info.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <cJSON.h>

#include <algorithm>
#include <cstring>
#include <string>

#define TAG "NotifyChannel"

namespace {
constexpr int kInitialRetrySeconds = 5;
constexpr int kMaxRetrySeconds = 300;
constexpr int kCheckIntervalMs = 5000;
constexpr int64_t kStaleTimeoutMs = 120000;
constexpr uint32_t kTaskStackSize = 8192;
constexpr int kConnectId = 2;
}  // namespace

void NotifyChannel::Start() {
    if (task_handle_ != nullptr) {
        return;
    }
    xTaskCreate(
        [](void* arg) {
            static_cast<NotifyChannel*>(arg)->Run();
            vTaskDelete(nullptr);
        },
        "notify_channel", kTaskStackSize, this, 2, &task_handle_);
}

void NotifyChannel::Touch() { last_incoming_ms_ = esp_timer_get_time() / 1000; }

bool NotifyChannel::IsStale() const {
    return esp_timer_get_time() / 1000 - last_incoming_ms_.load() > kStaleTimeoutMs;
}

void NotifyChannel::Run() {
    int retry_seconds = kInitialRetrySeconds;
    while (true) {
        if (websocket_ && websocket_->IsConnected() && !IsStale()) {
            vTaskDelay(pdMS_TO_TICKS(kCheckIntervalMs));
            continue;
        }
        if (websocket_) {
            ESP_LOGW(TAG, "Notify channel lost, reconnecting");
            websocket_.reset();
        }
        if (Connect()) {
            retry_seconds = kInitialRetrySeconds;
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(retry_seconds * 1000));
        retry_seconds = std::min(retry_seconds * 2, kMaxRetrySeconds);
    }
}

bool NotifyChannel::Connect() {
    Settings settings("websocket", false);
    std::string url = settings.GetString("url");
    std::string token = settings.GetString("token");
    int version = settings.GetInt("version");
    if (url.empty()) {
        return false;
    }
    url += url.find('?') == std::string::npos ? "?" : "&";
    url += "channel=notify";

    auto network = Board::GetInstance().GetNetwork();
    std::shared_ptr<WebSocket> websocket = network->CreateWebSocket(kConnectId);
    if (!websocket) {
        return false;
    }
    if (!token.empty()) {
        if (token.find(' ') == std::string::npos) {
            token = "Bearer " + token;
        }
        websocket->SetHeader("Authorization", token.c_str());
    }
    websocket->SetHeader("Protocol-Version", std::to_string(version != 0 ? version : 1).c_str());
    websocket->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
    websocket->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());

    websocket->OnData([this](const char* data, size_t len, bool binary) {
        Touch();
        if (binary) {
            return;
        }
        auto root = cJSON_ParseWithLength(data, len);
        if (root == nullptr) {
            return;
        }
        auto type = cJSON_GetObjectItem(root, "type");
        if (cJSON_IsString(type) && strcmp(type->valuestring, "notify") == 0) {
            Application::GetInstance().HandleNotifyMessage(root);
        }
        cJSON_Delete(root);
    });

    std::weak_ptr<WebSocket> weak_websocket = websocket;
    websocket->OnPing([this, weak_websocket](const char* data, size_t len) {
        Touch();
        Application::GetInstance().Schedule([weak_websocket, payload = std::string(data, len)]() {
            if (auto ws = weak_websocket.lock()) {
                ws->SendPong(payload.data(), payload.size());
            }
        });
    });

    if (auto connected = websocket->Connect(url.c_str()); !connected) {
        ESP_LOGW(TAG, "Failed to connect notify channel: %s", connected.error().ToString().c_str());
        return false;
    }
    Touch();
    websocket_ = websocket;
    ESP_LOGI(TAG, "Notify channel connected");
    return true;
}
