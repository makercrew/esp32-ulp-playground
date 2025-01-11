# Lesson 7 - The Stack Monster 🧟 

## What We'll Cover
- How stack growth can be catostrophic
- That's basically it, the stack can truly kill you

To see all of the changes made and working simply run `git checkout lesson-7-end`. 

To follow along, run `git checkout lesson-7` and follow the instructions below.

## The Stack Is Not Your Friend 
(🎵🎵 ominous music still playing from Lesson 6 🎵🎵)

I don't want to overplay the dangers of the stack. In reality, this chapter could be summarized as 
"Know how much stack space you have and make absolutely sure you won't use a byte more. If you do, very 
bad things will start to happen very quickly."

The inspiration for this entire series came about as I was working on some ULP code for a client. We 
were seeing some strange behavior. ULP code that was working before was now behaving sporadically. It 
was the worst kind of bug. Inconsistent and hard to trace. 

After an obscene number of hours of research I landed upon a very simple, yet disturbing truth.

> [!CAUTION]
> The ULP stack can grow and begin to eat the ULP program itself!!! 🧟

Put another way, if you don't deterministically control your stack size, it will first start overwritting your `.bss` section variables, then your `.data` variables, then your `.rodata` variables 
and will even start overwriting the ULP code itself in the `.text` section.

This isn't the main application domain where you will get a stack overflow panic and reset. Stack growth 
can put your ULP program into an undefined state but continue to run. **You will 
have NO idea things are wrong**.

## A Simple Proof
Currently our ULP application is recording fake temperature readings and we are displaying them from 
the main application. It's a contrived example but it's exactly the type of scenario in which you would 
use the ULP. A low power application that has the ULP reading a sensor and only waking up the main 
processor when something important happens like getting a temperature outside of an acceptable range. 

Let's run a simple test to see if we can get our stack to overwrite a portion of our history memory. 
Now, we could try to carefully craft a recursion scenario that just happens to land on that memory 
region or...we can simulate it. Just to show it's possible.

To prove the stack can bust your data 
I've created another function in our **sp.S** file from Lesson 6. It's called `stackbuster`.
You can add it below the `getsp` function in the ".S" file.

```gas
.global stackbuster
.type stackbuster, @function

stackbuster:
    neg t1, a0          # Store the negative of a0 in t1. t1 = -size
    add sp, sp, t1      # Subtract "size" bytes from the stack pointer

    li t6, 0x4058ff5c   # Put random value into t6 register
    sw t6, 4(sp)        # Save t6 register to stack. Will overwrite whatever is there

    add sp, sp, a0      # Restore the stack pointer
    ret                 # Return
```

Don't worry about understanding the assembly here. The comments explain what's going on.
I've carefully crafted a value to give us a bad 
temperature reading that was never actually taken. The only reason I picked a specific value was to 
make it easy to see in our output. The EXACT same thing can happen by accident if your stack grows 
into your shared variable memory region. 

Let's get a baseline of what our output looks like before we make any changes to our `main` function.

```txt
I (287) main: Normal boot, starting ULP program
I (287) main: ULP Loop Count: 0
I (297) main: Current Reading: 0.000000
I (297) main: ULP Stack Pointer LWM: 0xff0
I (5297) main: Current Reading: 2.500000
I (5297) main: History 0: 2.500000
I (10297) main: Current Reading: 4.250000
I (10297) main: History 0: 2.500000
I (10297) main: History 1: 4.250000
I (15297) main: Current Reading: 6.166667
I (15297) main: History 0: 2.500000
I (15297) main: History 1: 4.250000
I (15307) main: History 2: 6.166667
```

We get an initial reading of 0 before the first actual reading happens. Then we proceed to get 
determinstic readings which expand the history entries.

The only change we're going to make to our **main.c** program is to call `stackbuster`. We have to 
declare it as `extern` at the top of our file and then just add a call to it in our `main` function.

```c
extern void stackbuster(int size);
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

  // Bust our variables
  stackbuster(100);
}
```

The value of `100` here is just a placeholder. We still need to calculate the correct offset to bust 
our history array. We need the code in there so when we build the layout is as close to final as 
possible. To get into a clean slate let's rebuild our entire project.

```sh
idf.py fullclean
idf.py build
```

Now, let's open our ULP map file to get some information. 

```sh
code ./build/esp-idf/main/ulp_main/ulp_main.map
```

Since our `temp_reading` struct is unitialized it will go in the `.bss` section of memory. We see this 
in the map file as:

```txt
.bss.history   0x00000e30       0x28 CMakeFiles...
               0x00000e30                history
```

The `history` shared variable resides at address 0xe30 and is 0x28(40) bytes in size. We know from our 
output that, in `main`, the stack pointer is at 0xff0. To get to address 0xe30 we have to grow the 
stack by 0xff0-0xe30=0x1c0=448 bytes. But that would put us right at the beginning. Let's go a little 
bit less (in 16 byte increments) to put us somewhere in the middle of our history array. I'm going to 
use 416 bytes. 

```c
stackbuster(416);
```

Adding that to my stackbuster call I'm ready to build and flash again.

Here is the output.

```txt
I (288) main: Normal boot, starting ULP program
I (288) main: ULP Loop Count: 0
I (298) main: Current Reading: 0.000000
I (298) main: History 4: 99.989990
I (298) main: ULP Stack Pointer LWM: 0xff0
I (5308) main: Current Reading: 2.500000
I (5308) main: History 0: 2.500000
I (5308) main: History 4: 99.989990
I (10308) main: Current Reading: 4.250000
I (10308) main: History 0: 2.500000
I (10308) main: History 1: 4.250000
I (10308) main: History 4: 99.989990
```

Uh oh! Before a single reading is taken our history already thinks it has a reading at index 4 of 
99.989990.
You can then see new readings get recorded but the bad record remains. As far as our main app knows, 
there is absolutely no reason to question it. After all, it's in the ULP's history array.

Hopefully it is very obvious at this point what kind of catastrophic side effects can happen if your 
stack overruns your shared variables. 

Just in case, I'm going to spell it out. Let's pretend our hypothetical temperature device is connected 
to the fire suppression system of a datacenter. Any temperature reading over 90 degrees (ok, I know, not 
very hot, humor me) activates the system and deploys fire supression measures (aka water) in the room.

Your stack overflow just cost somebody a lot of money in ruined hardware and potential data loss.

If you leave a comment saying "Well, actually, many new datacenters use gaseous clean agent systems that 
can extinguish fires without damaging equipment" so help me I will call down the wrath of a thousand 
burning suns upon you. 

So, yeah, beware the stack monster. 🧟 🧟 🧟