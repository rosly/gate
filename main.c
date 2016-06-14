#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <boards.h> /* include nrf_gpio.h and pca10028.h based on BOARD_* 
                       define from makefile */
#include <sdk_errors.h> /* for ret_code_t and error defines */
#include <nrf_delay.h>

#include "uart_log.h"
#include "sodium.h"
#include "pn532.h" /* for pn532 driver */

#define ERR_CHECK(_err_code) \
  do { \
      if (_err_code != NRF_SUCCESS) { \
          log_printf("Error! %s:%d\r\n", __FILE__, __LINE__); \
          while(1); \
      } \
  } while(0);

#define TAG_TYPE_2_UID_LENGTH 7    // Length of the Tag's UID.
#define TAG_DETECT_TIMEOUT    5000 // Timeout for function which searches for a tag.

static const uint8_t def_mifare_key[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static const uint8_t crypto_pk[crypto_sign_ed25519_PUBLICKEYBYTES] =
   { 0x56, 0x53, 0x21, 0xd2, 0x6f, 0xa3, 0x11, 0x78, 0x13, 0x11, 0xfa, 0xae,
     0x1f, 0xaa, 0xdb, 0x14, 0x58, 0x78, 0x63, 0xa4, 0xa7, 0xe2, 0xf4, 0xb8,
     0x64, 0x5f, 0x7f, 0x52, 0x54, 0xbd, 0x33, 0x9f };
static const uint8_t crypto_sk[crypto_sign_ed25519_SECRETKEYBYTES] =
   { 0x3d, 0xde, 0xd9, 0x53, 0x1c, 0x73, 0xf0, 0x66, 0xc6, 0x03, 0x73, 0x04,
     0x7f, 0xad, 0x33, 0x33, 0x65, 0x25, 0xa5, 0x1d, 0x95, 0x1f, 0x02, 0x49,
     0x46, 0x6d, 0x5d, 0x38, 0xe9, 0x22, 0xed, 0x9c, 0x56, 0x53, 0x21, 0xd2,
     0x6f, 0xa3, 0x11, 0x78, 0x13, 0x11, 0xfa, 0xae, 0x1f, 0xaa, 0xdb, 0x14,
     0x58, 0x78, 0x63, 0xa4, 0xa7, 0xe2, 0xf4, 0xb8, 0x64, 0x5f, 0x7f, 0x52,
     0x54, 0xbd, 0x33, 0x9f };

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

void dump_card(void)
{
    ret_code_t err_code;
    uint8_t uid[TAG_TYPE_2_UID_LENGTH] = { 0 };
    uint8_t uid_length                 = TAG_TYPE_2_UID_LENGTH;
    uint8_t block_data[16];
    size_t i;

    /* Detect a ISO14443A Tag in the field and initiate a communication. This
     * function activates the NFC RF field. If a Tag is present, its UID is
     * stored in the uid buffer. The UID read from the Tag can not be longer
     * than uidLength value passed to the function.  As a result, the uidLength
     * variable returns length of the Tags UID read. */
    err_code = pn532_read_passive_target_id(
       PN532_MIFARE_ISO14443A_BAUD, uid, &uid_length, TAG_DETECT_TIMEOUT);
    if (err_code != NRF_SUCCESS) {
        log_printf("Error while scaning tag %u", err_code);
        return;
    }

    log_printf("Tag detected uid_length=%u", uid_length);
    log_hex("uid:", uid, uid_length);

    if (uid_length == 4) {
        log_printf("Tag is probably Mifare Clasic. Reading...");

        for (i = 0; i < 16; i++) {
            if (pn532_mifareclassic_authenticateblock(
                  uid, uid_length, i, MIFARE_AUTH_KEY_A, def_mifare_key)) {
                log_printf("Error while block auth");
                return;
            }
            
            if (pn532_mifareclassic_readdatablock(i, block_data, sizeof(block_data))) {
                log_printf("Error while block read");
                return;
            }

            log_hex("Data:", block_data, 16);
        }

        log_printf("Dump done.");
    }
}

void tag_scan(void)
{
    size_t i;
    ret_code_t err_code;
    uint8_t uid[TAG_TYPE_2_UID_LENGTH] = { 0 };
    uint8_t uid_length                 = TAG_TYPE_2_UID_LENGTH;
    uint8_t auth_data[4 + 16];
    uint8_t crypto_sig[64];
    uint8_t block;

    /* Detect a ISO14443A Tag in the field and initiate a communication. This
     * function activates the NFC RF field. If a Tag is present, its UID is
     * stored in the uid buffer. The UID read from the Tag can not be longer
     * than uidLength value passed to the function.  As a result, the uidLength
     * variable returns length of the Tags UID read. */
    err_code = pn532_read_passive_target_id(
       PN532_MIFARE_ISO14443A_BAUD, uid, &uid_length, TAG_DETECT_TIMEOUT);
    if (err_code != NRF_SUCCESS) {
        log_printf("Error while scaning tag %u", err_code);
        goto err;
    }

    log_printf("Tag detected uid_length=%u", uid_length);
    log_hex("uid:", uid, uid_length);

    if (uid_length == 4) {
        log_printf("Tag is probably Mifare Clasic. Reading...");

         /* Copy the uid into auth stream */
         memcpy(auth_data, uid, 4);

         /* Read the access rights into auth stream */
         log_printf("Reading access rights ...");
         block = 4;
         if (pn532_mifareclassic_authenticateblock(
               uid, uid_length, block, MIFARE_AUTH_KEY_A, def_mifare_key)) {
             log_printf("Error while unlocking block");
             goto err;
         }
 
         if (pn532_mifareclassic_readdatablock(
               block, &auth_data[4], sizeof(auth_data) - 4)) {
             log_printf("Error while reading user auth block");
             goto err;
         }

         log_hex("Auth data:", auth_data, 4 + 16);
         log_printf("Reading signature ...");
         for (i = 0; i < 4; i++) {
            block = (i + 8) + (i==3?1:0); /* omit 11th block */
            if (pn532_mifareclassic_authenticateblock(
                  uid, uid_length, block, MIFARE_AUTH_KEY_A, def_mifare_key)) {
                log_printf("Error while unlocking block");
                goto err;
            }

            if (pn532_mifareclassic_readdatablock(
                  block, &crypto_sig[i * 16], sizeof(crypto_sig) - (i * 16))) {
                log_printf("Error while reading user auth block");
                goto err;
            }

            log_hex("Auth sig:", &crypto_sig[i*16], 16);
         }

//#define INIT_EMPTY_TAG
#ifdef  INIT_EMPTY_TAG
         for (i = 0; i < sizeof(crypto_sig); i++) {
             if (crypto_sig[i])
                 break;
         }
         if (i == sizeof(crypto_sig)) {
             unsigned long long sig_len;
             int ret;

             log_printf("Card empty. Writing sig");
             ret = crypto_sign_ed25519_detached(
                  crypto_sig, &sig_len, auth_data, sizeof(auth_data), crypto_sk);
             if(ret || (sig_len != 64)) {
                log_printf("Sign error\n");
                goto err;
             }

             for (i = 0; i < 4; i++) {
                block = (i + 8) + (i==3?1:0); /* omit 11th block */
                if (pn532_mifareclassic_authenticateblock(
                      uid, uid_length, block, MIFARE_AUTH_KEY_A, def_mifare_key)) {
                    log_printf("Error while unlocking block");
                    goto err;
                }

                if (pn532_mifareclassic_writedatablock(
                      block, &crypto_sig[i * 16])) {
                    log_printf("Error while writing user auth block");
                    goto err;
                }

                log_hex("Write auth sig:", &crypto_sig[i*16], 16);
             }
         }
#endif

         log_printf("Reading done. Testing sig...");
         if (crypto_sign_ed25519_verify_detached(crypto_sig, auth_data, 16 + 4, crypto_pk)) {
            log_printf("Sig verify error\n\n");
            goto err;
         }
         log_printf("Sig test OK!\n\n");
    }

   return;

err:
   nrf_delay_ms(500);
   return;
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

