// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

// #include <u8g2-tock.h>
// #include <u8g2.h>

#include "sensor_service.h"

// #define max(a, b) ((a) > (b) ? (a) : (b))

// u8g2_t u8g2;

// int display_width;
// int display_height;

// size_t svc_num = 0;

// char ipc_buf[64] __attribute__((aligned(64)));

// static void show_moisture(uint32_t reading)
// {
//   u8g2_ClearBuffer(&u8g2);

//   char buf[30];
//   uint32_t whole = reading / 10;
//   uint32_t decimal = reading % 10;
//   snprintf(buf, 30, "Soil Moisture: %lu.%01lu%%", whole, decimal);

//   int strwidth = u8g2_GetUTF8Width(&u8g2, buf);

//   int y_center = display_height / 2;
//   int x = max((display_width / 2) - (strwidth / 2), 0);

//   u8g2_DrawStr(&u8g2, x, y_center, buf);
//   u8g2_SendBuffer(&u8g2);
// }

// static void ipc_callback(uint32_t moisture_reading)
// {
//   show_moisture(moisture_reading);
// }

// int main(void)
// {
//   int err;

//   printf("[Soil Moisture Data] Distribute data\n");

//   err = u8g2_tock_init(&u8g2);
//   if (err)
//   {
//     printf("Could not init screen\n");
//     return -2;
//   }

//   display_width = u8g2_GetDisplayWidth(&u8g2);
//   display_height = u8g2_GetDisplayHeight(&u8g2);

//   u8g2_SetFont(&u8g2, u8g2_font_helvR08_tf);
//   u8g2_SetFontPosCenter(&u8g2);

//   u8g2_ClearBuffer(&u8g2);
//   u8g2_SendBuffer(&u8g2);

//   err = connect_to_sensor_service(ipc_buf, ipc_callback);
//   if (err != RETURNCODE_SUCCESS)
//     return -1;

//   while (1)
//     yield();
// }

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libtock/kernel/ipc.h>
#include <libtock/tock.h>

#include <u8g2-tock.h>
#include <u8g2.h>

#define max(a, b) ((a) > (b) ? (a) : (b))

u8g2_t u8g2;
int display_width;
int display_height;

size_t service_id = 0;

// // 显示湿度和电压数据
// static void show_moisture(uint32_t moisture, uint32_t voltage_mv)
// {
//   u8g2_ClearBuffer(&u8g2);

//   char buf[30];

//   // 显示湿度
//   snprintf(buf, 30, "Soil: %lu.%01lu%%", moisture / 10, moisture % 10);
//   int strwidth = u8g2_GetUTF8Width(&u8g2, buf);
//   int y_center = display_height / 2 - 10;
//   int x = max((display_width / 2) - (strwidth / 2), 0);
//   u8g2_DrawStr(&u8g2, x, y_center, buf);

//   // 显示电压
//   snprintf(buf, 30, "Voltage: %lu.%03luV", voltage_mv / 1000, voltage_mv % 1000);
//   strwidth = u8g2_GetUTF8Width(&u8g2, buf);
//   y_center = display_height / 2 + 10;
//   x = max((display_width / 2) - (strwidth / 2), 0);
//   u8g2_DrawStr(&u8g2, x, y_center, buf);

//   u8g2_SendBuffer(&u8g2);
// }

// // IPC 回调
// static void ipc_callback(uint32_t *data)
// {
//   uint32_t moisture = data[0];
//   uint32_t voltage_mv = data[1];
//   show_moisture(moisture);
// }

static void show_moisture(uint32_t reading)
{
  u8g2_ClearBuffer(&u8g2);

  char buf[30];
  uint32_t whole = reading / 10;
  uint32_t decimal = reading % 10;
  snprintf(buf, 30, "Soil Moisture: %lu.%01lu%%", whole, decimal);

  int strwidth = u8g2_GetUTF8Width(&u8g2, buf);

  int y_center = display_height / 2;
  int x = max((display_width / 2) - (strwidth / 2), 0);

  u8g2_DrawStr(&u8g2, x, y_center, buf);
  u8g2_SendBuffer(&u8g2);
}

static void ipc_callback(uint32_t moisture_reading)
{
  show_moisture(moisture_reading);
}

int main(void)
{
  int err;
  printf("[Display App] Running...\n");

  // 初始化屏幕
  err = u8g2_tock_init(&u8g2);
  if (err)
  {
    printf("Could not init screen\n");
    return -1;
  }

  display_width = u8g2_GetDisplayWidth(&u8g2);
  display_height = u8g2_GetDisplayHeight(&u8g2);
  u8g2_SetFont(&u8g2, u8g2_font_helvR08_tf);
  u8g2_SetFontPosCenter(&u8g2);

  // 连接 IPC 服务
  char ipc_buf[64] __attribute__((aligned(64)));
  // err = ipc_discover(ipc_buf, &service_id);
  // if (err != RETURNCODE_SUCCESS)
  // {
  //   printf("IPC discover service not found\n");
  //   return -1;
  // }

  err = connect_to_sensor_service(ipc_buf, ipc_callback);
  if (err != RETURNCODE_SUCCESS)
  {
    printf("IPC service not found\n");
    return -1;
  }

  while (1)
  {
    // ipc_notify_service(service_id);
    yield();
  }
}
