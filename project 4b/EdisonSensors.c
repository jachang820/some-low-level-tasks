/*
 * NAME: Jonathan Chang
 * EMAIL: j.a.chang820@gmail.com
 * ID: 104853981
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "mraa.h"


/* input var */
float period;
enum Scale {FAHRENHEIT, CELSIUS} scale;

/* aio */
const int TEMPERATURE_ANALOG_PIN = 0;
mraa_aio_context temp_sensor;
float fahrenheit;
float celsius;

/* gpio */
const int BUTTON_DIGITAL_PIN = 3;
mraa_gpio_context button;
int pressed;


void init_temp_sensor(void) {
  temp_sensor = mraa_aio_init(TEMPERATURE_ANALOG_PIN);
  if (temp_sensor == NULL) {
    fprintf(stderr, "Failed to initialize temperature sensor.\r\n");
    exit(1);
  }
}

void read_temp_sensor(void) {
  const int B = 4275; /* B value of thermistor */
  const int R0 = 100000; /* resistance */
  int read = mraa_aio_read(temp_sensor);
  if (read == -1) {
    fprintf(stderr, "Temperature read failed.\r\n");
    exit(1);
  }
  float R = 1023.0/read - 1.0;
  celsius = (1.0 / ((logf(R) / B) + (1 / 298.15))) - 273.15;
  fahrenheit = celsius * 1.8 + 32;
}

void deinit_temp_sensor(void) {
  mraa_result_t result = mraa_aio_close(temp_sensor);
  if (result != MRAA_SUCCESS) {
    mraa_result_print(result);
  }
}

void init_button(void) {
  button = mraa_gpio_init(BUTTON_DIGITAL_PIN);
  mraa_gpio_dir(button, MRAA_GPIO_IN);
  if (button == NULL) {
    fprintf(stderr, "Failed to initialize button.\r\n");
    exit(1);
  }
}

int read_button(void) {
  int read = mraa_gpio_read(button);
  if (read == 1) pressed = 1;
  else if (read == -1) {
    fprintf(stderr, "Button fatal error.\r\n");
    exit(1);
  }
  return pressed;
}

void deinit_button(void) {
  mraa_result_t result = mraa_gpio_close(button);
  if (result != MRAA_SUCCESS) {
    mraa_result_print(result);
  }
}

void set_period(float sample_period) {
  period = sample_period;
}

void set_scale(const char temp_scale) {
  if (temp_scale == 'F' || temp_scale == 'f') {
    scale = FAHRENHEIT;
  }
  else if (temp_scale == 'C' || temp_scale == 'c') {
    scale = CELSIUS;
  }
  else { 
    fprintf(stderr, "Invalid scale argument.\r\n");
    exit(1);
  }
}

float get_period(void) {
  return period;
}

float get_temperature(void) {
  if (scale == FAHRENHEIT)
    return fahrenheit;
  else
    return celsius;
}
