#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

#include <boards.h> /* include nrf_gpio.h and pca10028.h based on BOARD_*
                       define from makefile */
#include <sdk_errors.h> /* for ret_code_t and error defines */
#include <nrf_delay.h>

#include "log_debug.h"
#include "sodium.h"
#include "pn532.h" /* for pn532 driver */

#define ERR_CHECK(_err_code) \
  do { \
      if (_err_code != NRF_SUCCESS) { \
          log_debug_printf("Error! %s:%d\r\n", __FILE__, __LINE__); \
          while(1); \
      } \
  } while(0);

#define TAG_TYPE_2_UID_LENGTH 7    // Length of the Tag's UID.
#define TAG_DETECT_TIMEOUT    5000 // Timeout for function which searches for a tag.

static const uint8_t def_mifare_key[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
static const uint8_t gate_mifare_key[] = { 0x56, 0x53, 0x21, 0xd2, 0x6f, 0xa3 };
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

   log_debug_init();

   log_debug_printf("Initializing pn532");
   err_code = pn532_init(false);
   ERR_CHECK(err_code);
   log_debug_printf("Initialization DONE");
}

void dump_card(void)
{
    ret_code_t err_code;
    uint8_t uid[TAG_TYPE_2_UID_LENGTH] = { 0 };
    uint8_t uid_length                 = TAG_TYPE_2_UID_LENGTH;
    uint8_t block_data[16];
    const uint8_t *auth_key;
    char trailer[12];
    size_t i;

    /* Detect a ISO14443A Tag in the field and initiate a communication. This
     * function activates the NFC RF field. If a Tag is present, its UID is
     * stored in the uid buffer. The UID read from the Tag can not be longer
     * than uidLength value passed to the function.  As a result, the uidLength
     * variable returns length of the Tags UID read. */
    err_code = pn532_read_passive_target_id(
       PN532_MIFARE_ISO14443A_BAUD, uid, &uid_length, TAG_DETECT_TIMEOUT);
    if (err_code != NRF_SUCCESS) {
        log_debug_printf("Error while scaning tag %u", err_code);
        return;
    }

    log_debug_printf("Tag detected uid_length=%u", uid_length);
    log_debug_hex("uid:", uid, uid_length);

    if (uid_length == 4) {
        log_debug_printf("Tag is probably Mifare Clasic. Reading...");

        auth_key = def_mifare_key;
        if (pn532_mifareclassic_authenticateblock(
              uid, uid_length, 4, MIFARE_AUTH_KEY_A, auth_key)) {
            log_debug_printf("Failed while using default key. Trying the \"gate\" key...");

            /* some bug causes that after invalid auth, all following auth also
             * does failure. Therefore we rescan which reset the state machine */
            err_code = pn532_read_passive_target_id(
               PN532_MIFARE_ISO14443A_BAUD, uid, &uid_length, TAG_DETECT_TIMEOUT);
            if (err_code != NRF_SUCCESS) {
                log_debug_printf("Error while scaning tag %u", err_code);
                return;
            }

            auth_key = gate_mifare_key;
            if (pn532_mifareclassic_authenticateblock(
                  uid, uid_length, 4, MIFARE_AUTH_KEY_A, auth_key)) {
                log_debug_printf("Failed while using \"gate\" key, bailing out.");
                return;
            }
        }

        for (i = 0; i < 16; i++) {
            if (pn532_mifareclassic_authenticateblock(
                  uid, uid_length, i, MIFARE_AUTH_KEY_A, auth_key)) {
                log_debug_printf("Error while block auth");
                return;
            }

            if (pn532_mifareclassic_readdatablock(i, block_data, sizeof(block_data))) {
                log_debug_printf("Error while block read");
                return;
            }

            snprintf(trailer, sizeof(trailer), "Block[%02u]:", i);
            log_debug_hex(trailer, block_data, 16);
        }

        log_debug_printf("Dump done.");
    }
}

