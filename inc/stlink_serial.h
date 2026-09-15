/*
 * File: stlink_serial.h
 *
 *
 */

#ifndef STLINK_SERIAL_H
#define STLINK_SERIAL_H

/*
 * These live apart from stlink.h because usb_backend.h needs them too, and it
 * cannot include stlink.h: stlink.h includes stlink_usb.h, which needs the
 * transport types, so the include would close a cycle.
 */

/* An ST-LINK reports its serial as 24 hexadecimal characters. */
#define STLINK_SERIAL_LENGTH                 24
#define STLINK_SERIAL_BUFFER_SIZE   (STLINK_SERIAL_LENGTH + 1)

#endif // STLINK_SERIAL_H
