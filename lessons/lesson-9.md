# Lesson 9 - Detecting a ULP Crash

## What We'll Cover
- How to detect a ULP crash when the main app is running
- How to detect a ULP crash when the main app is in deep sleep

To see all of the changes made and working simply run `git checkout lesson-9-end`. 

To follow along, run `git checkout lesson-9` and follow the instructions below.

## Thanks For Nothing
A very logical question you might have after Lesson 8 is "Why was there no indication 
in the main application that the ULP had died?" This is a great question. When the main 
application has a critical failure the ESP32 typically reboots and gives you some kind of error.

The ULP is different. Remember, it's a completely separate processor running code out of a shared 
memory space. As far as the ESP32 is concerned it should be able to handle itself. We proved 
otherwise but it would be very helpful to catch such things. Thankfully, we can. The approach you take 
depends on whether the main application is awake or in deep sleep.

From the IDF documentation we read: 

> Trap signal: the ULP RISC-V has a hardware trap that will trigger under certain conditions, e.g., 
> illegal instruction. This will cause the main CPU to be woken up with the wake-up cause 
> `ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG`.

## Detecting ULP Crashes While Awake
Based on that documentation note, the deep sleep scenario sounds trivial so we'll handle it next. Let's  tackle the harder one first. Our code is currently in a state where a ULP crash is happening due to our 
recursion problem but nothing is surfaced in the main app. As a reminder, here is the output.

```txt
I (287) main: Normal boot, starting ULP program
I (287) main: ULP Loop Count: 0
I (297) main: ULP Stack Pointer LWM: 0xf70
I (2297) main: ULP Stack Pointer LWM: 0xf30
I (4297) main: ULP Stack Pointer LWM: 0xef0
I (6297) main: ULP Stack Pointer LWM: 0xeb0
I (8297) main: ULP Stack Pointer LWM: 0xe70
I (10897) main: ULP Stack Pointer LWM: 0xe30
I (12897) main: ULP Stack Pointer LWM: 0xdf0
I (14897) main: ULP Stack Pointer LWM: 0xdb0
I (16897) main: ULP Stack Pointer LWM: 0xd70
I (18897) main: ULP Stack Pointer LWM: 0xd30
I (20897) main: ULP Stack Pointer LWM: 0xcf0
```

After stack address 0xcf0 is reported there is no more output. It's not entirely obvious from the 
documentation but the way we handle catching a ULP crash while the main app is awake uses the same 
mechanism as when it's in deep sleep. We can't look at the wakeup cause but what we can do is look at a 
certain register that contains information about whether the ULP hardware trap signal has been 
generated. We have two approaches to this; 1) use an interrupt or 2) poll the register itself.

### Using an Interrupt
In our main application **main.cpp** we need to register a ULP ISR using `ulp_riscv_isr_register`. It 
needs a callback function, optional context and we have to tell it what interrupt we want to know about.

Add the following in `app_main` somewhere above the `while` loop.

```cpp
ulp_riscv_isr_register(handle_ulp_interrupt, NULL, ULP_RISCV_TRAP_INT);
```

We are registering a callback function named `handle_ulp_interrupt` that doesn't need a callback context 
so `NULL` is passed as the second parameter. Finally, we are interested in the `ULP_RISCV_TRAP_INT` 
interrupt.

Now we just need to define `handle_ulp_interrupt`. Interrupt routines should be extremely lightweight 
and return quickly. As such, we can't use our normal logging statements in our interrupt handler. 
Here is the definition of `handle_ulp_interrupt`.

```cpp
void handle_ulp_interrupt(void *arg)
{
    ulp_crashed = true;
}
```

`ulp_crashed` can be declared and initialized to `false` at the top of **main.cpp**. Let's test things 
out by running `idf.py flash monitor`.

Finally, in our `app_main` `while` loop we can report an error if `ulp_crashed` is true.

```cpp
while(1)
{
    // print_history();
    // request_temperature();

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
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
}
```

Let's test things out by running `idf.py flash monitor`.

```txt
I (287) main: Normal boot, starting ULP program
I (287) main: ULP Loop Count: 0
I (297) main: ULP Stack Pointer LWM: 0xf70
I (2297) main: ULP Stack Pointer LWM: 0xf30
I (4297) main: ULP Stack Pointer LWM: 0xef0
I (6297) main: ULP Stack Pointer LWM: 0xeb0
I (8297) main: ULP Stack Pointer LWM: 0xe70
I (10897) main: ULP Stack Pointer LWM: 0xe30
I (12897) main: ULP Stack Pointer LWM: 0xdf0
I (14897) main: ULP Stack Pointer LWM: 0xdb0
I (16897) main: ULP Stack Pointer LWM: 0xd70
I (18897) main: ULP Stack Pointer LWM: 0xd30
I (20897) main: ULP Stack Pointer LWM: 0xcf0
E (20897) main: ULP has crashed
```

Excellent, now we know our ULP has crashed and can do something about it like reload it.

### Polling for a Crash
An interrupt is the best way to be notified immediately of a ULP crash. However, for the sake of 
completeness I'm also going to show you how to poll for a ULP crash. This took some digging through 
the ESP32 Technical Manual and IDF source code but here is a simply function that can be called 
regularly to see if the ULP has crashed.

```cpp
bool has_ulp_crashed()
{
    if(GET_PERI_REG_MASK(RTC_CNTL_INT_RAW_REG, RTC_CNTL_COCPU_TRAP_INT_RAW)){
        return true;
    }
    return false;
}
```

I'll leave it as an exercise to you if you want to test this out. In fact, you definitely should. See 
if you can refactor `app_main` to no longer use the interrupt approach and instead call this 
function each time through and report if the ULP has crashed.

## Detecting ULP Crashes While In Deep Sleep

We covered wakeup cause back in Lesson 3. Currently our main program is handling two causes: 
- `ESP_SLEEP_WAKEUP_TIMER`
- `ESP_SLEEP_WAKEUP_ULP`

This allows it to be woken up by our deep sleep timer as well as by a call to `ulp_riscv_wakeup_main_processor` from the ULP app. From the documentation note above we learn that 
catching a ULP crash while the ESP32 is in deep sleep is as simple as handling an additional wakeup 
cause of `ESP_SLEEP_WAKEUP_COCPU_TRAP_TRIG`. 

Let's refactor our code to try this out. Here is the refactored `app_main` which accounts for the new 
wakeup cause but also switches back to deep sleep instead of a constant `while` loop.

```cpp
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

    ESP_LOGI(TAG, "ULP Loop Count: %d", (int)ulp_loop_count);

    esp_sleep_enable_ulp_wakeup();
    esp_deep_sleep_start();
}
```

When you run this code you will see it report the initial loop count of zero. Then, after about 20 
seconds the ESP32 will wake from deep sleep with the following output.

```txt
I (294) main_task: Calling app_main()
I (294) main: ESP32-S3 ULP Playground
I (294) main: Wake up cause: 11
W (294) main: Woke up because ULP crashed!
I (294) main: ULP Loop Count: 11
```

This is exactly what we are looking for. You can see the ULP continued to run while in deep sleep 
(loop count 11) but when it crashed it woke up our main processor with the correct cause code.

> [!IMPORTANT]
> **The ULP will NOT restart on it's own after a crash.** It will typically crash if the app code has 
> become corrupt. Until you reload the ULP app or hard reset the ESP32 to clear RTC SLOW Memory, it will 
> be in a bad state.