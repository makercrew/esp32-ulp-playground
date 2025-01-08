#include "esp_log.h"
#include <thread>
#include <chrono>

constexpr const char *TAG = "main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 ULP Playground");
    while(true)
    {
        ESP_LOGI(TAG, "Main loop");
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}