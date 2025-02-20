#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libtock-sync/display/text_screen.h>
#include <libtock-sync/services/alarm.h>
#include <libtock/tock.h>

int main(void)
{
  printf("[TEXT_SCREEN] Test Screen Display\n");

  returncode_t ret;

  uint8_t *buffer = calloc(15, 1);

  ret = libtocksync_text_screen_display_on();

  printf("ret return code is: %d\n", ret);

  if (ret == RETURNCODE_SUCCESS)
  {
    ret = libtocksync_text_screen_set_cursor(0, 0);
    memcpy(buffer, "Hello!", 6);
    ret = libtocksync_text_screen_write(buffer, 15, 6);

    printf("buffer is:\n");
    for (int i = 0; i < 15; i++)
    {
      printf("0x%02X ", buffer[i]);
    }
    printf("\n");

    libtocksync_alarm_delay_ms(5000);

    ret = libtocksync_text_screen_set_cursor(0, 1);
    memcpy(buffer, "Goodbyee!", 9);
    ret = libtocksync_text_screen_write(buffer, 15, 9);

    libtocksync_alarm_delay_ms(2000);
    ret = libtocksync_text_screen_clear();
  }

  printf("END TEXT_SCREEN\n");

  return 0;
}