void tag_init(void)
{
    size_t i;
    ret_code_t err_code;
    unsigned long long sig_len;
    int ret;
    uint8_t uid[TAG_TYPE_2_UID_LENGTH] = { 0 };
    uint8_t uid_length                 = TAG_TYPE_2_UID_LENGTH;
    uint8_t auth_data[4 + 16];
    uint8_t check_data[16];
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
        log_debug_printf("Error while scaning tag %u", err_code);
        goto err;
    }

    LEDS_ON(BSP_LED_2_MASK);
    log_debug_printf("Tag detected uid_length=%u", uid_length);
    log_debug_hex("uid:", uid, uid_length);

    if (uid_length == 4) {
        log_debug_printf("Tag is probably Mifare Clasic. Reading...");

         /* Copy the uid into auth stream */
         memcpy(auth_data, uid, 4);

         /* Read the access rights into auth stream */
         log_debug_printf("Reading access rights ...");
         block = 4;
         if (pn532_mifareclassic_authenticateblock(
               uid, uid_length, block, MIFARE_AUTH_KEY_A, def_mifare_key)) {
             log_debug_printf("Error while unlocking block");
             goto err;
         }

         if (pn532_mifareclassic_readdatablock(
               block, &auth_data[4], sizeof(auth_data) - 4)) {
             log_debug_printf("Error while reading user auth block");
             goto err;
         }
         log_debug_hex("Auth data:", auth_data, 4 + 16);

         log_debug_printf("Writing check data...");
         block = 5;
         pn532_mifareclasic_value_format(0x00000000, check_data, block);
         log_debug_hex("Check data:", check_data, 16);
         if (pn532_mifareclassic_writedatablock(
               block, check_data)) {
             log_debug_printf("Error while writing check block");
             goto err;
         }

         log_debug_printf("Writing sig...");
         ret = crypto_sign_ed25519_detached(
              crypto_sig, &sig_len, auth_data, sizeof(auth_data), crypto_sk);
         if(ret || (sig_len != 64)) {
            log_debug_printf("Sign error\n");
            goto err;
         }

         for (i = 0; i < 4; i++) {
            block = (i + 8) + (i==3?1:0); /* omit 11th block */
            if (pn532_mifareclassic_authenticateblock(
                  uid, uid_length, block, MIFARE_AUTH_KEY_A, def_mifare_key)) {
                log_debug_printf("Error while unlocking block");
                goto err;
            }

            if (pn532_mifareclassic_writedatablock(
                  block, &crypto_sig[i * 16])) {
                log_debug_printf("Error while writing user auth block");
                goto err;
            }

            log_debug_hex("Write auth sig:", &crypto_sig[i*16], 16);
         }

         log_debug_printf("Securing block access rights and keys");
         memcpy(auth_data, gate_mifare_key, 6);
         /* following are the access rights, those are bit encoded with some redundant
	  * information encoded as inversed bit values. To decode the access
	  * rights use Mifare Bit calculator, like following
	  * http://www.akafugu.jp/posts/blog/2015_09_01-MIFARE-Classic-1K-Access-Bits-Calculator/ */
         auth_data[6] = 0xFF;
         auth_data[7] = 0x07;
         auth_data[8] = 0x80;
         auth_data[9] = 0x00;
         /* using default key for keyB since card will allow to read it, while in the
          * same time it will refuse to use it for block operations */
         memset(&auth_data[10], 0xff, 6);

         for (i = 0; i < 4; i++) {
            block = i*4 + 3;
            if (pn532_mifareclassic_authenticateblock(
                  uid, uid_length, block, MIFARE_AUTH_KEY_A, def_mifare_key)) {
                log_debug_printf("Error while unlocking block");
                goto err;
            }

            log_debug_hex("Secure block:", auth_data, 16);

            if (pn532_mifareclassic_writedatablock(
                  block, auth_data)) {
                log_debug_printf("Error while writing auth block");
                goto err;
            }

         log_debug_printf("Sector %u secured", i);
         }

         log_debug_printf("Writing OK!");
         nrf_delay_ms(1000);
    }

err:
    LEDS_OFF(BSP_LED_2_MASK);
}

