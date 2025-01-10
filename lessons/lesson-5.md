# Lesson 5 - Shared Variables

## What We'll Cover
- What are shared variables
- An example of two-way shared variables
- How do they work
- Limitations of shared variables

To see all of the changes made and working simply run `git checkout lesson-5-end`. 

To follow along, run `git checkout lesson-5` and follow the instructions below.

## Shared Variables

From the IDF docs we read:

> Global symbols defined in the ULP RISC-V program may be used inside the main program.

Pretty straightforward but it's actually really cool. Remember, your main application and your ULP 
application are running on completely different processors. Yet, a variable you declare in your 
ULP app is avaible to be read or written to in your main app.

In fact, we are already using a shared variable. Remember all the way back to Lesson 2 when we first 
created our ULP app? We added a `loop_count` variable which we increment in our ULP app, and then print 
out over serial from our main application.

## Simple Example

Let's create a couple more shared variables to show that we aren't just limited to `uint32_t` 
simple variables. We'll also show how shared variables can work both ways to not only receive data from the ULP but to send data to the ULP. This example is a bit contrived but try to see the bigger picture. 

As an example, we're going to pretend that we have a temperature sensor hooked up to our RTC I2C bus. To simulate the two-way nature of shared variables we're going to implement the following specs.

- The ULP application will read and store a temperature reading
- The ULP application will maintain a history of readings
- The ULP application will only take a reading when requested by the main application

Since we are going to share data types between the two domains, it's a good idea to create a shared 
header file. Create the file **sensor.h** under the **ulp** folder with the following contents.

```c
#ifndef SENSOR_H
#define SENSOR_H

#define HISTORY_LENGTH 5

typedef enum _STATE{
  READY = 0,
  BEGIN = 1,
  IN_PROGRESS = 2,
} STATE;

typedef struct {
  double temp_in_f;
  STATE state;
} temp_reading_t;

#endif // SENSOR_H
```

Now, before you complain about this design, remember we're trying to show that we can pass more complex 
data types between the domains. I wouldn't typically put `state` in the `temp_reading_t` but that was 
the best I could come up with so we could have a `struct`, so deal with it.

So we have a `struct` containing a `double` and a custom `enum`. Perfect! Now we just need to implement 
the requirements in our ULP application. Give it a try to see if you can figure it out. Remember, shared variables between the domains have to be marked as `volatile`.

Here is my new **main.c**:

```c
#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "sensor.h"

volatile uint32_t loop_count;
volatile temp_reading_t temp_reading;
volatile double history[HISTORY_LENGTH];

void take_temperature_reading(void)
{
  // Typically you would read a sensor here with an RTC peripheral
  // To keep things simple we're going to pretend we did that and
  // make up a reading.
  static int history_index = 0;
  temp_reading.state = IN_PROGRESS;
  // 500ms delay to simulate sensor read time
  ulp_riscv_delay_cycles(500 * ULP_RISCV_CYCLES_PER_MS);
  temp_reading.temp_in_f = (double)loop_count + 1.0/loop_count;

  // Store the reading in history
  if(history_index >= HISTORY_LENGTH){
    history_index = 0;
  }
  history[history_index++] = temp_reading.temp_in_f;

  // Set the state back to ready
  temp_reading.state = READY;
}
int main (void)
{
  loop_count++;

  // Take a reading only when the main app asks for it
  if(temp_reading.state == BEGIN){
    take_temperature_reading();
  }
  // if(loop_count % 5 == 0){
  //   ulp_riscv_wakeup_main_processor();
  // }
}
```

You'll notice I commented out the main processor wakeup code. For this example, we're going to leave the 
main CPU awake so it can continuosly poll the ULP. Again, this is just a contrived example to show how 
shared variables work, not an example of best practice.

The general flow is that the ULP will check to see if `temp_reading.state` is set to `BEGIN`. If it is, 
it'll simulate taking a temperature reading by storing the current reading and adding it to a history 
array. Pretty straightforward. 

Now we need to modify our main application to access this new data and functionality. Don't forget to 
`#include ulp/sensor.h` at the top.

In **main.cpp** I've added the following function above `app_main`:

```cpp
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
```

Just like our loop count variable we have to prefix the name of the variable with `ulp_` to access it 
from the main application. The important thing to notice here is that **we take the address of the ulp 
variable and then cast it back to whatever type we need it to be.** 

> [!IMPORTANT]
> Regardless of what type you use to declare your variable in your ULP code, it will be declared as a 
> `uint32_t` in your main application. More on this below. This is why casting is necessary.

