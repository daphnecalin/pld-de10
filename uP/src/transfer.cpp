#include "transfer.h"
#include <stdint.h>
// flag for spi_receive to start transfer
volatile uint8_t data_ready_flag = 0;

// ISR that goes off when gpio goes high
void transfer::data_ready(void) {
  // change data ready flag
  data_ready_flag = 1;
}

packet::Frame transfer::spi_receive(void) {
  // poll data ready flag then initiate transmission
  while (!data_ready_flag) {
  }

  packet::Frame f;
  // do transmission stuff and fill frame

  return f;
}
