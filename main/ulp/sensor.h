#ifndef SENSOR_H
#define SENSOR_H

#define HISTORY_LENGTH 5

typedef enum _STATE{
  READY = 0,
  BEGIN = 1,
  IN_PROGRESS = 2,
} STATE;

typedef struct {
  double temp_in_f;
  STATE state;
} temp_reading_t;

#endif // SENSOR_H