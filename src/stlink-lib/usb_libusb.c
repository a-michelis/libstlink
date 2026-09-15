/*
 * File: usb_libusb.c
 *
 * libusb implementation of the USB transport.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb_settings.h>

#include "logging.h"
#include "stlink_usb.h"
#include "usb_backend.h"

struct libusb_priv {
    libusb_context *ctx;
    libusb_device_handle *handle;
};

/* return the length of serial or (0) in case of errors */
uint32_t stlink_serial(struct libusb_device_handle *handle, struct libusb_device_descriptor *desc, char *serial) {
    unsigned char desc_serial[(STLINK_SERIAL_LENGTH) * 2];

    /* truncate the string in the serial buffer */
    serial[0] = '\0';

    /* get the LANGID from String Descriptor Zero */
    int32_t ret = libusb_get_string_descriptor(handle, 0, 0, desc_serial, sizeof(desc_serial));
    if(ret < 4) return 0;

    uint32_t langid = desc_serial[2] | (desc_serial[3] << 8);

    /* get the serial */
    ret = libusb_get_string_descriptor(handle, desc->iSerialNumber, langid, desc_serial,
        sizeof(desc_serial));
    if(ret < 0) return 0; // could not read serial

    unsigned char len = desc_serial[0];

    if(len == ((STLINK_SERIAL_LENGTH + 1) * 2)) { /* len == 50 */
        /* good ST-Link adapter */
        ret = libusb_get_string_descriptor_ascii(
            handle, desc->iSerialNumber, (unsigned char *)serial, STLINK_SERIAL_BUFFER_SIZE);
        if(ret < 0) return 0;
    } else if(len == ((STLINK_SERIAL_LENGTH / 2 + 1) * 2)) { /* len == 26 */
        /* fix-up the buggy serial */
        for(uint32_t i = 0; i < STLINK_SERIAL_LENGTH; i += 2)
            sprintf(serial + i, "%02X", desc_serial[i + 2]);
        serial[STLINK_SERIAL_LENGTH] = '\0';
    } else {
        return 0;
    }

    return (uint32_t)strlen(serial);
}

/*
 *  Log message levels.
 *  - LIBUSB_LOG_LEVEL_NONE (0)    : no messages ever printed by the library
 * (default)
 *  - LIBUSB_LOG_LEVEL_ERROR (1)   : error messages are printed to stderr
 *  - LIBUSB_LOG_LEVEL_WARNING (2) : warning and error messages are printed to
 * stderr
 *  - LIBUSB_LOG_LEVEL_INFO (3)    : informational messages are printed to
 * stderr
 *  - LIBUSB_LOG_LEVEL_DEBUG (4)   : debug and informational messages are
 * printed to stderr
 */
static int32_t libusb_log_level(enum ugly_loglevel v) {
#ifdef __FreeBSD__
  // FreeBSD includes its own reimplementation of libusb.
  // Its libusb_set_debug() function expects a lib_debug_level
  // instead of a lib_log_level and is verbose enough to drown out
  // all other output.
  switch (v) {
  case UDEBUG:
    return (3); // LIBUSB_DEBUG_FUNCTION + LIBUSB_DEBUG_TRANSFER
  case UINFO:
    return (1); // LIBUSB_DEBUG_FUNCTION only
  case UWARN:
    return (0); // LIBUSB_DEBUG_NO
  case UERROR:
    return (0); // LIBUSB_DEBUG_NO
  }
  return (0);
#else
  switch (v) {
  case UDEBUG:
    return (4);
  case UINFO:
    return (3);
  case UWARN:
    return (2);
  case UERROR:
    return (1);
  }
  return (2);
#endif
}

static int32_t libusb_backend_init(struct stlink_usb *usb, enum ugly_loglevel verbose) {
    struct libusb_priv *priv = calloc(1, sizeof(struct libusb_priv));
    if(priv == NULL) { return (-1); }

    if(libusb_init(&priv->ctx)) {
        WLOG("failed to init libusb context, wrong version of libraries?\n");
        free(priv);
        return (-1);
    }

#if LIBUSB_API_VERSION < 0x01000106
    libusb_set_debug(priv->ctx, libusb_log_level(verbose));
#else
    libusb_set_option(priv->ctx, LIBUSB_OPTION_LOG_LEVEL, libusb_log_level(verbose));
#endif

    usb->backend_data = priv;

    return (0);
}

static void libusb_backend_exit(struct stlink_usb *usb) {
    struct libusb_priv *priv = usb->backend_data;
    if(priv == NULL) { return; }

    if(priv->handle != NULL) { libusb_close(priv->handle); }
    if(priv->ctx != NULL) { libusb_exit(priv->ctx); }

    free(priv);
    usb->backend_data = NULL;
}

static int32_t libusb_backend_enumerate(struct stlink_usb *usb, struct stlink_usb_device **devices) {
    struct libusb_priv *priv = usb->backend_data;
    libusb_device **list = NULL;

    ssize_t cnt = libusb_get_device_list(priv->ctx, &list);
    if(cnt < 0) { return ((int32_t)cnt); }

    /* One spare entry, so that calloc() is never asked for zero elements. */
    struct stlink_usb_device *found = calloc((size_t)cnt + 1, sizeof(struct stlink_usb_device));

    if(found == NULL) {
        libusb_free_device_list(list, 1);
        return (-1);
    }

    int32_t count = 0;

    /* Walked from the end of the list, which is the order the discovery loop
     * in usb.c has always visited devices in. */
    while (cnt-- > 0) {
        struct libusb_device_descriptor desc;

        if(libusb_get_device_descriptor(list[cnt], &desc) < 0) { continue; }
        if(desc.idVendor != STLINK_USB_VID_ST) { continue; }

        found[count].vid = desc.idVendor;
        found[count].pid = desc.idProduct;
        snprintf(found[count].location, sizeof(found[count].location), "%03d:%03d",
                 libusb_get_bus_number(list[cnt]), libusb_get_device_address(list[cnt]));

        /* No serial here on purpose: reading one costs an open, and doing that
         * for every device would make concurrent probes fight over devices
         * they are not even looking for. See read_serial below. */

        /* Keep the device alive independently of the list, which is freed
         * below while the caller still holds these entries. */
        found[count].priv = libusb_ref_device(list[cnt]);
        count++;
    }

    libusb_free_device_list(list, 1);

    *devices = found;

    return (count);
}

