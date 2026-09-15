/*
 * File: usb_backend.h
 *
 *
 */

#ifndef USB_BACKEND_H
#define USB_BACKEND_H

#include <stdbool.h>
#include <stdint.h>

#include <stlink_serial.h>

#include "logging.h"

/*
 * Separates how bytes reach the ST-LINK from what the bytes mean, so that
 * usb.c keeps only the latter. It follows the pattern stlink_backend already
 * uses: a table of function pointers plus opaque data.
 *
 * Everything the ST-LINK protocol needs is synchronous bulk transfer, so this
 * stays small on purpose: discover, open, close, read, write.
 *
 * Reading the serial belongs down here rather than above it. libusb has to
 * open a device and ask for a string descriptor, while on Windows the serial
 * is already part of the device path, and the caller should not have to know
 * which.
 *
 * This header includes no project header other than the two leaf ones above,
 * so that it can be included on its own.
 */

/* Endpoint direction, matching bit 7 of the USB endpoint address. */
#define STLINK_USB_EP_OUT 0x00
#define STLINK_USB_EP_IN  0x80

/* Longest string error_name() will write, including the terminator. */
#define STLINK_USB_ERROR_NAME_SIZE 256

struct stlink_usb;

/*
 * One ST-LINK as discovery found it, described without reference to any
 * backend. priv is whatever that backend needs to open it later: a
 * libusb_device*, or a device interface path. It belongs to the backend and
 * stays valid only until release().
 */
struct stlink_usb_device {
    uint16_t vid;
    uint16_t pid;
    /* Free form and for diagnostics only: a bus:address pair, or a port. */
    char location[16];
    char serial[STLINK_SERIAL_BUFFER_SIZE];
    void *priv;
};

struct stlink_usb_backend {
    const char *name;

    /*
     * Prepare the transport and allocate usb->backend_data. This, and not
     * open(), is where backend_data comes from, because enumerate() runs
     * before any device is opened and already needs the backend's own state.
     * exit() is the matching teardown, and is safe after a failed init().
     */
    int32_t (*init)(struct stlink_usb *usb, enum ugly_loglevel verbose);
    void (*exit)(struct stlink_usb *usb);

    /*
     * Report every attached ST-LINK, filling in vid, pid, location and priv,
     * but deliberately not the serial. No filtering beyond the vendor is
     * applied: callers differ on which products they accept, so that decision
     * stays with them. On a negative return devices is untouched.
     */
    int32_t (*enumerate)(struct stlink_usb *usb, struct stlink_usb_device **devices);

    /*
     * Fill in device->serial, leaving it empty on failure.
     *
     * Kept apart from enumerate() because it can be expensive: libusb has to
     * open the device to read a string descriptor, and opening every device up
     * front makes concurrent probes collide on the ones they do not even want.
     * Callers read only the serials they actually need, and stop early when
     * they are looking for one in particular.
     *
     * Does not log. The caller knows whether a failure here is worth reporting.
     */
    int32_t (*read_serial)(struct stlink_usb *usb, struct stlink_usb_device *device);

    /*
     * Release what enumerate() returned. Must be a no-op when given a NULL
     * array or the negative count enumerate() returned, since a caller whose
     * enumerate() failed has no other count to pass.
     */
    void (*release)(struct stlink_usb *usb, struct stlink_usb_device *devices, int32_t count);

    /*
     * Open one of the devices enumerate() returned. The caller may release()
     * the array immediately afterwards while the connection stays live, so
     * open() must copy anything it still needs rather than keep a pointer
     * into device.
     */
    int32_t (*open)(struct stlink_usb *usb, const struct stlink_usb_device *device);

    /* Safe to call when nothing is open. */
    void (*close)(struct stlink_usb *usb);

    /*
     * Synchronous bulk transfer. The buffer is not const because every USB API
     * in reach takes one pointer for both directions.
     *
     * ep is an endpoint number combined with STLINK_USB_EP_IN or _OUT.
     * transferred may be NULL, and is otherwise always written, including on
     * failure, where it is best effort.
     *
     * Returns 0 on success, negative on failure.
     *
     * Named bulk_ rather than plainly, because win32_socket.h macro-defines
     * read and write and a member of either name would not survive it.
     */
    int32_t (*bulk_write)(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                          uint32_t len, uint32_t timeout_ms, int32_t *transferred);
    int32_t (*bulk_read)(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                         uint32_t len, uint32_t timeout_ms, int32_t *transferred);

