// TODO: description here

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <app_state.h>
#include <console.h>
#include <usb_keyboard_hid.h>
#include <timer.h>
#include <button.h>
#include <led.h>
#include <hmac.h>

#include "base32.h"

int decrypt(const uint8_t*, int, uint8_t*, int);
int hmac(const uint8_t*, int, const uint8_t*, int, uint8_t*, int);


#define NUM_KEYS 4
typedef uint64_t counter_t;

struct hotp_key {
    uint8_t len;
    uint8_t key[64];
    counter_t counter;
};

struct key_storage {
    uint32_t magic;
    struct hotp_key keys[NUM_KEYS];
};

APP_STATE_DECLARE(struct key_storage, keystore);




// Base32-encoded key:
// O775VWOS5TBT6VZ4VNDMB4SOMGKNC2BXOVKCRNDFYJ4WSDLPTELUG24QJCNIT53CDACYE6CDKDQKOXSINABRA5UFOPOU5WIDZJLFBNQ=
//
// To generate HOTP values in Python, use:
// >>> import pyotp, hashlib
// >>> otp = pyotp.HOTP("$THE_ABOVE_BASE32_KEY", digest=hashlib.sha256)
// >>> otp.at(1)
// '571577'
/*const uint8_t key0[64] = {
    0x77, 0xff, 0xda, 0xd9, 0xd2, 0xec, 0xc3, 0x3f,
    0x57, 0x3c, 0xab, 0x46, 0xc0, 0xf2, 0x4e, 0x61,
    0x94, 0xd1, 0x68, 0x37, 0x75, 0x54, 0x28, 0xb4,
    0x65, 0xc2, 0x79, 0x69, 0x0d, 0x6f, 0x99, 0x17,
    0x43, 0x6b, 0x90, 0x48, 0x9a, 0x89, 0xf7, 0x62,
    0x18, 0x05, 0x82, 0x78, 0x43, 0x50, 0xe0, 0xa7,
    0x5e, 0x48, 0x68, 0x03, 0x10, 0x76, 0x85, 0x73,
    0xdd, 0x4e, 0xd9, 0x03, 0xca, 0x56, 0x50, 0xb6
};*/

// Select how many digits the key is
int key_digits[NUM_KEYS] = { 6, 6, 6, 6 };



static void hmac_upcall(__attribute__ ((unused)) int   arg0,
                        __attribute__ ((unused)) int   arg1,
			__attribute__ ((unused)) int   arg2,
			                         void* done_flag) {
    *((bool *) done_flag) = true;
}

int hmac(const uint8_t* key, int key_len, const uint8_t* data, int data_len, uint8_t* output_buffer, int output_buffer_len) {
    int ret;
    bool hmac_done = false;

    ret = hmac_set_callback(hmac_upcall, &hmac_done);
    if (ret < 0) {
	goto done;
    }

    ret = hmac_set_key_buffer(key, key_len);
    if (ret < 0) {
	goto deregister_upcall;
    }

    ret = hmac_set_dest_buffer(output_buffer, output_buffer_len);
    if (ret < 0) {
	goto unallow_key_buffer;
    }

    ret = hmac_set_data_buffer(data, data_len);
    if (ret < 0) {
	goto unallow_dest_buffer;
    }

    ret = hmac_set_algorithm(TOCK_HMAC_ALG_SHA256);
    if (ret < 0) {
	goto unallow_data_buffer;
    }

    ret = hmac_run();
    if (ret < 0) {
        printf("HMAC failure: %d\r\n", ret);
        goto unallow_data_buffer;
    }

    yield_for(&hmac_done);

 unallow_data_buffer:
    hmac_set_data_buffer(NULL, 0);

 unallow_dest_buffer:
    hmac_set_dest_buffer(NULL, 0);

 unallow_key_buffer:
    hmac_set_key_buffer(NULL, 0);

 deregister_upcall:
    hmac_set_callback(NULL, NULL);

 done:
    return ret;
}

int decrypt(const uint8_t* cipher, int cipherlen, uint8_t* plaintext, int plaintext_capacity) {
  int copylen = cipherlen;
  if (plaintext_capacity < cipherlen) {
    copylen = plaintext_capacity;
  }
  memcpy(plaintext, cipher, copylen);
  return copylen;
}



// --- Button Handling ---

// Global to keep track of most recently pressed button
int pressed_btn_num;

// Callback for button presses.
//   num: The index of the button associated with the callback
//   val: 1 if pressed, 0 if depressed
static void button_upcall(int num,
                          int val,
                          __attribute__ ((unused)) int arg2,
                         void *ud) {
  if (val == 1) {
    pressed_btn_num = num;
    *((bool*)ud) = true;
  }
}

// Initializes interrupts for all buttons on the board
static int initialize_buttons(bool* flag_pointer) {
  // Enable button interrupts
  int err = button_subscribe(button_upcall, (void*)flag_pointer);
  if (err != RETURNCODE_SUCCESS) {
    return err;
  }

  // Enable interrupts on each button.
  int count = 0;
  err = button_count(&count);
  if (err != RETURNCODE_SUCCESS) {
    return err;
  }

  for (int i=0; i < count; i++) {
    button_enable_interrupt(i);
  }

  return RETURNCODE_SUCCESS;
}


// --- App State Handling ---



// --- Main Loop ---