Down in our `app_main` function we're going to comment out the deep sleep code because we want our main 
CPU to stay active for this demo. We'll bring back the infinite while loop:

```cpp
// Set the wakeup timer to 5 seconds and go to sleep
// esp_sleep_enable_timer_wakeup(20 * 1'000 * 1'000);
// esp_sleep_enable_ulp_wakeup();
// esp_deep_sleep_start();

while(1)
{
    print_history();
    std::this_thread::sleep_for(std::chrono::seconds{5});
}
```
Every 5 seconds we'll print out the current reading and all history entries that aren't zero. 

Run `idf.py flash monitor` to see the result.

```txt
I (288) main: Normal boot, starting ULP program
I (288) main: ULP Loop Count: 0
I (298) main: Current Reading: 0.000000
I (5298) main: Current Reading: 0.000000
I (10298) main: Current Reading: 0.000000
I (15298) main: Current Reading: 0.000000
I (20298) main: Current Reading: 0.000000
```

Uh oh! That doesn't look right. The current reading is always zero and we're not getting any history.
Remember our spec above? The ULP should only take a reading when the main CPU requests it. We've 
implemented that feature using the `state` enum on our reading struct. The main app has to set it to 
`BEGIN` for the ULP to take a reading.

See if you can figure out how to modify **main.cpp** to request a temperature reading each time through 
the main loop. Only set the state to `BEGIN` if the ULP is ready. You don't want to overwrite the 
variable if it's in the middle of taking a reading.

Your output should look like this:

```txt
I (288) main: Normal boot, starting ULP program
I (288) main: ULP Loop Count: 0
I (298) main: Current Reading: 0.000000
I (5298) main: Current Reading: 2.500000
I (5298) main: History 0: 2.500000
I (10298) main: Current Reading: 4.250000
I (10298) main: History 0: 2.500000
I (10298) main: History 1: 4.250000
I (15298) main: Current Reading: 6.166667
I (15298) main: History 0: 2.500000
I (15298) main: History 1: 4.250000
I (15298) main: History 2: 6.166667
```

To see my implementation just run `git checkout lesson-5-end`.


## How Do They Work?

Shared variables work because both processors have access to the same RTC SLOW Memory region we 
discussed in Lesson 4. But let's uncover the mystery of this `ulp_` business and how we get these new 
variables that we never explicitly declared.

If you studied Lesson 4 (I told you not to skip it) you have all of the tools you need to answer this 
question. But let's go through it together.

At the top of our **main.cpp** file we have `#include ulp_main.h`. But we never created that file. Let's see what's in it.

```c
/* ULP variable definitions for the compiler.
 * This file is generated automatically by esp32ulp_mapgen.py utility.
 */
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
extern uint32_t ulp_RTCCNTL;
extern uint32_t ulp_SENS;
extern uint32_t ulp___adddf3;
extern uint32_t ulp___clz_tab;
extern uint32_t ulp___clzsi2;
extern uint32_t ulp___divdf3;
extern uint32_t ulp___floatunsidf;
extern uint32_t ulp___stack_top;
extern uint32_t ulp___start;
extern uint32_t ulp_history;
extern uint32_t ulp_loop_count;
extern uint32_t ulp_main;
extern uint32_t ulp_reset_vector;
extern uint32_t ulp_take_temperature_reading;
extern uint32_t ulp_temp_reading;
extern uint32_t ulp_ulp_riscv_halt;
extern uint32_t ulp_ulp_riscv_rescue_from_monitor;

#ifdef __cplusplus
}
#endif
```

As it states, it's an automatically generated file and you'll notice it's buried in the build folder. 
It contains all of our fancy `ulp_` variables that we use. So that's how we can reference them 
without getting a compile error. There are two important things to notice.

1. They are all declared with `extern` which means they are defined somewhere else.
1. They are all `uint32_t`. This is why, for any type that isn't `uint32_t`, we need to cast the address of the variable to get back our original type.

But we still don't know where they come from. Well, we learned in Lesson 4 that map files tell us a lot 
of interesting things about our program layout so let's check there. Do you remember where to find the 
map file for the main app? It's just in the build folder, **ulp_playground.map**. If we search that file for "ulp_history", for example, we find:

```txt
0x50000df0     PROVIDE (ulp_history = 0x50000df0)
```

So the `ulp_history` variable is at address 0x50000df0. Also from Lesson 4 we know that's in the RTC 
SLOW Memory region. I tried to tell you Lesson 4 was 🔥🔥🔥.

Let's go a step further and look for the `history` variable in our ULP map file. It doesn't have the "ulp_" prefix in the ULP code.

