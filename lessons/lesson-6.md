# Lesson 6 - The ULP Stack

## What We'll Cover
- The RISC-V ULP Stack Spec
- ESP32 Implementation
- How to calculate your stack space
- A simple trick to monitor the stack
- An Ominous Warning

To see all of the changes made and working simply run `git checkout lesson-6-end`. 

To follow along, run `git checkout lesson-6` and follow the instructions below.

## The RISC-V Stack Specification
I'm not going to bore you with large portions of the RISC-V spec. It's great reading if you need 
something to put you to sleep. All we care about in this lesson is the stack and all I'm going to share 
are these few bits of information from the [Calling Convention](https://riscv.org/wp-content/uploads/2024/12/riscv-calling.pdf) document.

> In the standard RISC-V calling convention, the stack grows downward and the stack pointer is
always kept 16-byte aligned.

> The stack pointer `sp` points to the first argument not passed in a register.

> The RISC-V calling convention passes arguments in registers when possible. Up to eight integer
registers, a0–a7, and up to eight floating-point registers, fa0–fa7, are used for this purpose.

> [!TIP]
> I **HIGHLY** recommend you stop here and take 20 minutes to read the RISC-V Calling convention doc I 
> linked above. It's only 3 pages long and is a treasure trove of data to help you understand how the 
> stack works in your ULP program.

> [!NOTE]
> While the document mentions floating-point registers, recall that the RISC-V chip on the ESP32 is 
> fixed point so registers fa0-fa7 won't exist.

So the spec has taught us some really important things:

1. The stack grows downward in memory
1. The stack pointer is always kept 16-byte aligned
1. Hardware registers are used to hold function arguments instead of the stack where possible
1. The `sp` register points to the first argument NOT in a register i.e to the start of the stack frame

Number 3 was an important concept for me when I was first digging into this. It's general knowledge that 
function arguments and local variables go on the stack. This, however, tells us that most simple 
functions with a couple of arguments don't add any additional stack space because the arguments are 
stored in raw hardware registers. 🤯

Locals are still added to the stack but this is a great piece of information to know when it comes to 
optimizing your C code. 

## How The ESP32 Implements the Stack

The implemenation is quite trivial and we've already seen it. I just didn't call it out at the time. Back in Lesson....you guessed it, Lesson 4 we looked at the generic linker file **ulp_riscv.ld** provided by IDF. Here it is again for reference.

```txt
ENTRY(reset_vector)
MEMORY
{
    ram(RW) : ORIGIN = 0, LENGTH = 4096
}
SECTIONS
{
    . = ORIGIN(ram);
    .text :
    {
        *ulp_riscv_vectors.S.obj(.text.vectors)
        *(.text)
        *(.text*)
    } >ram
    .rodata ALIGN(4):
    {
        *(.rodata)
        *(.rodata*)
    } > ram
    .data ALIGN(4):
    {
        *(.data)
        *(.data*)
        *(.sdata)
        *(.sdata*)
    } > ram
    .bss ALIGN(4) :
    {
        *(.bss)
        *(.bss*)
        *(.sbss)
        *(.sbss*)
    } >ram
    __stack_top = ORIGIN(ram) + LENGTH(ram);
}
```

All the way at the very bottom we have `__stack_top = ORIGIN(ram) + LENGTH(ram);`. That creates the 
symbol for our stack pointer. It's assigned the value of the origin of our `ram` memory space plus the 
length of that space. With our current `menuconfig` settings that means 4096 bytes. So our `__stack_top` 
symbol points to the very end of the ULP memory region.

This allows it to meet the RISC-V spec of growing downward (because it has no other direction to grow).

But how does the stack pointer get initialized? The linker file just defines the symbol. It doesn't load 
any registers. That has to happen at runtime in the form of RISC-V assembly commands executed by the 
processor. 

The stack initialization happens in the startup code which is also provided by ESP-IDF. To find the file we need to search for our entry routine mentioned in the linker file (`reset_vector`). Run the following.

```sh
grep -rnw ~/esp/idf/components/ -e reset_vector
```

 The search will yield a few results but we are looking for assembly source code so something that ends 
 in ".S". That still gives us a couple of options, several of which refer to an "lp_core" in the path. 
 This is for a different chip than the ESP32-S3 that we're using so we're going to open 
 **components/ulp/ulp_riscv/ulp_core/ulp_riscv_vectors.S**.  Remember, you can just Ctrl-click it in the 
 VSCode terminal window and it will open in a new editor tab.

 In that file we find the following:

 ```txt
/* The reset vector, jumps to startup code */
reset_vector:
	j __start
 ```

 This is the function definition of `reset_vector` with a very helpful comment informing you that the 
 single assembly instruction is a jump instruction to another function called `__start`. Let's keep 
 hunting. A trick for a lot of these assembly functions is that they are declared as `.global [function 
 name]`. Knowing this can help us narrow our search.

 ```sh
 grep -rnw ~/esp/idf/components/ -e ".global __start"
 ```

This time we get a single file result so let's open it up. It's small enough we can put the entire file here.

```gas
#include "sdkconfig.h"
#include "ulp_riscv_interrupt_ops.h"

    .section .text
    .global __start

.type __start, %function
__start:
    /* setup the stack pointer */
    la sp, __stack_top

#if CONFIG_ULP_RISCV_INTERRUPT_ENABLE
    /* Enable interrupts globally */
    maskirq_insn(zero, zero)
#endif /* CONFIG_ULP_RISCV_INTERRUPT_ENABLE */

    /* Start ULP user code */
    call ulp_riscv_rescue_from_monitor
    call main
    call ulp_riscv_halt
loop:
    j loop
    .size __start, .-__start
```

As you can see, the very first instruction in the `__start` function is `la sp, __stack_top`. This is a 
RISC-V assembly instruction that says "load symbol addres"(la) of `__stack_top` into register `sp` which 
is the RISC-V ABI symbol name for the stack pointer. 

A little further down you can also see `call main` which is how the ULP app starts executing the code 
you wrote in **main.c**. Pretty cool stuff. Now you know, at the assembly level, how we go from ULP 
processor startup to your C code. And we also see how the stack pointer gets initiatlized. 

## How Much Stack Space Do I Have?
A very important piece of information you need to know when writing ULP programs is how much 
stack space your program has. This is very easy to calculate.

```txt
[STACK SPACE] = [ULP RESERVED MEM] - [SIZE OF ALL LINKED SECTIONS] - 0x10(16 bytes for call to main)
```

You can use the ULP application map file to easily calculate this (Lesson 4 folks). 
For simplicity let's consider the following condensed map file. By condensed, I mean it only includes 
the lines that show the address and size of the 4 sections along with the `MEMORY` section

```txt
MEMORY
{
    ram(RW) : ORIGIN = 0, LENGTH = 4096
}
SECTIONS
{
.text           0x00000000      0xcd0
...
.rodata         0x00000cd0      0x13c
...
.data
...
.bss            0x00000e18       0x48
...
}
```

To calculate your available stack size you take the total length of 4096 and convert it to hex which is 
0x1000. Then you simply subtract the size of each section along with an additional 16 bytes to account 
for the mandatory call to your main function ("Can't get around the old minimum wage, Mortimer" IYKYK).

```txt
Stack Space = 0x1000 - 0xcd0 - 0x13c - 0x48 = 0x1ac = 428 - 16 (call to main) = 412 bytes
```

Notice the `.data` section in this example is empty as it has no size or address specified.

Your main application has 412 bytes available for stack growth. If all of your function calls are simple 
with not a lot of local variables you can assume 16 bytes of growth per call depth. That means, in this 
scenario, your function call stack could go 412 / 16 = 25.75, rounded down, 25 call levels deep.

Knowing that can inform some decisions like whether or not to shrink or grow your total ULP memory 
allocation.


## Monitoring the Stack
If you've written much code for the ESP32 you may have used a super nice helper function called 
`uxTaskGetStackHighWaterMark`. This lets you know, for a given main application task, the high water 
mark for the stack. The lower that number, the closer you have come to a stack overflow and a panic 
reset of your ESP32.

Knowing that information is extremely helpful in configuring your ESP32 task stack size during 
development to ensure you aren't playing with fire once your firmware goes to production.

Unfortunately there isn't an equivalent function we can call from our ULP application. No problem. we're 
developers, let's write one. Better yet, let's do it in assembly. Sounds scary but it's actually quite 
trivial as you'll see in a moment.

All we are trying to do is get the value of the stack pointer and display it somehow. Well, from Lesson 
4 (if you still haven't gone through it at this point, you're hopeless and are never going to understand 
this stuff. Stop being lazy and do Lesson 4!). Anyway, from Lesson 4 we know that we have access to the 
RISC-V RV32IMC instruction set so really we just need to know a couple of things.

1. How do I get the value of the stack pointer
1. How do I load that into a C variable

Here is the quick version. From the Calling Convention doc (linked above) we know that the stack 
pointer's ABI register name is `sp`. Now we just need to figure out how to load the value in that 
register into a C variable. Well, also from the Calling Convention doc, we know that the return value of 
a function is always passed in register `a0` if it will fit in 4 bytes, and in a combination of `a0` and 
`a1` if we need 16 bytes. Since all addressing in RISC-V is 32-bit, `a0` is all we need to return 
the stack pointer value.

With this we can reword what we are trying to do. "I need to load the value of `sp` into `a0` and call 
that function from my C code".

Create a new file in the **ulp** folder called **sp.S** (.S is the typical file extension for assembly). 
In that file we are going to create an assembly function called `getsp`. If you aren't an assembly 
wizard don't worry. I created this by simply pattern-matching the things I saw while digging around 
earlier to find the startup code.

```gas
.section .text.sp

# Assembly function to get the value of the stack pointer
.global getsp
.type getsp, @function

getsp:
    add a0, sp, zero
    ret
```

`.section .text.sp` specifies the section this should be put in. This helps our linker file know that it 
needs to be added to our `.text` section.

We declare the function as global and as type `@function` and then we define it. It's two lines of code; 
an add instruction and the return instruction.

There is probably a better way to do this but it was the easiest I could find having not much RISC-V 
assembly experience (translation: almost none). The `add` instruction will store into `a0` (our return 
value by calling convention) the sum of `sp` and `zero`. `zero` is a special register in RISC-V that is, 
yep, the value 0. So we are adding zero to the stack pointer and then storing the result in `a0` which 
means we're just storing the value of `sp` in `a0`. (I couldn't figure out how to do this with a load 
instruction).

