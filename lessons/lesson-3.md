# Lesson 3 - Wake Up the Main CPU

## What We'll Cover
- Waking up the main CPU from the ULP
- Putting the main CPU in deep sleep
- Properly handle the wakeup source from the main application

To see all of the changes made and working simply run `git checkout lesson-3-end`. 

To follow along, run `git checkout lesson-3` and follow the instructions below.

## Wake Up, Sleepy Head

A primary reason for using the ULP in the first place is for low power applications. It can carry 
out your business logic on a much lower power budget than the main ESP32 CPU. As such, when using the 
ULP you will almost always utilize deep sleep for the main CPU.

> [!IMPORTANT]
> The ULP application typically runs while the main CPU is in deep sleep. However, it can also run 
> while the main CPU is awake which can lead to some tricky concurrency problems.

With the main CPU in deep sleep, the ULP application is left tending to the house. But what happens when 
the ULP application detects something that it needs it's big brother (the main CPU) to handle? Well, in 
that case, you need to wake up the main CPU.

The ULP can't do heavy data processing or talk to the internet. It will typically monitor some sensors 
and then wake up the main CPU to do the heavy lifting. Here's how.

```c
ulp_riscv_wakeup_main_processor();
```

That's it! Any time you need to wake the main CPU from your ULP application, just call that function 
and big brother will wake up from deep sleep.

For our super contrived sample app, let's immediately wake up the main CPU any time the loop count is 
a multiple of 
5. Currently, the main CPU isn't sleeping at all, so let's fix that first.

## Sleep Tight, Big Brother
Right now our main application is just looping and reporting the ULP loop count every 5 seconds. 
Let's modify this to put the main application in deep sleep and then wake up every 5 seconds to report 
the loop count. Replace your `app_main` function with the following code.

```cpp
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-S3 ULP Playground");
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "Wake up cause: %d", cause);

    if (cause == ESP_SLEEP_WAKEUP_TIMER){
        ESP_LOGI(TAG, "Woke up from deep sleep timer");
    }
    else{
        ESP_LOGI(TAG, "Normal boot, starting ULP program");
        init_ulp_program();
    }

    ESP_LOGI(TAG, "ULP Loop Count: %d", (int)ulp_loop_count);

    // Set the wakeup timer to 20 seconds and go to sleep
    esp_sleep_enable_timer_wakeup(20 * 1'000 * 1'000);
    esp_sleep_enable_ulp_wakeup();
    esp_deep_sleep_start();
}
```

We've introduced a new concept here of a wakeup cause. This allows us to know why our ESP32 has woken up.
We get that value by calling `esp_sleep_get_wakeup_cause()` and then we can compare that value to the 
known list of causes and act accordingly.

In our case, if we are waking up from the deep sleep timer going off then we don't need to load the ULP program 
because it's already running. However, if it's not a wakeup from the timer then we want to load it and 
start executing it.

The only other difference here was to replace the while loop with a 5s sleep with a few lines of code 
to put our ESP32 into deep sleep mode with a wakeup interval of every 20 seconds.

> [!IMPORTANT]
> You'll need to add `#include "esp_sleep.h"` to the top of **main.cpp** to be able to access the 
> sleep variables and functions.

Now when we first run our application you will see output like this:

```txt
I (255) main_task: Started on CPU0
I (295) main_task: Calling app_main()
I (295) main: ESP32-S3 ULP Playground
I (295) main: Wake up cause: 0
I (295) main: Normal boot, starting ULP program
I (295) main: ULP Loop Count: 0
```

After 20 seconds the device will appear to completely reset and boot up again. That's becuase it's 
coming out of deep sleep but you'll notice a difference in the output.

```txt
I (256) main_task: Started on CPU0
I (296) main_task: Calling app_main()
I (296) main: ESP32-S3 ULP Playground
I (296) main: Wake up cause: 4
I (296) main: Woke up from deep sleep timer
I (296) main: ULP Loop Count: 11
```

This time we see the wakeup cause was 4 (`ESP_SLEEP_WAKEUP_TIMER`) and the ULP loop count is 
reported. If you continue to let this run you'll find the loop count continues to increment even 
though the main ESP32 is mostly in deep sleep mode. That's because your ULP program is happily 
running in the background and incrementing the count.

## Conditional Wakeup
Currently our main application is waking up every 20 seconds to check on things. But, remember, we want 
to force it to wake up any time the loop count is a multiple of 5. The change is rather trivial on the ULP side.

```c
int main (void)
{
  loop_count++;
  if(loop_count % 5 == 0){
    ulp_riscv_wakeup_main_processor();
  }
}
```

That's all we need from the ULP side. However, we've just introduced another wakeup cause. Currently, 
our main app handles the wakeup cause for the deep sleep timer and normal boot. But now we're forcing a 
wakeup from the ULP. This is handled separately as `ESP_SLEEP_WAKEUP_ULP`. This is similar to our deep 
sleep timer wakeup in that we don't want to try to reload our ULP program.

See if you can figure out how to modify **main.cpp** to handle this new wakeup cause. Make it log 
something useful. If you've done everything correctly your app should boot and after about 10 seconds should wake up from deep sleep and log something like this.

```txt
I (254) main_task: Started on CPU0
I (294) main_task: Calling app_main()
I (294) main: ESP32-S3 ULP Playground
I (294) main: Wake up cause: 6
I (294) main: Woken up by ULP co-processor
I (294) main: ULP Loop Count: 5
```

Great job! You now know how to wake up your main application from your ULP application. Now you can have the ULP monitor sensors in ultra low power mode and only wake up the main CPU for tasks requiring it's full power.