static int32_t libusb_backend_read_serial(struct stlink_usb *usb, struct stlink_usb_device *device) {
    (void)usb;

    struct libusb_device_descriptor desc;
    libusb_device_handle *handle = NULL;

    device->serial[0] = '\0';

    if(libusb_get_device_descriptor(device->priv, &desc) < 0) { return (-1); }

    int32_t ret = libusb_open(device->priv, &handle);

    if(ret != 0) { return (ret); }

    stlink_serial(handle, &desc, device->serial);
    libusb_close(handle);

    return (0);
}

static void libusb_backend_release(struct stlink_usb *usb, struct stlink_usb_device *devices, int32_t count) {
    (void)usb;

    if(devices == NULL || count < 0) { return; }

    for(int32_t i = 0; i < count; i++) {
        if(devices[i].priv != NULL) { libusb_unref_device(devices[i].priv); }
    }

    free(devices);
}

static int32_t libusb_backend_open(struct stlink_usb *usb, const struct stlink_usb_device *device) {
    struct libusb_priv *priv = usb->backend_data;

    /* libusb_open() takes its own reference, so nothing belonging to device is
     * retained once this returns. */
    int32_t ret = libusb_open(device->priv, &priv->handle);

    if(ret != 0) { return (ret); }

// libusb_kernel_driver_active is not available on Windows.
#if !defined(_WIN32)
    if(libusb_kernel_driver_active(priv->handle, 0) == 1) {
        ret = libusb_detach_kernel_driver(priv->handle, 0);

        if(ret < 0) {
            WLOG("libusb_detach_kernel_driver(() error %s\n", strerror(-ret));
            goto on_error;
        }
    }
#endif // NOT _WIN32

    int32_t config;

    if(libusb_get_configuration(priv->handle, &config)) {
        // this may fail for a previous configured device
        WLOG("libusb_get_configuration()\n");
        goto on_error;
    }

    if(config != 1) {
        printf("setting new configuration (%d -> 1)\n", config);

        if(libusb_set_configuration(priv->handle, 1)) {
            // this may fail for a previous configured device
            WLOG("libusb_set_configuration() failed\n");
            goto on_error;
        }
    }

    if(libusb_claim_interface(priv->handle, 0)) {
        WLOG("Stlink usb device found, but unable to claim (probably already in use?)\n");
        goto on_error;
    }

    return (0);

on_error:
    libusb_close(priv->handle);
    priv->handle = NULL;

    return (-1);
}

static void libusb_backend_close(struct stlink_usb *usb) {
    struct libusb_priv *priv = usb->backend_data;

    if(priv == NULL || priv->handle == NULL) { return; }

    libusb_close(priv->handle);
    priv->handle = NULL;
}

static int32_t libusb_backend_transfer(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                                       uint32_t len, uint32_t timeout_ms, int32_t *transferred) {
    struct libusb_priv *priv = usb->backend_data;
    int moved = 0;

    int32_t ret = libusb_bulk_transfer(priv->handle, ep, buf, (int32_t)len, &moved, timeout_ms);

    if(transferred != NULL) { *transferred = (moved < 0) ? 0 : moved; }

    return (ret);
}

static int32_t libusb_backend_write(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                                    uint32_t len, uint32_t timeout_ms, int32_t *transferred) {
    return (libusb_backend_transfer(usb, ep, buf, len, timeout_ms, transferred));
}

static int32_t libusb_backend_read(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                                   uint32_t len, uint32_t timeout_ms, int32_t *transferred) {
    return (libusb_backend_transfer(usb, ep, buf, len, timeout_ms, transferred));
}

static int32_t libusb_backend_clear_halt(struct stlink_usb *usb, uint8_t ep) {
    struct libusb_priv *priv = usb->backend_data;

    return (libusb_clear_halt(priv->handle, ep));
}

static bool libusb_backend_is_stall(int32_t error) {
    return (error == LIBUSB_ERROR_PIPE);
}

static const char *libusb_backend_error_name(int32_t error, char *buf, uint32_t len) {
    /* libusb's names are string constants, so the caller's buffer is unused. */
    (void)buf;
    (void)len;

    return (libusb_error_name(error));
}

static const struct stlink_usb_backend _libusb_backend = {
    .name       = "libusb",
    .init       = libusb_backend_init,
    .exit       = libusb_backend_exit,
    .enumerate  = libusb_backend_enumerate,
    .read_serial = libusb_backend_read_serial,
    .release    = libusb_backend_release,
    .open       = libusb_backend_open,
    .close      = libusb_backend_close,
    .bulk_write = libusb_backend_write,
    .bulk_read  = libusb_backend_read,
    .clear_halt = libusb_backend_clear_halt,
    .is_stall   = libusb_backend_is_stall,
    .error_name = libusb_backend_error_name,
};

const struct stlink_usb_backend *stlink_usb_backend_get(void) {
    return (&_libusb_backend);
}