Now we just need to tell the build system to build this code and make it available to the linker. That's 
as easy as adding it to the source list in our **CMakeLists.txt** file in our **main** directory.

```cmake
set(ulp_riscv_sources 
    "ulp/main.c"
    "ulp/sp.S"
)
```

Now we can call it from our ULP program and store the result in a shared variable. Let's write a 
function that gives us the low water mark of our ULP stack. In other words, let's keep track of how low 
our stack address gets. See if you can write some code in **main.c** to store the ULP stack 
low water mark in a shared variable as well as the current stack pointer address. 
We could put this code in a function but that would distort the 
value because calling it will further decrease the stack address.

Here's my version. I added two new shared variables. One for the current stack pointer address and one 
for the lowest address.

```c
extern uint32_t getsp();
volatile uint32_t cur_stack_address;
volatile uint32_t min_stack_address;
```

The `extern uint32_t getsp();` tells the compiler "This function exists, but is defined somewhere else. 
Where is of no importance to you young Padawan compiler, for the linker knows."

Then I added this to the `main` function.

```c
cur_stack_address = getsp();
if(cur_stack_address < min_stack_address || min_stack_address == 0) {
    min_stack_address = cur_stack_address;
}
```

Back up in the main application `app_main` we can add the following in our `while` loop to print out the 
stack pointer low water mark.

