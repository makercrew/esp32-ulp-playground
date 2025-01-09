#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"

volatile uint32_t loop_count;

int main (void)
{
  loop_count++;
  if(loop_count % 5 == 0){
    ulp_riscv_wakeup_main_processor();
  }
}