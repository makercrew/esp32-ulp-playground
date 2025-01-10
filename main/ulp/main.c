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