// Performs initialization and interactivity.
int main(void) {
  delay_ms(1000);
  printf("Tock HOTP App Started. Usage:\r\n"
      "* Press a button to get the next HOTP code for that slot.\r\n"
      "* Hold a button to enter a new HOTP secret for that slot.\r\n");

  // Recover state from flash if it exists
  int ret;
  ret = app_state_load_sync();
  if (ret != 0) printf("ERROR(%i): Could not read the flash region.\r\n", ret);
  else printf("Flash read\r\n");
  // Check that the magic value is as expected.
  if (keystore.magic != 0xdeadbeef) {
    keystore.magic = 0xdeadbeef;
    for (int i = 0; i < NUM_KEYS; i++) {
      keystore.keys[i].len = 0;
    }
    ret = app_state_save_sync();
    if (ret != 0) printf("ERROR(%i): Could not write back to flash.\r\n", ret);
    else printf("Initialized state\r\n");
  }

  // Initialize buttons
  bool button_pressed = false;
  if (initialize_buttons(&button_pressed) != RETURNCODE_SUCCESS) {
    printf("ERROR initializing buttons: \r\n");
    return 1;
  }

  // Main loop. Waits for button presses
  while (true) {
    // Yield until a button is pressed
    button_pressed = false;
    yield_for(&button_pressed);
    int btn_num = pressed_btn_num;

    // Delay and check if button is still pressed, signalling a "hold"
    delay_ms(500);
    int new_val = 0;
    button_read(btn_num, &new_val);

    // Handle long presses (program new secret)
    if (new_val) {

      // Request user input
      led_on(btn_num);
      printf("Program a new key in slot %d\r\n", btn_num);
      printf("(hit enter without typing to cancel) >  ");

      // Read key values from user
      // TODO: sure would be nice to clear all previous input before starting this
      uint8_t newkey[128];
      int i = 0;
      while (i < 127) {
        // read next character
        char c = getch();

        // break on enter
        if (c == '\n') {
          break;
        }

        // only record alphanumeric characters
        if (isalnum(c)) {
          newkey[i] = c;
          i++;

          // echo input to user
          putnstr(&c, 1);
        }
      }

      // Finished. Append null terminator and echo newline
      newkey[i] = '\0';
      putnstr("\r\n", 1);

      // Handle early exits
      if (newkey[0] == '\0') {
        printf("Aborted\r\n");
        led_off(btn_num);
        continue;
      }

      // Decode and save secret to flash
      keystore.keys[btn_num].len = base32_decode(newkey, keystore.keys[btn_num].key, 64);
      keystore.keys[btn_num].counter = 0;
      ret = app_state_save_sync();
      if (ret != 0) {
        printf("ERROR(%i): Could not write back to flash.\r\n", ret);
      }

      // Completed!
      printf("Programmed \"%s\" to slot %d\r\n", newkey, btn_num);
      led_off(btn_num);

      // Handle short presses on already configured keys (output next code)
    } else if (btn_num < NUM_KEYS && keystore.keys[btn_num].len > 0) {
      led_on(btn_num);

      // Decrypt the key:
      uint8_t key[64];
      int keylen = decrypt(keystore.keys[btn_num].key, keystore.keys[btn_num].len, key, 64);

      // Generate the HMAC'ed data from the "moving factor" (timestamp in TOTP,
      // counter in HOTP), shuffled in a specific way:
      uint8_t moving_factor[sizeof(counter_t)];
      for (size_t i = 0; i < sizeof(counter_t); i++) {
        moving_factor[i] = (keystore.keys[btn_num].counter >> ((sizeof(counter_t) - i - 1) * 8)) & 0xFF;
      }

      // Perform the HMAC operation
      const uint8_t HMAC_OUTPUT_BUF_LEN = 32;
      uint8_t hmac_output_buf[HMAC_OUTPUT_BUF_LEN];
      hmac(key, keylen, moving_factor, sizeof(counter_t), hmac_output_buf, HMAC_OUTPUT_BUF_LEN);

      // Increment the counter and save to flash
      keystore.keys[btn_num].counter++;
      ret = app_state_save_sync();
      if (ret != 0) {
        printf("ERROR(%i): Could not write back to flash.\r\n", ret);
      }

      // Get output value
      uint8_t offset = hmac_output_buf[HMAC_OUTPUT_BUF_LEN - 1] & 0x0f;
      uint32_t S = (((hmac_output_buf[offset] & 0x7f) << 24)
          | ((hmac_output_buf[offset + 1] & 0xff) << 16)
          | ((hmac_output_buf[offset + 2] & 0xff) << 8)
          | ((hmac_output_buf[offset + 3] & 0xff)));

      // Limit output to correct number of digits
      switch (key_digits[btn_num]) {
        case 6:
          S = S % 1000000;
          break;
        case 7:
          S = S % 10000000;
          break;
        case 8:
          S = S % 100000000;
          break;
        default:
          printf("ERROR: invalid HMAC output\r\n");
          S = 0;
          break;
      }

      // Write the value to the USB keyboard.
      char hotp_format_buffer[16];
      snprintf(hotp_format_buffer, 16, "%.*ld", key_digits[btn_num], S);
      uint8_t keyboard_buffer[64];
      ret = usb_keyboard_hid_send_string_sync(keyboard_buffer, hotp_format_buffer, 16);
      if (ret < 0) {
        printf("ERROR sending string with USB keyboard HID: %i\r\n", ret);
      } else {
        printf("Typed \"%s\" on the USB HID the keyboard\r\n", hotp_format_buffer);
      }

      // Complete
      led_off(btn_num);

      // Error for short press on a non-configured key
    } else if (keystore.keys[btn_num].len == 0) {
      printf("HOTP / TOTP slot %d not yet configured.\r\n", btn_num);
    }
  }

  return 0;
}

