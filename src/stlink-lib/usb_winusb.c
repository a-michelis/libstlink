/*
 * File: usb_winusb.c
 *
 * WinUSB implementation of the USB transport.
 */

/*
 * winsock2.h ahead of windows.h, for the same reason libusb_settings.h does it:
 * whichever of winsock.h and winsock2.h arrives second loses, and the project
 * headers below reach for winsock2.h through sys_time.h.
 */
#include <winsock2.h>
#include <windows.h>
#include <winusb.h>
#include <setupapi.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "stlink_usb.h"
#include "usb_backend.h"

/*
 * The device interface an ST-LINK publishes for its debug endpoint. A V2 gets
 * it from the driver package, a V3 from the MS OS descriptors in its own
 * firmware, and on a composite V3 it belongs to the first interface rather
 * than to the device. Enumerating it therefore finds the debug endpoint of
 * either generation directly, without having to walk a device tree.
 */
static const GUID stlink_interface_guid = {
    0xDBCE1CD9, 0xA320, 0x4B51, {0xA3, 0x65, 0xA0, 0xC3, 0xF3, 0xC5, 0xFB, 0x29}
};

/* Windows has no LANGID-less form of a string descriptor request. */
#define WINUSB_LANGID_ENGLISH_US 0x0409

/*
 * One slot per endpoint address, so that a pipe is only reconfigured when the
 * timeout actually changes. Address bit 7 is the direction, bits 0 to 3 the
 * number, which is what the low 5 bits below index.
 */
#define WINUSB_PIPE_SLOTS 32
#define WINUSB_PIPE_SLOT(ep) ((uint32_t)((((ep) & 0x80u) >> 3) | ((ep) & 0x0Fu)))

struct winusb_priv {
    HANDLE file;
    WINUSB_INTERFACE_HANDLE winusb;
    /* (0) means the pipe has not been given a timeout yet. */
    uint32_t timeout_ms[WINUSB_PIPE_SLOTS];
};

/* Win32 reports failure out of band, so carry the code in the return value. */
static int32_t winusb_last_error(void) {
    DWORD error = GetLastError();

    return ((error == 0) ? (-1) : (-(int32_t)error));
}

/*
 * Pull the identifiers out of a device interface path, which reads
 * \\?\usb#vid_0483&pid_374e&mi_00#<instance>#<guid> and is always lowercase.
 */
static bool winusb_path_ids(const char *path, uint16_t *vid, uint16_t *pid) {
    const char *vid_at = strstr(path, "vid_");
    const char *pid_at = strstr(path, "pid_");

    if((vid_at == NULL) || (pid_at == NULL)) { return (false); }

    unsigned int vid_value = 0;
    unsigned int pid_value = 0;

    if(sscanf(vid_at + 4, "%4x", &vid_value) != 1) { return (false); }
    if(sscanf(pid_at + 4, "%4x", &pid_value) != 1) { return (false); }

    *vid = (uint16_t)vid_value;
    *pid = (uint16_t)pid_value;

    return (true);
}

/* Open the WinUSB interface a device path names. */
static int32_t winusb_open_path(const char *path, HANDLE *file, WINUSB_INTERFACE_HANDLE *winusb) {
    /* WinUsb_Initialize() requires the handle to have been opened overlapped. */
    *file = CreateFileA(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);

    if(*file == INVALID_HANDLE_VALUE) {
        *file = NULL;
        return (winusb_last_error());
    }

    if(!WinUsb_Initialize(*file, winusb)) {
        int32_t ret = winusb_last_error();

        CloseHandle(*file);
        *file = NULL;
        *winusb = NULL;

        return (ret);
    }

    return (0);
}

static void winusb_close_handles(HANDLE *file, WINUSB_INTERFACE_HANDLE *winusb) {
    if(*winusb != NULL) {
        WinUsb_Free(*winusb);
        *winusb = NULL;
    }

    if(*file != NULL) {
        CloseHandle(*file);
        *file = NULL;
    }
}

static int32_t winusb_backend_init(struct stlink_usb *usb, enum ugly_loglevel verbose) {
    /* Nothing to configure: WinUSB logs nowhere and has no context to create. */
    (void)verbose;

    struct winusb_priv *priv = calloc(1, sizeof(struct winusb_priv));

    if(priv == NULL) { return (-1); }

    usb->backend_data = priv;

    return (0);
}

static void winusb_backend_exit(struct stlink_usb *usb) {
    struct winusb_priv *priv = usb->backend_data;

    if(priv == NULL) { return; }

    winusb_close_handles(&priv->file, &priv->winusb);

    free(priv);
    usb->backend_data = NULL;
}

