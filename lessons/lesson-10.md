# Lesson 10 - Reducing App Size

## What We'll Cover
- How to reduce app size by inspecting the map file

To see all of the changes made and working simply run `git checkout lesson-10-end`. 

To follow along, run `git checkout lesson-10` and follow the instructions below.

The code has been refactored slightly to eliminate the failures we were covering in previous 
lessons. We are back to simply reporting temperatures every 5 seconds. This should be the ouput 
of the code at the start of this lesson.

```txt
I (288) main: Normal boot, starting ULP program
I (288) main: ULP Loop Count: 0
I (298) main: Current Reading: 0.000000
I (298) main: ULP Stack Pointer LWM: 0xff0
I (5298) main: Current Reading: 2.500000
I (5298) main: History 0: 2.500000
I (10298) main: Current Reading: 4.250000
I (10298) main: History 0: 2.500000
I (10298) main: History 1: 4.250000
I (15298) main: Current Reading: 6.166667
I (15308) main: History 0: 2.500000
I (15308) main: History 1: 4.250000
I (15308) main: History 2: 6.166667
```

## A Quick Reminder

We've covered a lot so far in this series. A very important reminder we need as a segue into this 
lesson is that we do not have a lot of space for our ULP program. A bottom tier ESP32 will have at 
least 2MB of flash space. Our ULP application can consume, at the very most, 8KB of space. That's not a 
lot. 

That is the impetus for this lesson. You may find that you want to run more and more logic in the ULP to 
conserve power. As you do so, however, you are likely to bump up against the code size limit.

The easiest place to see how big your ULP application has become is to look in the `.S` output file in 
the **build** folder. It will be at the very bottom. If we do a fresh build of our project in it's 
current state we can see the following in **build/ulp_main.bin.S**.

```txt
.global ulp_main_bin_length
ulp_main_bin_length:
.long 3624
```

Our ULP app is 3624 bytes in size which means it's taking up about 44% of our total available space 
and 88% of our currently allocated space which is set to 4096.
That's not very good since all we are doing is creating a fake temperature reading and stuffing it into 
a shared variable.

## Lesson 4 Saves Us...Again (Map File)

I'm starting to believe that the only really important lesson in this series is Lesson 4. We seem to 
always refer back to it and we'll do so again to help us shrink our application. At the end of Lesson 
4 I told you about the map file for our ULP application and how it contains a gold mine of information. 
Well, the time as come to mine that gold. ⛏️

Do you remember how to find it?

```sh
find ./build/esp-idf -name ulp_main.map
```

Open that file up. At the very top there is a section called _Discarded input sections_. This is a list 
of variables and functions that were compiled but discarded by the linker because they aren't used 
anywhere. It's telling you about all of your dead code which is pretty helpful. But let's skip past all 
of that until we get to the included sections. You'll want to scroll down until you see the following:

```txt
                0x00000000                        . = ORIGIN (ram)

.text           0x00000000      0xce4
```

This is the start of your included sections. Right from the start you can see the `.text` section 
starts at address 0 and has a size of 0xce4 (3300) bytes. As you scroll through the map file each 
line tells you a few pieces of information. What section something will be placed in, at what 
address, it's size, and where it came from. As you can imagine, when trying to reduce the size of 
your ULP app, you are most interested in the size. Once you find out what is taking up all of the space 
you can look at ways to improve your code.

So, looking at our existing map file, at just the `.text` section, try to identify the biggest two 
items.

```txt
 .text          0x0000001a      0x5d4 /home/dev/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20241119/riscv32-esp-elf/bin/../lib/gcc/riscv32-esp-elf/14.2.0/rv32imc_zicsr_zifencei/ilp32/libgcc.a(adddf3.o)
                0x0000001a                __adddf3
 .text          0x000005ee      0x51c /home/dev/.espressif/tools/riscv32-esp-elf/esp-14.2.0_20241119/riscv32-esp-elf/bin/../lib/gcc/riscv32-esp-elf/14.2.0/rv32imc_zicsr_zifencei/ilp32/libgcc.a(divdf3.o)
                0x000005ee                __divdf3
```

From this we see that `__adddf3` and `__divdf3` together account for 0x5d4 + 0x51c = 0xaf0 or 2800 bytes 
of code space. Wow! That's almost 85% of our total ULP code space. But, wait a minute, we never wrote 
any functions with those names. This is where the location information is helpful. It can be a little 
hard to decipher but when you look at the location you can see these functions are coming from 
libgcc and, based on the name, seem to deal with float addition (adddf3.o) and division (divdf3.o).

If we do a Google search for **adddf3.o** the top result is "Soft float library routines". From that 
link to the GCC docs we read:

> The software floating point library is used on machines which do not have hardware support for 
> floating point.

