#include <libtock/kernel/ipc.h>
#include <libtock/tock.h>
#include "stdio.h"

struct rot13_buf
{
  int8_t length;
  char buf[31];
};

static void rot13_callback(int pid, int len, int buf, __attribute__((unused)) void *ud)
{
  printf("[rot13_service] Received IPC notification! PID: %d, len: %d\n", pid, len);

  struct rot13_buf *rb = (struct rot13_buf *)buf;
  printf("[rot13_service] Received data: %.*s\n", rb->length, rb->buf);

  int length = rb->length;
  if (length > len - 1)
  {
    length = len - 1;
  }
  for (int i = 0; i < length; ++i)
  {
    if (rb->buf[i] >= 'a' && rb->buf[i] <= 'z')
    {
      rb->buf[i] = (((rb->buf[i] - 'a') + 13) % 26) + 'a';
    }
    else if (rb->buf[i] >= 'A' && rb->buf[i] <= 'Z')
    {
      rb->buf[i] = (((rb->buf[i] - 'A') + 13) % 26) + 'A';
    }
  }

  printf("[rot13_service] Notifying client PID: %d\n", pid);
  ipc_notify_client(pid);
}

int main(void)
{
  int err = ipc_register_service_callback("org.tockos.examples.rot13", rot13_callback,
                                          NULL);
  if (err != RETURNCODE_SUCCESS)
  {
    printf("[rot13_service] Failed to register IPC service! Error code: %d\n", err);
    return -1;
  }

  printf("[rot13_service] IPC service registered successfully!\n");

  while (1)
  {
    yield();
  }
}
