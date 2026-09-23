---
title: STLINK
section: 3
header: Library Functions Manual
footer: libstlink
date: 2026-09-23
---

# NAME

stlink - talk to an ST-LINK programmer, and through it to an STM32

# SYNOPSIS

```c
#include <stlink/stlink.h>
#include <stlink/device.h>
#include <stlink/flash.h>

void stlink::init();
void stlink::init(const std::string &chips_dir);

std::vector<stlink::BasicDeviceInfo> stlink::Discovery::enumerate();
std::vector<stlink::DeviceInfo>      stlink::Discovery::find(const DeviceFilter &filter);

stlink::Result<std::unique_ptr<stlink::IDevice>> stlink::IDevice::create(const DeviceInfo &info);
stlink::Result<std::unique_ptr<stlink::IDevice>> stlink::IDevice::create(const BasicDeviceInfo &info);
```

Link with *-lstlink*.

# DESCRIPTION

**libstlink** connects to an ST-LINK programmer of any generation and reaches the
STM32 behind it. A caller works with devices and chips; probes, opcodes and USB
endpoints are the library's own business and are not exposed.

Everything is C++17 in namespace **stlink**. A C binding is planned and does not
exist yet.

## Starting the library

**stlink::init**() reads the chip descriptions the library was installed with.
Nothing else works until it has been called, and calling it twice does nothing.
It looks in *$STLINK_CHIPS_DIR*, then in *../share/stlink/config/chips* relative
to the running program, then in *./chips* beside it, and uses the first that
exists.

**stlink::init**(*chips_dir*) reads that directory and nothing else, which is how
a caller supplies their own descriptions. A file that cannot be read or does not
parse is logged and skipped, so one bad file does not stop the library starting.

## Finding what is attached

**Discovery::enumerate**() lists every attached programmer without touching any
target, so it is safe to call while other targets are running. It returns a
*BasicDeviceInfo* per probe, carrying the generation, serial, USB identifiers and
where it is attached.

**Discovery::find**() goes further and connects through to the chip behind each
probe, which is what lets it filter on what the chip is. It returns a
*DeviceInfo*, which is a *BasicDeviceInfo* together with a *ChipInfo*. An empty
result means nothing matched, which is not an error.

A *DeviceFilter* may constrain the serial, the generation, or the chip
identifier; every field is optional and **DeviceFilter::NoFilter** accepts
everything. The serial and generation are checked before anything is opened, so
filtering on those costs no traffic.

## Opening a device

**IDevice::create**() connects and returns a device. One device exists per
programmer: a second **create**() for a programmer that is already open fails
with **ErrorCode::Busy** rather than returning the first. The device is
connected when it is created and disconnected when it is destroyed, and it
leaves debug mode on the way out, so a target that was running when it was found
is running again afterwards.

Given a *BasicDeviceInfo* the chip is read on connection, since a basic sheet
does not say what it is.

## Working with the target

A device can **halt**(), **run**(), **step**() and **reset**() the core, and
report whether it is running with **is_running**().

**read**() and **write**() move target memory. **read_register**() and
**write_register**() reach the core registers by index.

**info**() returns the *DeviceInfo* the device was opened with, including the
chip's memory map: flash, SRAM, the system bootloader, option bytes and OTP,
each with a base and a size. A size of zero means the chip does not have that
region. The flash size is read from the chip rather than taken from a file,
because one part number ships in several sizes.

## Flash

**flash**() returns the chip's flash controller, owned by the device and valid
for as long as it is.

In this release no flash family is implemented and every operation returns
**ErrorCode::NotSupported**. The interface is stable; the implementations are
not yet written.

Nothing in that interface is implicit. Erasing does not happen because a write
needed it, and nothing is verified unless verification was asked for.

# RETURN VALUE

Operations that can fail return *VoidResult*, or *Result<T>* if they produce a
value. Ask **ok**() first. **error**() describes the failure and **value**()
takes the value, and each throws **BadResultAccess** if asked when it does not
apply, because asking for a value that is not there is a mistake in the caller
rather than something the device did.

*Result<T>* is move only, and **value**() moves the value out, so it may be
taken only once.

# ERRORS

An *Error* carries an *ErrorCode*, a context saying what was being attempted,
and optionally the error that caused it. **describe**() renders the whole chain,
outermost first, separated by " <- ":

    erasing the sector <- writing flash <- the probe refused the command

No foreign error code ever reaches a caller. A libusb or Win32 failure is
translated, so the vocabulary in *stlink/error.h* is the whole vocabulary.

The codes are **IO**, **NotFound**, **AccessDenied**, **Busy**,
**Disconnected**, **Timeout**, **Protocol**, **TargetUnknown**,
**TargetRefused**, **NotSupported**, **InvalidArgument**, **OutOfRange**,
**OutOfMemory** and **Internal**. **Internal** means a broken invariant in the
library itself and is worth reporting as a bug.

# THREAD SAFETY

One device belongs to one thread at a time and nothing is internally
synchronised. Two threads may drive two devices; they may not drive one.

# LOGGING

Logging is not the error channel: a failure reaches the caller as a return
value, and logging is the detail behind it. **Log::set_sink**() installs a sink
that receives every record, and an empty sink silences the library.
**Log::set_threshold**() discards anything less severe than the kind given; the
default is **LogKind::Info**.

# EXAMPLE

```c
stlink::init();

auto found = stlink::Discovery::find({.chip_id = 0x450});

if (found.empty())
{
    return 1;
}

auto opened = stlink::IDevice::create(found.front());

if (!opened.ok())
{
    STLINK_LOG_ERR("%s", opened.error().describe().c_str());
    return 1;
}

auto device = opened.value();
auto halted = device->halt();
```

# FILES

*$STLINK_CHIPS_DIR*
: Chip descriptions, if set, in preference to the installed ones.

*../share/stlink/config/chips*
: Where the installed chip descriptions are, relative to the program.

# SEE ALSO

The headers are the reference for exact signatures: *stlink/stlink.h*,
*stlink/device.h*, *stlink/flash.h*, *stlink/result.h*, *stlink/error.h* and
*stlink/log.h*.

The chip description file format is documented in
*share/stlink/config/chips/README.md*.

# BUGS

No flash family is implemented, so the library cannot program flash.

Trace output, debug clock selection, and connecting under reset are implemented
below the public surface and are not reachable through it yet.