static int32_t winusb_backend_enumerate(struct stlink_usb *usb, struct stlink_usb_device **devices) {
    (void)usb;

    HDEVINFO set = SetupDiGetClassDevsA(&stlink_interface_guid, NULL, NULL,
                                        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if(set == INVALID_HANDLE_VALUE) { return (winusb_last_error()); }

    /* Count first, so that the array is allocated once and never grown. */
    SP_DEVICE_INTERFACE_DATA interface_data;
    interface_data.cbSize = sizeof(interface_data);

    DWORD present = 0;

    while(SetupDiEnumDeviceInterfaces(set, NULL, &stlink_interface_guid, present, &interface_data)) { present++; }

    /* One spare entry, so that calloc() is never asked for zero elements. */
    struct stlink_usb_device *found = calloc((size_t)present + 1, sizeof(struct stlink_usb_device));

    if(found == NULL) {
        SetupDiDestroyDeviceInfoList(set);
        return (-1);
    }

    int32_t count = 0;

    for(DWORD i = 0; i < present; i++) {
        interface_data.cbSize = sizeof(interface_data);

        if(!SetupDiEnumDeviceInterfaces(set, NULL, &stlink_interface_guid, i, &interface_data)) { continue; }

        DWORD needed = 0;

        /* Asking with no buffer is how the length of the path is discovered. */
        SetupDiGetDeviceInterfaceDetailA(set, &interface_data, NULL, 0, &needed, NULL);

        if(needed == 0) { continue; }

        SP_DEVICE_INTERFACE_DETAIL_DATA_A *detail = calloc(1, needed);

        if(detail == NULL) { continue; }

        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        SP_DEVINFO_DATA device_data;
        device_data.cbSize = sizeof(device_data);

        if(!SetupDiGetDeviceInterfaceDetailA(set, &interface_data, detail, needed, NULL, &device_data)) {
            free(detail);
            continue;
        }

        uint16_t vid = 0;
        uint16_t pid = 0;

        if(!winusb_path_ids(detail->DevicePath, &vid, &pid) || (vid != STLINK_USB_VID_ST)) {
            free(detail);
            continue;
        }

        found[count].vid = vid;
        found[count].pid = pid;

        /* Reads as Port_#0002.Hub_#0004, which is what Device Manager shows. */
        if(!SetupDiGetDeviceRegistryPropertyA(set, &device_data, SPDRP_LOCATION_INFORMATION, NULL,
                                              (PBYTE)found[count].location,
                                              sizeof(found[count].location) - 1, NULL)) {
            found[count].location[0] = '\0';
        }

        /* No serial here on purpose, exactly as the libusb backend does it:
         * reading one costs an open. See read_serial below. */

        /* The path is all that is needed to open the device later, and it
         * stays valid for as long as the device is attached. */
        found[count].priv = _strdup(detail->DevicePath);

        free(detail);

        if(found[count].priv == NULL) { continue; }

        count++;
    }

    SetupDiDestroyDeviceInfoList(set);

    *devices = found;

    return (count);
}

static int32_t winusb_backend_read_serial(struct stlink_usb *usb, struct stlink_usb_device *device) {
    (void)usb;

    HANDLE file = NULL;
    WINUSB_INTERFACE_HANDLE winusb = NULL;

    device->serial[0] = '\0';

    int32_t ret = winusb_open_path(device->priv, &file, &winusb);

    if(ret != 0) { return (ret); }

    USB_DEVICE_DESCRIPTOR descriptor;
    ULONG moved = 0;

    if(!WinUsb_GetDescriptor(winusb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
                             (PUCHAR)&descriptor, sizeof(descriptor), &moved)) {
        ret = winusb_last_error();
        winusb_close_handles(&file, &winusb);

        return (ret);
    }

    if(descriptor.iSerialNumber == 0) {
        winusb_close_handles(&file, &winusb);

        return (-1);
    }

    uint8_t raw[(STLINK_SERIAL_LENGTH + 1) * 2];

    if(!WinUsb_GetDescriptor(winusb, USB_STRING_DESCRIPTOR_TYPE, descriptor.iSerialNumber,
                             WINUSB_LANGID_ENGLISH_US, raw, sizeof(raw), &moved)) {
        ret = winusb_last_error();
        winusb_close_handles(&file, &winusb);

        return (ret);
    }

    winusb_close_handles(&file, &winusb);

    stlink_usb_serial_from_descriptor(raw, (uint32_t)moved, device->serial);

    return (0);
}

static void winusb_backend_release(struct stlink_usb *usb, struct stlink_usb_device *devices, int32_t count) {
    (void)usb;

    if((devices == NULL) || (count < 0)) { return; }

    for(int32_t i = 0; i < count; i++) { free(devices[i].priv); }

    free(devices);
}

static int32_t winusb_backend_open(struct stlink_usb *usb, const struct stlink_usb_device *device) {
    struct winusb_priv *priv = usb->backend_data;

    /* The path is copied into the handle by CreateFile, so nothing belonging
     * to device is retained once this returns. */
    int32_t ret = winusb_open_path(device->priv, &priv->file, &priv->winusb);

    if(ret != 0) { return (ret); }

    memset(priv->timeout_ms, 0, sizeof(priv->timeout_ms));

    /* Configuration and interface are the driver's business, so there is no
     * equivalent of set_configuration or claim_interface to do here. */

    return (0);
}

static void winusb_backend_close(struct stlink_usb *usb) {
    struct winusb_priv *priv = usb->backend_data;

    if(priv == NULL) { return; }

    winusb_close_handles(&priv->file, &priv->winusb);
}

/*
 * WinUSB waits forever by default, so every pipe needs a timeout before it is
 * used. The value is remembered per pipe, since the protocol keeps the same
 * one for long stretches and this saves a request per transfer.
 */
static void winusb_set_timeout(struct winusb_priv *priv, uint8_t ep, uint32_t timeout_ms) {
    uint32_t slot = WINUSB_PIPE_SLOT(ep);

    if(priv->timeout_ms[slot] == timeout_ms) { return; }

    ULONG value = timeout_ms;

    if(WinUsb_SetPipePolicy(priv->winusb, ep, PIPE_TRANSFER_TIMEOUT, sizeof(value), &value)) {
        priv->timeout_ms[slot] = timeout_ms;
    }
}

static int32_t winusb_backend_write(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                                    uint32_t len, uint32_t timeout_ms, int32_t *transferred) {
    struct winusb_priv *priv = usb->backend_data;
    ULONG moved = 0;

    winusb_set_timeout(priv, ep, timeout_ms);

    BOOL ok = WinUsb_WritePipe(priv->winusb, ep, buf, len, &moved, NULL);

    if(transferred != NULL) { *transferred = (int32_t)moved; }

    return (ok ? (0) : winusb_last_error());
}

static int32_t winusb_backend_read(struct stlink_usb *usb, uint8_t ep, uint8_t *buf,
                                   uint32_t len, uint32_t timeout_ms, int32_t *transferred) {
    struct winusb_priv *priv = usb->backend_data;
    ULONG moved = 0;

    winusb_set_timeout(priv, ep, timeout_ms);

    BOOL ok = WinUsb_ReadPipe(priv->winusb, ep, buf, len, &moved, NULL);

    if(transferred != NULL) { *transferred = (int32_t)moved; }

    return (ok ? (0) : winusb_last_error());
}

static int32_t winusb_backend_clear_halt(struct stlink_usb *usb, uint8_t ep) {
    struct winusb_priv *priv = usb->backend_data;

    /* Resetting the pipe is what clears a stall and drops whatever the pipe
     * had queued, which is what libusb_clear_halt() does as well. */
    return (WinUsb_ResetPipe(priv->winusb, ep) ? (0) : winusb_last_error());
}

static bool winusb_backend_is_stall(int32_t error) {
    /* A stalled pipe is the one condition WinUSB reports as a generic failure
     * rather than with a code of its own. */
    return (error == -(int32_t)ERROR_GEN_FAILURE);
}

static const char *winusb_backend_error_name(int32_t error, char *buf, uint32_t len) {
    if((buf == NULL) || (len == 0)) { return (""); }

    DWORD code = (DWORD)((error < 0) ? -error : error);
    DWORD written = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                   NULL, code, 0, buf, len, NULL);

    if(written == 0) {
        snprintf(buf, len, "Win32 error %lu", (unsigned long)code);
        return (buf);
    }

    /* FormatMessage ends its strings with a newline, which a log line adds. */
    while((written > 0) && ((buf[written - 1] == '\n') || (buf[written - 1] == '\r'))) {
        buf[--written] = '\0';
    }

    return (buf);
}

static const struct stlink_usb_backend _winusb_backend = {
    .name        = "winusb",
    .init        = winusb_backend_init,
    .exit        = winusb_backend_exit,
    .enumerate   = winusb_backend_enumerate,
    .read_serial = winusb_backend_read_serial,
    .release     = winusb_backend_release,
    .open        = winusb_backend_open,
    .close       = winusb_backend_close,
    .bulk_write  = winusb_backend_write,
    .bulk_read   = winusb_backend_read,
    .clear_halt  = winusb_backend_clear_halt,
    .is_stall    = winusb_backend_is_stall,
    .error_name  = winusb_backend_error_name,
};

const struct stlink_usb_backend *stlink_usb_backend_get(void) {
    return (&_winusb_backend);
}
