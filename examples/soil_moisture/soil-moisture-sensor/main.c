#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libtock-sync/peripherals/adc.h>
#include <libtock/kernel/ipc.h>
#include <libtock/peripherals/gpio.h>
#include <libtock/services/alarm.h>
#include <libtock/tock.h>

#define SAMPLE_INTERVAL_MS 5000

libtock_alarm_repeating_t timer;

struct sensor_client
{
  int pid;
  uint8_t *buffer;
};

struct sensor_client clients[10];
int client_count = 0;

uint32_t reference_voltage;

uint32_t reading_count = 0;

// Called when another app registers to our IPC service.
static void ipc_client_registered(int pid, int len, int buf, __attribute__((unused)) void *ud)
{
  uint8_t *buffer = (uint8_t *)buf;

  // Save the client in our static buffer.
  if (client_count == 10 || len < 4)
    return;
  clients[client_count].pid = pid;
  clients[client_count].buffer = buffer;
  client_count += 1;
}

// Activate the soil moisture sensor and take a reading. Returns the soil
// moisture in tenths of a percent.
static uint32_t take_measurement(uint32_t ref)
{
  uint16_t samples[30];

  // Power on the soil moisture sensor.
  libtock_gpio_set(0);

  // Take 25 ADC readings.
  int err = libtocksync_adc_sample_buffer(0, 25, samples, 30);
  if (err != RETURNCODE_SUCCESS)
  {
    printf("Error sampling ADC: %d\n", err);
    return -1;
  }

  // Calculate the average of the ADC readings.
  uint32_t total = 0;
  for (int i = 0; i < 30; i++)
  {
    total += samples[i];
  }
  uint32_t average = total / 30;
  // Convert from ADC counts to millivolts.
  uint32_t voltage_mv = (average * ref) / ((1 << 16) - 1);

  // Increment our measurement counter.
  reading_count += 1;

  // Calculate the soil moisture percentage.
  //
  // vcc
  // 2.546
  // 1.350
  //
  // gpio
  // 2.093
  // 1.045
  //
  // -0.0954x + 199.71
  // -(9542/10000)x + 1997

  // uint32_t soil = ((2480000 / voltage_mv) - 720) / 100;
  // uint32_t soil = 1797 - ((8111 * voltage_mv) / 10000);
  uint32_t soil = -0.376 * voltage_mv + 1516;

  printf("[Soil Moisture Sensor] Reading #%lu\n", reading_count);
  printf("  voltage %ld.%03ldV\n", voltage_mv / 1000, voltage_mv % 1000);
  printf("  soil: %lu.%lu%%\n\n", soil / 10, soil % 10);

  // Disable the soil moisture sensor.
  libtock_gpio_clear(0);

  return soil;
}

// Timer callback for starting a soil moisture reading.
static void timer_cb(__attribute__((unused)) uint32_t now,
                     __attribute__((unused)) uint32_t scheduled,
                     __attribute__((unused)) void *opaque)
{
  uint32_t moisture_percent = take_measurement(reference_voltage);

  // Copy in to each IPC app's shared buffer.
  for (int i = 0; i < client_count; i++)
  {
    uint32_t *moisture_buf = (uint32_t *)clients[i].buffer;
    moisture_buf[0] = moisture_percent;
    ipc_notify_client(clients[i].pid);
  }
}

int main(void)
{
  int err;
  printf("[Soil Moisture] Sensor App\n");

  // Check if ADC driver exists.
  if (!libtock_adc_exists())
  {
    printf("[Soil Moisture] No ADC driver!\n");
    return -1;
  }

  // Create an IPC service to make sensor readings available to other apps.
  err = ipc_register_service_callback("soil_moisture_sensor", ipc_client_registered, NULL);
  if (err != RETURNCODE_SUCCESS)
  {
    printf("Could not register %i ?\n", err);
  }

  // Record the ADC reference voltage for converting voltage to moisture.
  err = libtock_adc_command_get_reference_voltage(&reference_voltage);
  if (err == RETURNCODE_SUCCESS)
  {
    printf("ADC reference voltage %ld.%03ldV\n", reference_voltage / 1000, reference_voltage % 1000);
  }
  else
  {
    reference_voltage = 3300;
    printf("ADC no reference voltage, assuming 3.3V\n");
  }

  // Set a timer to measure soil moisture periodically.
  libtock_alarm_repeating_every(SAMPLE_INTERVAL_MS, timer_cb, NULL, &timer);

  while (1)
    yield();
}