    /*
     * Clear a stalled endpoint so that transfers on it can resume, and say
     * whether an error meant the endpoint stalled in the first place. The
     * second one is a predicate rather than a normalised error code because
     * only the backend knows what its own codes mean, and the V1 SCSI path is
     * the only caller that needs to tell a stall apart from anything else.
     */
    int32_t (*clear_halt)(struct stlink_usb *usb, uint8_t ep);
    bool (*is_stall)(int32_t error);

    /*
     * Name an error this backend returned. The caller supplies the buffer,
     * because probing opens several devices on several threads and a shared
     * static one would race. A backend whose strings are compile time
     * constants may ignore the buffer and return its own pointer.
     */
    const char *(*error_name)(int32_t error, char *buf, uint32_t len);
};

/*
 * backend_data is allocated by backend->init() and freed by backend->exit(),
 * so a caller never pairs the two up itself.
 */
struct stlink_usb {
    const struct stlink_usb_backend *backend;
    void *backend_data;
};

/* The transport compiled in for this platform. */
const struct stlink_usb_backend *stlink_usb_backend_get(void);

/*
 * Write the serial a raw USB string descriptor carries, as read_serial() needs
 * whichever transport fetched the descriptor. Returns the length written, or
 * (0) if the descriptor was not one an ST-LINK produces, leaving serial empty.
 * serial must hold STLINK_SERIAL_BUFFER_SIZE bytes.
 */
uint32_t stlink_usb_serial_from_descriptor(const uint8_t *desc, uint32_t len, char *serial);


/* Dispatchers, so that call sites read as operations rather than lookups. */

/*
 * backend is assigned before init() runs, which is what makes a
 * stlink_usb_exit() after a failed init() safe.
 */
static inline int32_t stlink_usb_init(struct stlink_usb *usb, enum ugly_loglevel verbose) {
    usb->backend = stlink_usb_backend_get();
    return (usb->backend->init(usb, verbose));
}

static inline void stlink_usb_exit(struct stlink_usb *usb) {
    if(usb->backend != NULL) { usb->backend->exit(usb); }
}

static inline int32_t stlink_usb_enumerate(struct stlink_usb *usb, struct stlink_usb_device **devices) {
    return (usb->backend->enumerate(usb, devices));
}

static inline int32_t stlink_usb_read_serial(struct stlink_usb *usb, struct stlink_usb_device *device) {
    return (usb->backend->read_serial(usb, device));
}

static inline void stlink_usb_release(struct stlink_usb *usb, struct stlink_usb_device *devices, int32_t count) {
    usb->backend->release(usb, devices, count);
}

static inline int32_t stlink_usb_open(struct stlink_usb *usb, const struct stlink_usb_device *device) {
    return (usb->backend->open(usb, device));
}

static inline void stlink_usb_close(struct stlink_usb *usb) {
    if(usb->backend != NULL) { usb->backend->close(usb); }
}

static inline int32_t stlink_usb_write(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                                       uint32_t len, uint32_t timeout_ms, int32_t *transferred) {
    return (usb->backend->bulk_write(usb, ep, buf, len, timeout_ms, transferred));
}

static inline int32_t stlink_usb_read(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                                      uint32_t len, uint32_t timeout_ms, int32_t *transferred) {
    return (usb->backend->bulk_read(usb, ep, buf, len, timeout_ms, transferred));
}

static inline int32_t stlink_usb_clear_halt(struct stlink_usb *usb, uint8_t ep) {
    return (usb->backend->clear_halt(usb, ep));
}

static inline bool stlink_usb_is_stall(struct stlink_usb *usb, int32_t error) {
    return (usb->backend->is_stall(error));
}

static inline const char *stlink_usb_error_name(struct stlink_usb *usb, int32_t error,
                                                char *buf, uint32_t len) {
    return (usb->backend->error_name(error, buf, len));
}

#endif // USB_BACKEND_H
