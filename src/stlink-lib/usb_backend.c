/*
 * File: usb_backend.c
 *
 * Helpers shared by every USB transport implementation.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <stlink_serial.h>

#include "usb_backend.h"

/*
 * Turn a raw USB string descriptor into the serial.
 *
 * Which of the two shapes arrives is a property of the adapter rather than of
 * the transport that fetched it, so every backend shares this. A good adapter
 * sends the serial as text, while an older one sends half as many raw bytes
 * that have to be expanded to hexadecimal.
 *
 * len is how much of desc actually arrived, which is not always as much as the
 * descriptor claims in its first byte.
 */
uint32_t stlink_usb_serial_from_descriptor(const uint8_t *desc, uint32_t len, char *serial) {
    serial[0] = '\0';

    if((desc == NULL) || (len < 2)) { return (0); }

    uint32_t reported = desc[0];

    if(reported > len) { return (0); }

    if(reported == ((STLINK_SERIAL_LENGTH + 1) * 2)) {
        /* good ST-Link adapter: UTF-16, behind a length and a type byte */
        for(uint32_t i = 0; i < STLINK_SERIAL_LENGTH; i++) { serial[i] = (char)desc[2 + (i * 2)]; }

        serial[STLINK_SERIAL_LENGTH] = '\0';
    } else if(reported == (((STLINK_SERIAL_LENGTH / 2) + 1) * 2)) {
        /* fix-up the buggy serial */
        for(uint32_t i = 0; i < STLINK_SERIAL_LENGTH; i += 2) { sprintf(serial + i, "%02X", desc[i + 2]); }

        serial[STLINK_SERIAL_LENGTH] = '\0';
    } else {
        return (0);
    }

    return ((uint32_t)strlen(serial));
}
