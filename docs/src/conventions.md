# Conventions

These hold everywhere in the library. They are worth learning once, because after that no
individual file surprises you.

## Failure is returned, never thrown

Every operation that can fail returns a `VoidResult`, or a `Result<T>` if it produces a value.
`Result<T>` derives from `VoidResult`, so a caller who only wants to know whether something
worked can take the base and ignore the value.

Errors are returned rather than thrown for three reasons. The library ships as a shared object,
and an exception crossing that boundary requires the caller to have been built with the same
compiler and runtime. A C ABI consumer, which is planned, cannot catch anything at all. And a
missing device is an expected outcome rather than an exceptional one.

`ok()` is the only accessor that never throws. Asking a result for a value it does not hold,
or taking that value twice, is a mistake in the caller rather than something the device did,
and throws `BadResultAccess`. That makes `Result` the one place the library throws, and only
on misuse.

```cpp
auto opened = stlink::IDevice::create(info);

if (!opened.ok())
{
    STLINK_LOG_ERR("%s", opened.error().describe().c_str());
    return;
}

auto device = opened.value();
```

Both are move only, and `value()` moves the value out, so `T` may itself be move only. Taking
it twice throws, which is worth knowing because it is an easy mistake in an error path: read
it into a local if you need it more than once.

Slicing a `Result<T>` down to `VoidResult` would silently drop the value, so `VoidResult`
deletes a templated constructor taking `Result<T>&&`. It matches exactly and therefore beats
the base move constructor, which turns the mistake into a compile error rather than a lost
value.

## Errors carry the whole chain

An `Error` is immutable. `wrap()` returns a new error whose cause is the old one, so each layer
adds what it was attempting on the way up and nothing is overwritten. `describe()` renders the
lot, outermost first:

```
erasing the sector <- writing flash <- the probe refused the command
```

The context is a string literal, so carrying it costs nothing; anything that varies goes to
the log instead. The chain is a linked list rather than a fixed array because libusb, the
transport, the programmer, the device and the flash can each add a frame, and nesting inside
flash makes the depth open ended. An allocation happens once per `wrap`, and only once
something has already failed.

No foreign code ever reaches a caller. A libusb or Win32 failure is translated into an
`ErrorCode` with a context saying what was being attempted, so the vocabulary in `error.h` is
the entire vocabulary a caller will ever see.

## Lifetime is ownership

Everything that holds a connection is connected when it is constructed and disconnected when
it is destroyed. There is no `open()` or `close()` anywhere in the library, so there is nothing
to forget to call and no half-open state to represent or test.

A `Device` leaves debug mode in its destructor, so a chip that was running when it was found is
running again once the device goes away.

## Logging is not the error channel

A failure reaches the caller as a return value. Logging is the detail behind it, and the two
are never substitutes.

One sink receives every record. To send records to several places, or to filter them, install
a sink that wraps the others rather than expecting the library to keep a list. The default
sink writes warnings and worse to stderr and the rest to stdout.

A record carries its kind, the issuing function, the line, and an already formatted message.
The function is the compiler's own signature as a string literal, so carrying it is free;
`qualified()` reduces it to `namespace::Class::function` on demand, which keeps the parsing off
the hot path. There is no file name, because the qualified name says more and does not change
when a file moves.

The threshold is atomic and is tested by the macro before anything is formatted, so a
suppressed record costs one relaxed load.

`LogKind::Fatal` means the library's own invariants are broken, so a bug here worth reporting,
as against `Error`, which is the device or the target refusing. Nothing aborts: the library
reports and the caller decides.

## Public and private headers

`include/` is installed. `include_pv/` is not. Both are reached as `<stlink/...>`, so moving a
header between them changes no include line anywhere.

Everything public is marked `STLINK_API`. On Windows that is a `__declspec` which resolves to
an export while the library is being compiled and an import everywhere else, the difference
being `STLINK_EXPORT`, defined `PRIVATE` on the target so only the library's own translation
units see it. Elsewhere it is default visibility against a hidden default, so the ELF builds
export the same set Windows does instead of exporting everything.

`STLINK_STATIC` disables both, since a static build has no import to describe, and CMake
defines it publicly when `BUILD_SHARED_LIBS` is off.

The practical consequence: a shared build is the one that catches a missing `STLINK_API`, so
CI builds shared everywhere. A static-only matrix would never notice.

## Naming

Filenames are lowercase, because Windows and macOS filesystems are case insensitive and Linux
is not, so a CamelCase include breaks on exactly one platform and nowhere else.

Types are CamelCase and members are snake_case, which is the Google and LLVM convention and
keeps our types visually distinct from the standard library's.

Constants are `kLikeThis`. Interfaces are `IPrefixed`.

## One type per header and source

A header and its source hold one type, except for helper types directly coupled to it. Several
files predate this rule and still break it; they are listed in the work backlog to be split
before they grow further.