```cpp
static uint32_t old_lwm = 0x1000; // The starting point of the stack
if(old_lwm != ulp_min_stack_address){
    old_lwm = ulp_min_stack_address;
    ESP_LOGI(TAG, "ULP Stack Pointer LWM: 0x%lx", ulp_min_stack_address);
}
```

Time to run it and see what happens. `idf.py flash monitor`

```txt
I (288) main: Normal boot, starting ULP program
I (288) main: ULP Loop Count: 0
I (298) main: Current Reading: 0.000000
I (298) main: ULP Stack Pointer LWM: 0xff0
```

That's awesome!! Let's think about this for a second. The stack pointer starts at address 0x1000. 
However, in the startup code we found above we see a `call main` which is a call to our `main` function. 
Well, when you call a function a new stack frame has to be added. There are no arguments and no locals 
in 
our `main` but, per spec, the stack pointer has to be 16-byte aligned so it will grow downward by a 
minimum of 16 
bytes for each function call. So, in hex, the stack pointer started at 0x1000. `main` is one stack frame 
deep so it will subtract 16 bytes, 0x10 hex, from the stack pointer address. 0x1000 - 0x10 = 0xff0 which 
is exactly what we see!

From our main application we can now monitor the location of the stack pointer.

## Beware of Stack Growth
So now you should have a pretty solid understanding of how the stack works in your ULP program.
Every time you call a level deeper in your code, a minimum of 16*n bytes is subtracted from the stack 
pointer. When that function returns, the stack pointer has 16*n bytes added back to it. If your code 
has only a few functions and doesn't have a lot of nested calls you probably won't need much stack space.

However, if your code is more complex and starts to make a lot of nested function calls or, even worse, 
uses recursion, what prevents the stack from growing indefinitely? The answer is....nothing. Absolutely 
nothing prevents the stack from runaway growth in your ULP program. And that, friends, is what we 
will cover in the next lesson. (🎵🎵 ominous music plays 🎵🎵)