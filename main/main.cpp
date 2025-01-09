#include "esp_log.h"
#include <thread>
#include <chrono>
#include "ulp_main.h"
#include "ulp_riscv.h"

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
    init_ulp_program();

    while(true)
    {
        ESP_LOGI(TAG, "ULP Loop Count: %d", (int)ulp_loop_count);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}