#ifndef NOTIFY_CHANNEL_H
#define NOTIFY_CHANNEL_H

#include <web_socket.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>
#include <chrono>
#include <memory>

class NotifyChannel {
public:
    void Start();

private:
    std::shared_ptr<WebSocket> websocket_;
    TaskHandle_t task_handle_ = nullptr;
    std::atomic<int64_t> last_incoming_ms_{0};

    void Run();
    bool Connect();
    void Touch();
    bool IsStale() const;
};

#endif
