#include "esp_log.h"
#include <thread>
#include <chrono>
#include "ulp_main.h"
#include "ulp_riscv.h"
#include "esp_sleep.h"

constexpr const char *TAG = "main";

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

static void init_ulp_program(void)
{
    // Load the binary into RTC SLOW RAM. This call also has the side affect of 
    // zero-initializing our ULP .bss section containing unintialized variables.
    esp_err_t err = ulp_riscv_load_binary(ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start));
    ESP_ERROR_CHECK(err);

    // Set the ULP wakeup period to 2 seconds. Once our ULP application finishes
    // it will be automatically restarted 2 seconds later.
    ulp_set_wakeup_period(0, 2 * 1000 * 1000);

    // Begin execution of our ULP application
    err = ulp_riscv_run();
    ESP_ERROR_CHECK(err);
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 ULP Playground");
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "Wake up cause: %d", cause);

    if (cause == ESP_SLEEP_WAKEUP_TIMER){
        ESP_LOGI(TAG, "Woke up from deep sleep timer");
    }
    else if(cause == ESP_SLEEP_WAKEUP_ULP){
        ESP_LOGI(TAG, "Woken up by ULP co-processor");
    }
    else{
        ESP_LOGI(TAG, "Normal boot, starting ULP program");
        init_ulp_program();
    }

    ESP_LOGI(TAG, "ULP Loop Count: %d", (int)ulp_loop_count);

    // Set the wakeup timer to 5 seconds and go to sleep
    esp_sleep_enable_timer_wakeup(20 * 1'000 * 1'000);
    esp_sleep_enable_ulp_wakeup();
    esp_deep_sleep_start();
}