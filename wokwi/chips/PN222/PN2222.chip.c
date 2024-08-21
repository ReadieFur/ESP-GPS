#include "wokwi-api.h"
#include <stdlib.h>
#include <stdio.h>

// Chip data.
typedef struct {
  pin_t E;
  pin_t B;
  pin_t C;
  int timer;
  timer_t timer_id;
  uint32_t analog_base;
  uint32_t collector_pwm;
} chip_data_t;

// Timer callback, for analog base.
void chip_timer_callback(void *data)
{
  chip_data_t *chip = (chip_data_t*)data;

  float volts = pin_adc_read(chip->B);

  if (attr_read(chip->collector_pwm))
  {
    float offset = 220 - (volts * 44.0);
    float interval = (volts * 400.0) - offset;

    if (volts > 0.55 && !pin_read(chip->E))
    {
      pin_write(chip->C, chip->timer > interval);
    }
    else
    {
      pin_write(chip->C, 1);
    }

    chip->timer = chip->timer < 2000 ? chip->timer + 10 : 0;
  }
  else
  {
    pin_write(chip->C, volts < 0.55);
  }
}

// Called when base pin changes state, for digital base.
void chip_pin_change(void *user_data, pin_t pin, uint32_t value)
{
  chip_data_t *chip = (chip_data_t*)user_data;
  
  if (value && !pin_read(chip->E))
  {
    pin_mode(chip->C, OUTPUT);
    pin_write(chip->C, LOW);
  }
  else
  {
    pin_mode(chip->C, INPUT);
  }
}

// Initializes the chip.
void chip_init() 
{
  chip_data_t *chip = (chip_data_t*)malloc(sizeof(chip_data_t));

  chip->analog_base = attr_init("analog_base", 0);
  chip->collector_pwm = attr_init("collector_pwm", 0);

  chip->E = pin_init("E", INPUT);
  chip->C = pin_init("C", OUTPUT_HIGH);

  if (attr_read(chip->analog_base))
  {
    chip->B = pin_init("B", ANALOG);
  }
  else
  {
    chip->B = pin_init("B", INPUT);
  }

  const timer_config_t config = 
  {
    .callback = chip_timer_callback,
    .user_data = chip,
  };

  chip->timer_id = timer_init(&config);

  if (attr_read(chip->analog_base))
  {
    if (attr_read(chip->collector_pwm))
    {
      timer_start(chip->timer_id, 10, true);
    }
    else
    {
      timer_start(chip->timer_id, 1000, true);
    }
  }
  else
  {
    const pin_watch_config_t watch_config = {
      .edge = BOTH,
      .pin_change = chip_pin_change,
      .user_data = chip,
    };

    pin_watch(chip->B, &watch_config);
  }
}