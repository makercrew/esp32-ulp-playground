# Lesson 8 - The Stack Monster: Part Deux 🧟 🧟 

## What We'll Cover
- You thought it was bad in Lesson 7
- Hold my beer 🍺

To see all of the changes made and working simply run `git checkout lesson-8-end`. 

To follow along, run `git checkout lesson-8` and follow the instructions below.

## Watch the ULP Eat Itself
In Lesson 7 we showed how stack growth can cause serious issues with our shared variable data 
integrity. Well, just to carry this doomsday prophecy to it's logical end, I'm now going to show 
you how the ULP can actually eat itself if the stack grows too much.

In Lesson 7 we were merciful and kept the simulated stack growth contained to this `.bss` section. 
Not this time. 
This time we're going to release the full wrath of recursion on our ULP application and watch it 
consume itself.

## Simple Recursion
We have to set a few things up to make this happen. We could use `stackbuster` but I wanted to show an 
example with real runaway code. Meet `nXOR`. Don't ask me how I came up with this. I started with the 
classic fibonacci recursion but the problem was twofold. One, it quickly starts to seriously bog 
down the ULP and two, the return value gets really big really fast. I even found out how helpful 
the compiler can be by actually optimizing out your recursion in some cases.

After a bunch of trial and error I ended up with `nXOR` which the compiler doesn't optimize, it runs 
fast, and can recurse to the depth of max val of `int`. Let's add it to our ULP app.

```c
uint32_t nXOR(int n)
{
    // dummy here helps increase the size of the stack frame from 16 bytes
    // to 64 bytes. It helps speed up the process of self destruction
    int dummy[10];

    // We want to track stack low water mark as our recursion deepens
    cur_stack_address = getsp();
    if(cur_stack_address < min_stack_address || min_stack_address == 0) {
        min_stack_address = cur_stack_address;
    }

    // base condition to terminate the recursion when N = 0
    if (n == 0) {
        return 0;
    }

    // recursive case / recursive call
    int res = n ^ nXOR(n - 1) ^ dummy[4];

    return res;
}
```

To watch it slowly kill itself we're going to call `nXOR` with the loop count so it 
generates progressively deeper call stacks.

Our ULP `main` function now looks like this:

```c
int main (void)
{
  loop_count++;

  cur_stack_address = getsp();
  if(cur_stack_address < min_stack_address || min_stack_address == 0) {
      min_stack_address = cur_stack_address;
  }

  // Take a reading only when the main app asks for it
  if(temp_reading.state == BEGIN){
    take_temperature_reading();
  }

  nXOR(loop_count);
}
```

We're also going to modify our loop in the main application as well. Instead of having a long sleep 
delay we are going to constantly monitor the ULP stack and report on changes. We're also going 
to skip requesting the temperature reading so we can just focus on stack growth from `nXOR`.

This is what our `while` loop now looks like in `app_main` in **main.cpp**.

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
    std::this_thread::sleep_for(std::chrono::milliseconds{200});
}
```

If we flash and run this code this is what the output looks like.

```txt
I (288) main: Normal boot, starting ULP program
I (288) main: ULP Loop Count: 0
I (298) main: ULP Stack Pointer LWM: 0xf70
I (2298) main: ULP Stack Pointer LWM: 0xf30
I (4298) main: ULP Stack Pointer LWM: 0xef0
I (6298) main: ULP Stack Pointer LWM: 0xeb0
I (8298) main: ULP Stack Pointer LWM: 0xe70
I (10898) main: ULP Stack Pointer LWM: 0xe30
I (12898) main: ULP Stack Pointer LWM: 0xdf0
I (14898) main: ULP Stack Pointer LWM: 0xdb0
I (16898) main: ULP Stack Pointer LWM: 0xd70
I (18898) main: ULP Stack Pointer LWM: 0xd30
I (20898) main: ULP Stack Pointer LWM: 0xcf0

```

It never updates past this and appears to hang. Actually, our ULP has crashed.

## How Bad Is It?
It's pretty bad. First of all, we can see that each level of recursion decreased our stack pointer 
by a constant 64 bytes (0x40). That's pretty brutal. But if you look at the map file you'll see the 
following memory addresses. 

- **.bss 0xe78** - 5 iterations in, we've already clobbered this
- **.rodata 0xd34** - 10 iterations in, this section is toast

The final reported stack value was 0xcf0 which puts us in the `.text` section where it is overwriting 
the system call `ulp_riscv_rescue_monitor`. If we assume it made it one level deeper that would put us 
at address 0xcb0 which is around where our `getsp` function is placed. That explains why we stopped 
getting updates. And since the code at that address was overwritten by the stack, the next time our 
`main` function tries to call `getsp` it's pretty much guaranteed to be an illegal RISC-V instruction 
so the processor will crash. 💥

I'm sorry you had to witness that but it's for your own good. The scariest part to me is how much data memory we trashed on the way to full system meltdown but things kept happily running.

So, I repeat, make sure you absolutely, positively, 100% understand your stack usage in the ULP 
domain and ensure you have allocated enough RTC SLOW Memory for it.

Next up, we're going to look at how to harden code around your ULP app to detect when things have gone 
wrong so you can gracefully recover.