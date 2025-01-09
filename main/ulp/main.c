#include "ulp_riscv.h"
#include "ulp_riscv_utils.h"

volatile uint32_t loop_count;

int main (void)
{
  loop_count++;
}