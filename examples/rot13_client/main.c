#include <stdio.h>
#include <string.h>

#include <libtock-sync/services/alarm.h>
#include <libtock/kernel/ipc.h>

size_t rot13_svc_num = 0;

char buf[64] __attribute__((aligned(64)));

struct rot13_buf
{
  int8_t length;
  char buf[31];
};

static void rot13_callback(__attribute__((unused)) int pid,
                           __attribute__((unused)) int len,
                           __attribute__((unused)) int arg2, void *ud)
{
  struct rot13_buf *rb = (struct rot13_buf *)ud;
  printf("[rot13_client] Callback triggered! Data: %d: %.*s\n", rb->length, rb->length, rb->buf);
  libtocksync_alarm_delay_ms(500);
  ipc_notify_service(rot13_svc_num);
}

int main(void)
{
  int err;
  err = ipc_discover("org.tockos.examples.rot13", &rot13_svc_num);
  if (err < 0)
  {
    printf("No rot13 service\n");
    return -1;
  }

  struct rot13_buf *rb = (struct rot13_buf *)buf;
  ipc_register_client_callback(rot13_svc_num, rot13_callback, rb);

  rb->length = snprintf(rb->buf, sizeof(rb->buf), "Hello World!");
  ipc_share(rot13_svc_num, rb, 64);

  printf("[rot13_client] Sending IPC request...\n");

  int ret = ipc_notify_service(rot13_svc_num);
  printf("[rot13_client] ipc_notify_service ret = %d\n", ret);

  while (1)
  {
    printf("[rot13_client] Waiting for IPC...\n");
    yield();
  }
}
