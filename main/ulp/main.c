#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"
#include "sensor.h"

extern uint32_t getsp();
extern void stackbuster(int size);

volatile uint32_t loop_count;
volatile uint32_t cur_stack_address;
volatile uint32_t min_stack_address;
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

uint32_t nXOR(int n)
{
    // dummy here helps increase the size of the stack frame from 16 bytes
    // to 32 bytes. It helps speed up the process of self destruction
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

  // Can be used to grow the stack to the point of overwriting
  // critical program sections
  // nXOR(loop_count);

  // Can be used to simulate stack growth with overwrites 
  // shared variables
  // stackbuster(416);

  // if(loop_count % 5 == 0){
  //   ulp_riscv_wakeup_main_processor();
  // }
}