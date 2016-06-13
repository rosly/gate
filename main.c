#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include <boards.h> /* include nrf_gpio.h and pca10028.h based on BOARD_* 
                       define from makefile */
#include <sdk_errors.h> /* for ret_code_t and error defines */
#include <pn532.h> /* for pn532 driver */

#include "uart_log.h"

#define ERR_CHECK(_err_code) \
  do { \
      if (_err_code != NRF_SUCCESS) { \
          log_printf("Error! %s:%d\r\n", __FILE__, __LINE__); \
          while(1); \
      } \
  } while(0);

#define TAG_TYPE_2_UID_LENGTH 7    // Length of the Tag's UID.
#define TAG_DETECT_TIMEOUT    5000 // Timeout for function which searches for a tag.

void board_setup(void)
{
   ret_code_t err_code;

   LEDS_CONFIGURE(LEDS_MASK);
   LEDS_OFF(LEDS_MASK);

   log_init();

   log_printf("Initializing pn532");
   err_code = pn532_init(false);
   ERR_CHECK(err_code);
}

void tag_scan(void)
{
    ret_code_t err_code;
    uint8_t uid[TAG_TYPE_2_UID_LENGTH] = { 0 };
    uint8_t uid_length                 = TAG_TYPE_2_UID_LENGTH;
    uint8_t block_data[16];
    size_t i;
    static const uint8_t mifare_key[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };


    // Detect a ISO14443A Tag in the field and initiate a communication. This function activates
    // the NFC RF field. If a Tag is present, its UID is stored in the uid buffer. The
    // UID read from the Tag can not be longer than uidLength value passed to the function.
    // As a result, the uidLength variable returns length of the Tags UID read.
    err_code = pn532_read_passive_target_id(
       PN532_MIFARE_ISO14443A_BAUD, uid, &uid_length, TAG_DETECT_TIMEOUT);
    if (err_code != NRF_SUCCESS) {
        log_printf("Error while scaning tag %u", err_code);
        return;
    }

    log_printf("Tag detected uid_length=%u", uid_length);
    log_hex("uid:", uid, uid_length);

    if (uid_length == 4) {
        log_printf("Tag is probably Mifare Clasic");
        for (i = 0; i < 16; i++) {
            if (pn532_mifareclassic_first_block(i)) {
               if (pn532_mifareclassic_authenticateblock(uid, uid_length, i, 
                                                         MIFARE_AUTH_KEY_A, mifare_key)) {
                   log_printf("Error while block auth");
                   return;
               }
            }
            
            if (pn532_mifareclassic_readdatablock(i, block_data, sizeof(block_data))) {
                log_printf("Error while block read");
                return;
            }

            log_hex("Data:", block_data, 16);
        }
    }
}

int main(void)
{
    board_setup();

    LEDS_ON(BSP_LED_0_MASK);
    log_printf("Main loop");

    for (;;) {
        LEDS_OFF(BSP_LED_1_MASK);
        tag_scan();
        LEDS_ON(BSP_LED_1_MASK);
    }
}