void tag_scan(void)
{
    size_t i;
    ret_code_t err_code;
    uint32_t check_val;
    uint8_t uid[TAG_TYPE_2_UID_LENGTH] = { 0 };
    uint8_t uid_length                 = TAG_TYPE_2_UID_LENGTH;
    uint8_t auth_data[4 + 16];
    uint8_t check_data[16];
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
        log_debug_printf("Error while scaning tag %u", err_code);
        goto err;
    }

    LEDS_ON(BSP_LED_2_MASK);
    log_debug_printf("Tag detected uid_length=%u", uid_length);
    log_debug_hex("uid:", uid, uid_length);

    if (uid_length == 4) {
        log_debug_printf("Tag is probably Mifare Clasic. Reading...");

         /* Copy the uid into auth stream */
         memcpy(auth_data, uid, 4);

         /* Read the access rights into auth stream */
         log_debug_printf("Reading access rights ...");
         block = 4;
         if (pn532_mifareclassic_authenticateblock(
               uid, uid_length, block, MIFARE_AUTH_KEY_A, gate_mifare_key)) {
             log_debug_printf("Error while unlocking block");
             goto err;
         }

         if (pn532_mifareclassic_readdatablock(
               block, &auth_data[4], sizeof(auth_data) - 4)) {
             log_debug_printf("Error while reading user auth block");
             goto err;
         }
         log_debug_hex("Auth data:", auth_data, 4 + 16);

         block = 5;
         if (pn532_mifareclassic_readdatablock(
               block, check_data, sizeof(check_data))) {
             log_debug_printf("Error while reading check_data block");
             goto err;
         }
         log_debug_hex("Checkpoint data:", check_data, sizeof(check_data));

         log_debug_printf("Validating check_data...");
         if (pn532_mifareclasic_value_verify(&check_val, check_data, block)) {
             log_debug_printf("Check validation error");
             goto err;
         }
         log_debug_printf("Check value %u", check_val);

         log_debug_printf("Incrementing check point...");
         if (pn532_mifareclassic_increment(block, 0x01)) {
             log_debug_printf("Error while incrementing checkpoint");
             goto err;
         }
         if (pn532_mifareclassic_transfer(block)) {
             log_debug_printf("Error while transfer checkpoint block");
             goto err;
         }

         log_debug_printf("Reading signature ...");
         for (i = 0; i < 4; i++) {
            block = (i + 8) + (i==3?1:0); /* omit 11th block */
            if (pn532_mifareclassic_authenticateblock(
                  uid, uid_length, block, MIFARE_AUTH_KEY_A, gate_mifare_key)) {
                log_debug_printf("Error while unlocking block");
                goto err;
            }

            if (pn532_mifareclassic_readdatablock(
                  block, &crypto_sig[i * 16], sizeof(crypto_sig) - (i * 16))) {
                log_debug_printf("Error while reading user auth block");
                goto err;
            }

            log_debug_hex("Auth sig:", &crypto_sig[i*16], 16);
         }

         LEDS_OFF(BSP_LED_2_MASK);

         log_debug_printf("Testing sig...");
         if (crypto_sign_ed25519_verify_detached(crypto_sig, auth_data, 16 + 4, crypto_pk)) {
            log_debug_printf("Sig verify error\n\n");
            goto err;
         }
         log_debug_printf("Sig test OK!\n\n");
         LEDS_ON(BSP_LED_3_MASK);
         nrf_delay_ms(1000);
    }

   return;

err:
   log_debug_printf("Sig test FAIL!\n\n");
   LEDS_OFF(BSP_LED_2_MASK);
   LEDS_ON(BSP_LED_1_MASK);
   nrf_delay_ms(1000);
   return;
}

int main(void)
{
    board_setup();

    LEDS_ON(BSP_LED_0_MASK);
    log_debug_printf("Main loop");
    for (;;) {
        LEDS_OFF(LEDS_MASK);
        LEDS_ON(BSP_LED_0_MASK);
//#define DUMP_CARD
#ifdef  DUMP_CARD
        dump_card();
#endif

//#define INIT_EMPTY_TAG
#ifdef  INIT_EMPTY_TAG
        tag_init();
#else
        tag_scan();
#endif
    }
}

