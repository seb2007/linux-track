#ifndef LTLIB__H
#define LTLIB__H

#include <stdbool.h>
#include <stdint.h>
#include "linuxtrack.h"

typedef enum{RUN_CMD, PAUSE_CMD, STOP_CMD, NOP_CMD} ltr_cmd;
struct ltr_comm{
  uint8_t cmd;
  uint8_t recenter;
  uint8_t state;
  float heading, pitch, roll;
  float tx, ty, tz;
  uint32_t counter;
  uint8_t dead_man_button;
};


#endif