We know that our ULP doesn't support floating point operations so that makes sense. But we don't use 
`float` anywhere in our code. Let's keep reading. Just a little ways down that 
[same page from GCC](https://gcc.gnu.org/onlinedocs/gccint/Soft-float-library-routines.html) we see the 
signature of the `__adddf3` function.

```txt
Runtime Function: double __adddf3 (double a, double b)
```

We store our temperature readings as `double`s! And there is the culprit. Right there on line 23 in 
**main.c**

```c
temp_reading.temp_in_f = (double)loop_count + 1.0/loop_count;
```

We add and divide `double`s so the compiler has to include the software implementation of those  
functions because they aren't natively supported by the ULP processor.

## Refactoring for Code Size

Ok, we've found the problem, but how do we address it? Temperature readings are never nice clean 
integers so we **have** to store them as `double`s right? Well, let's be a little more creative. How 
many decimal places do you need for the temperature? Do you need to know it's 87.3234483234 degrees 
or is 87.3 or 87.32 sufficient? This isn't an engineering problem, it's a specification problem. You 
should have an understanding of the requirements of the project and should know what precision you need 
for temperature. 

Let's assume our device spec says we only need to know the temperature to two decimal places. Another 
way we could represent 87.32 degrees is in hundredths of a degree. 100 hundredths of a degree is the 
same as 1 degree. So 87.32 could be represented as 8732 hundredths of degrees. Now we're back to a nice 
integer value.

And since we don't have the same limitation on `double` in our main application we can just convert it 
back to a `double` there and be on our merry way with the rest of our application logic. Let's refactor 
our code to give this a try and see what happens. 

First, in our ULP code we need to drop all references to `double`. That means changing our `history` array as well as the `temp_reading_t` to both use `int`. Then we need to change how our fake 
temperature reading is calculated. 

> [!NOTE] For the sake of simplicity we're not going to keep the exact same output as before which was 
> loop 2 generated a temp of 2.5, 4 was 4.25, etc. Since it's a contrived 
> result and we're trying to show code optimization, we're now going to output loop 2 being a temp of 
> 2.2, 4 will be 4.4 degrees, etc.

```c
temp_reading.temp_in_f = (100 * loop_count) + (10 * loop_count);
```

Then, back in our C++ main application we can translate the ULP value back to a double when we use 
it. In our case, that's just to print it out.

```cpp
void print_history(){
    temp_reading_t *reading = (temp_reading_t*)&ulp_temp_reading;
    int *history = (int*)&ulp_history;
    ESP_LOGI(TAG, "Current Reading: %f", (double)reading->temp_in_f/100.0);
    for(int i = 0; i < HISTORY_LENGTH; i++){
        if(*(history + i) > 0){
            ESP_LOGI(TAG, "History %d: %f", i, (double)*(history + i)/100.0);
        }
    }
}
```

Flashing and running our code now produces the following output.

```txt
I (287) main: Normal boot, starting ULP program
I (287) main: ULP Loop Count: 0
I (297) main: Current Reading: 0.000000
I (297) main: ULP Stack Pointer LWM: 0xff0
I (5297) main: Current Reading: 2.200000
I (5297) main: History 0: 2.200000
I (10297) main: Current Reading: 4.400000
I (10297) main: History 0: 2.200000
I (10297) main: History 1: 4.400000
I (15297) main: Current Reading: 6.600000
I (15297) main: History 0: 2.200000
I (15307) main: History 1: 4.400000
I (15307) main: History 2: 6.600000
```

Exactly as we expect.

## Inspecting the Results

So how well did our refactor work? Let's take a look in **ulp_main.bin.S**.

```txt
.global ulp_main_bin_length
ulp_main_bin_length:
.long 268
```

🤯🤯🤯. Our ULP program went from 3624 bytes down to 268 bytes. That's about a 92% reduction in code 
size!!! Just from changing a couple of `double`s to `int`s. Inspecting the map file shows that our 
huge libgcc functions are gone.

## Rinse and Repeat

Inspecting the map file for your ULP program will show you exactly how your application is using memory. 
In this lesson we addressed shrinking the `.text` section which is often the biggest culprit. However, 
don't forget our lessons on the Stack Monster. Your `.rodata`, `.data`, and `.bss` sections all reduce 
the amount of memory you have for your stack.

The same principle we applied to optimizing the `.text` section can be used to reduce the size of the 
other sections as well. Be mindful of your types and structures. Can you use a shortcut in ULP code 
and then account for it in the main application? 

Another simple opitimization we could make in our app would be to move the history out of the ULP and 
keep it in the main application. This would significantly reduce the size of our `.bss` section.

Now that you understand how this stuff works and how to inspect things it's really just a matter of 
finding innovative ways to optimize your code. 