```sh
find ./build/esp-idf -name ulp_main.map
```
Open the file.

```txt
 .bss.history   0x00000df0       0x28 CMakeFiles/ulp_main.dir/workspaces/ulp_playground/main/ulp/main.c.obj
                0x00000df0                history
```

Look at that, it's at address df0 in our ULP app. And since it gets loaded right at the front or RTC 
SLOW Memory, that translates to 0x50000df0 in our main app. Another fun fact to notice here is that 
`history`, from our ULP map file, occupies 0x28(40) bytes of memory. It holds 5 readings which means 
each reading is 8 bytes which happens to correspond with the size of a `double`. Pretty cool, huh?

The last piece of this puzzle is how `ulp_history` (or our other shared variables) made it into the 
main application map file in the first place. That is thanks to another bit of autogenerated code from 
ESP-IDF. Run the following from your terminal:

```sh
find ./build/esp-idf -name ulp_main.ld
```

```txt
/* ULP variable definitions for the linker.
 * This file is generated automatically by esp32ulp_mapgen.py utility.
 */
PROVIDE ( ulp_RTCCNTL = 0x50008000 );
PROVIDE ( ulp_SENS = 0x5000c800 );
PROVIDE ( ulp___adddf3 = 0x5000001a );
PROVIDE ( ulp___clz_tab = 0x50000ce8 );
PROVIDE ( ulp___clzsi2 = 0x50000b5e );
PROVIDE ( ulp___divdf3 = 0x500005ee );
PROVIDE ( ulp___floatunsidf = 0x50000b0a );
PROVIDE ( ulp___stack_top = 0x50001000 );
PROVIDE ( ulp___start = 0x50000004 );
PROVIDE ( ulp_history = 0x50000df0 );
PROVIDE ( ulp_loop_count = 0x50000e2c );
PROVIDE ( ulp_main = 0x50000c46 );
PROVIDE ( ulp_reset_vector = 0x50000000 );
PROVIDE ( ulp_take_temperature_reading = 0x50000b98 );
PROVIDE ( ulp_temp_reading = 0x50000e18 );
PROVIDE ( ulp_ulp_riscv_halt = 0x50000c86 );
PROVIDE ( ulp_ulp_riscv_rescue_from_monitor = 0x50000c72 );
```

Another auto-generated file. This time it's a linker file that provides symbols to our main app linker. 
It now knows to make the `ulp_history` variable reference the same spot in RTC SLOW Memory where our ULP program will be loaded so they perfectly match. 

This completes the connection between the two processors so they can both access the same variable in a 
bidirectional manner.

## Limitations

This is all pretty great but, as with everthing, there are drawbacks.

### Race Condiditions
The first drawback should be pretty obvious. Two completely different processors have address pointers 
to the same spot in a shared memory location. That's like the perfect opening line to a book written 
about software bugs caused by race conditions.

The IDF does offer `ulp_riscv_lock_acquire()` and `ulp_riscv_lock_release()` functions to help with 
this but it includes the following caveat.

> The ULP does not have any hardware instructions to facilitate mutual exclusion, so the lock API 
> achieves this through a software algorithm (Peterson's algorithm).
> 
> The locks are intended to only be called from a single thread in the main program, and will not 
> provide mutual exclusion if used simultaneously from multiple threads.

So while there is a form of mutex support for shared variable access it's only at the main app 
level and it's only on a single thread. 

You need to be very careful and intentional about how you access the shared variables between the two 
domains.

### Everything is a `uint32_t`
As we covered above, regardless of the type you give your shared variables in the ULP domain, they
are surfaced in the main application domain as `uint32_t`. This means you need to be careful and 
intentional about how you cast them to "recover" them in the main application domain. 

### Pre-Access Error
Before the ULP application is loaded with a call to `ulp_riscv_load_binary` your main application 
reference of the shared variable will be pointing to uninitialized memory. This can introduce an 
entire class of nasty bugs. Don't access your shared variables from the main application domain 
until you've loaded the ULP app.

### Code Size
Your variables are stored in the same memory region as your ULP code. Whatever size you reserve for your 
ULP application has to be big enough for your code, variables, and stack. The more variables you share 
the more space they will consume which leaves less room for your code and stack. 

In our example above we store our temperature as a `double` which makes sense given it can be a fraction. 
However, each `double` occupies 8 bytes of memory. That means for our current reading along with 
5 historical readings we are using 8x6=48 bytes of memory in our ULP space. In a future lesson I'll 
show you how we can drastically reduce that with some creative thinking and type changes.
