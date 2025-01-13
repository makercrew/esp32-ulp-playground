#include "esp_log.h"
#include <thread>
#include <chrono>
#include "ulp_main.h"
#include "ulp_riscv.h"
#include "esp_sleep.h"
#include "ulp/sensor.h"
#include "soc/rtc_cntl_reg.h"

constexpr const char *TAG = "main";

bool ulp_crashed = false;

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

void request_temperature(){
    temp_reading_t *reading = (temp_reading_t*)&ulp_temp_reading;
    if(reading->state == READY){
        reading->state = BEGIN;
    }
}

void print_history(){
    temp_reading_t *reading = (temp_reading_t*)&ulp_temp_reading;
    double *history = (double*)&ulp_history;
    ESP_LOGI(TAG, "Current Reading: %f", reading->temp_in_f);
    for(int i = 0; i < HISTORY_LENGTH; i++){
        if(*(history + i) > 0){
            ESP_LOGI(TAG, "History %d: %f", i, *(history + i));
        }
    }
}

void handle_ulp_interrupt(void *arg)
{
    ulp_crashed = true;
}

// A simple function that tests if the ULP hardware trap signal 
// interrupt has fired. Note, this just polls the register. You
// would need to clear the interrupt bit after addressing the crash.
bool has_ulp_crashed()
{
    if(GET_PERI_REG_MASK(RTC_CNTL_INT_RAW_REG, RTC_CNTL_COCPU_TRAP_INT_RAW)){
        return true;
    }
    return false;
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
    else if(cause == ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG){
        ESP_LOGW(TAG, "Woke up because ULP crashed!");
    }
    else{
        ESP_LOGI(TAG, "Normal boot, starting ULP program");
        init_ulp_program();
    }

    // Get notified if the ULP crashes by registering an interrupt
    // handler for the hardware trap signal. Choose between using 
    // an interrupt or simply polling each time in the while loop
    ulp_riscv_isr_register(handle_ulp_interrupt, NULL, ULP_RISCV_TRAP_INT);

    ESP_LOGI(TAG, "ULP Loop Count: %d", (int)ulp_loop_count);

    // Set the wakeup timer to 5 seconds and go to sleep
    // esp_sleep_enable_timer_wakeup(20 * 1'000 * 1'000);
    // esp_sleep_enable_ulp_wakeup();
    // esp_deep_sleep_start();

    while(1)
    {
        print_history();
        request_temperature();

        static uint32_t old_lwm = 0x1000;
        if(old_lwm != ulp_min_stack_address){
            old_lwm = ulp_min_stack_address;
            ESP_LOGI(TAG, "ULP Stack Pointer LWM: 0x%lx", ulp_min_stack_address);
        }

        if(ulp_crashed){
            ESP_LOGE(TAG, "ULP has crashed");
            // Reset the variable so it only prints once
            ulp_crashed = false;
        }
        std::this_thread::sleep_for(std::chrono::seconds{5});
    }
}