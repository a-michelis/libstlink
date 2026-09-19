/**
 * @file    usb_transport.cpp
 * @brief   Reaching a programmer over USB, through libusb.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Used wherever the system provides libusb, which is Linux and the BSDs.
 * Windows and macOS reach USB their own way.
 */

#include <stlink/transport.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <utility>

#include <libusb.h>

#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /* A serial is 12 bytes on the wire and twice that written out. */
        constexpr std::size_t kSerialBytes = 12;
        constexpr std::size_t kSerialDigits = kSerialBytes * 2;

        /* The interface every ST-LINK puts its bulk endpoints on. */
        constexpr int kInterface = 0;

        /** @brief Which endpoints a programmer uses, decided by its product id. */
        struct Endpoints
        {
            std::uint8_t command = 0;
            std::uint8_t reply = 0;
            std::uint8_t trace = 0;
        };

        bool is_v3(std::uint16_t pid) noexcept
        {
            switch (pid)
            {
            case 0x374d: /* V3 loader */
            case 0x374e: /* V3E */
            case 0x374f: /* V3S */
            case 0x3753: /* V3 two VCP */
            case 0x3754: /* V3 no mass storage */
            case 0x3757: /* V3P */
                return true;
            default:
                return false;
            }
        }

        Endpoints endpoints_for(std::uint16_t pid) noexcept
        {
            Endpoints ep;

            ep.reply = 1 | LIBUSB_ENDPOINT_IN;

            if (is_v3(pid))
            {
                ep.command = 1 | LIBUSB_ENDPOINT_OUT;
                ep.trace = 2 | LIBUSB_ENDPOINT_IN;
            }
            else
            {
                ep.command = 2 | LIBUSB_ENDPOINT_OUT;
                ep.trace = 3 | LIBUSB_ENDPOINT_IN;
            }

            return ep;
        }

        ErrorCode code_for(int libusb_error) noexcept
        {
            switch (libusb_error)
            {
            case LIBUSB_ERROR_TIMEOUT:
                return ErrorCode::Timeout;
            case LIBUSB_ERROR_ACCESS:
                return ErrorCode::AccessDenied;
            case LIBUSB_ERROR_BUSY:
                return ErrorCode::Busy;
            case LIBUSB_ERROR_NO_DEVICE:
            case LIBUSB_ERROR_NOT_FOUND:
                return ErrorCode::Disconnected;
            case LIBUSB_ERROR_PIPE:
                return ErrorCode::Protocol;
            case LIBUSB_ERROR_OVERFLOW:
                return ErrorCode::Protocol;
            case LIBUSB_ERROR_NO_MEM:
                return ErrorCode::OutOfMemory;
            case LIBUSB_ERROR_NOT_SUPPORTED:
                return ErrorCode::NotSupported;
            case LIBUSB_ERROR_INVALID_PARAM:
                return ErrorCode::InvalidArgument;
            default:
                return ErrorCode::IO;
            }
        }

        /** @brief Turn a libusb failure into an error, saying what libusb called it. */
        Error error_from(int libusb_error, const char *context)
        {
            STLINK_LOG_DBG("%s: libusb says %s", context, libusb_error_name(libusb_error));

            return Error(code_for(libusb_error), context);
        }

        /**
         * @brief The serial of an open device, as the ST-LINK reports it.
         *
         * Older programmers answer with the twelve raw bytes rather than the
         * twenty four characters the descriptor promises. Both are accepted
         * and both come back written out, so that one adapter has one serial
         * whichever firmware it runs.
         */
        std::string read_serial(libusb_device_handle *handle,
                                const libusb_device_descriptor &descriptor)
        {
            if (descriptor.iSerialNumber == 0)
            {
                return {};
            }

            std::array<unsigned char, 256> raw{};
            const int length = libusb_get_string_descriptor_ascii(
                handle, descriptor.iSerialNumber, raw.data(), static_cast<int>(raw.size()));

            if (length == static_cast<int>(kSerialDigits))
            {
                return std::string(reinterpret_cast<const char *>(raw.data()),
                                   static_cast<std::size_t>(length));
            }

            if (length == static_cast<int>(kSerialBytes))
            {
                std::string written;
                written.reserve(kSerialDigits);

                for (std::size_t i = 0; i < kSerialBytes; ++i)
                {
                    char pair[3] = {};
                    std::snprintf(pair, sizeof(pair), "%02X", raw[i]);
                    written += pair;
                }

                return written;
            }

            if (length < 0)
            {
                STLINK_LOG_DBG("could not read a serial: libusb says %s",
                               libusb_error_name(length));
            }
            else
            {
                STLINK_LOG_DBG("a programmer reported a %d character serial, which is neither "
                               "%zu nor %zu", length, kSerialBytes, kSerialDigits);
            }

            return {};
        }

        std::string location_of(libusb_device *device)
        {
            char text[16] = {};
            std::snprintf(text, sizeof(text), "%03u:%03u", libusb_get_bus_number(device),
                          libusb_get_device_address(device));

            return text;
        }

        /** @brief A programmer reached through libusb. */
        class UsbTransport final : public ITransport
        {
        public:
            UsbTransport(libusb_context *context, libusb_device_handle *handle,
                         ProbeAddress address) noexcept
                : context_(context), handle_(handle), address_(std::move(address)),
                  endpoints_(endpoints_for(address_.pid))
            {
                /*
                 * A halt belongs to the device rather than to us, so one left
                 * behind by a process that died mid-transfer is still there
                 * when the next process opens the probe: it outlives our exit,
                 * and a probe in that state looks broken until someone
                 * unplugs it. Clearing all three on the way in costs three
                 * control transfers and removes a whole class of failure that
                 * otherwise reads as "it worked yesterday".
                 *
                 * Best effort. A probe that will not clear is probably
                 * unusable, but refusing to open it here would hide whatever
                 * the real failure turns out to be.
                 */
                for (const std::uint8_t endpoint :
                     {endpoints_.command, endpoints_.reply, endpoints_.trace})
                {
                    if (!clear_stall(endpoint).ok())
                    {
                        STLINK_LOG_DBG("could not clear endpoint %#04x on opening", endpoint);
                    }
                }
            }

            ~UsbTransport() override
            {
                if (handle_ != nullptr)
                {
                    libusb_release_interface(handle_, kInterface);
                    libusb_close(handle_);
                }

                if (context_ != nullptr)
                {
                    libusb_exit(context_);
                }
            }

            const ProbeAddress &address() const noexcept override
            {
                return address_;
            }

            Result<std::size_t> send(const std::uint8_t *data, std::size_t size,
                                     Timeout timeout) override
            {
                /* libusb does not write through this, despite the signature. */
                return transfer(endpoints_.command, const_cast<std::uint8_t *>(data), size,
                                timeout, "sending a command");
            }

            Result<std::size_t> receive(std::uint8_t *data, std::size_t size,
                                        Timeout timeout) override
            {
                return transfer(endpoints_.reply, data, size, timeout, "reading a reply");
            }

            Result<std::size_t> receive_trace(std::uint8_t *data, std::size_t size,
                                              Timeout timeout) override
            {
                return transfer(endpoints_.trace, data, size, timeout, "reading trace output");
            }

            VoidResult reset(Channel channel) override
            {
                return clear_stall(endpoint_for(channel));
            }

        private:
            /**
             * @brief Clear a halt on one endpoint.
             *
             * Harmless on an endpoint that is not halted, since it is the
             * same control request either way. It also resets the data
             * toggle, which is what makes it the right thing after any
             * abandoned transfer rather than only after a stall.
             */
            VoidResult clear_stall(std::uint8_t endpoint)
            {
                const int failed = libusb_clear_halt(handle_, endpoint);

                if (failed != 0)
                {
                    return error_from(failed, "clearing a stalled channel");
                }

                return {};
            }

            std::uint8_t endpoint_for(Channel channel) const noexcept
            {
                switch (channel)
                {
                case Channel::Command:
                    return endpoints_.command;
                case Channel::Reply:
                    return endpoints_.reply;
                case Channel::Trace:
                    return endpoints_.trace;
                }

                return endpoints_.command;
            }

            Result<std::size_t> transfer(std::uint8_t endpoint, std::uint8_t *data,
                                         std::size_t size, Timeout timeout, const char *context)
            {
                if (size == 0)
                {
                    return Error(ErrorCode::InvalidArgument, context);
                }

                int moved = 0;
                const int failed = libusb_bulk_transfer(handle_, endpoint, data,
                                                        static_cast<int>(size), &moved,
                                                        static_cast<unsigned int>(timeout));

                if (failed != 0)
                {
                    Error failure = error_from(failed, context);

                    /*
                     * Leave nothing behind. A stall is device-side state that
                     * outlives this call, this transport and this process, so
                     * the endpoint is cleared here rather than left for
                     * whoever opens the probe next. This is what transport.h
                     * promises and what the WinUSB side already does; the
                     * assumption that libusb got it from the kernel for free
                     * turned out to be wrong.
                     *
                     * Either way the caller hears about the original failure,
                     * not about the clearing.
                     */
                    if (!clear_stall(endpoint).ok())
                    {
                        STLINK_LOG_WRN("could not clear endpoint %#04x after a failed transfer",
                                       endpoint);
                    }

                    return failure;
                }

                return static_cast<std::size_t>(moved);
            }

            libusb_context *context_ = nullptr;
            libusb_device_handle *handle_ = nullptr;
            ProbeAddress address_;
            Endpoints endpoints_;
        };
    } // namespace

    std::vector<ProbeAddress> enumerate_probes(std::uint16_t vid)
    {
        std::vector<ProbeAddress> found;

        libusb_context *context = nullptr;

        if (const int failed = libusb_init(&context))
        {
            STLINK_LOG_ERR("USB is unavailable: libusb says %s", libusb_error_name(failed));

            return found;
        }

        libusb_device **devices = nullptr;
        const ssize_t count = libusb_get_device_list(context, &devices);

        for (ssize_t i = 0; i < count; ++i)
        {
            libusb_device_descriptor descriptor{};

            if (libusb_get_device_descriptor(devices[i], &descriptor) != 0)
            {
                continue;
            }

            if (descriptor.idVendor != vid)
            {
                continue;
            }

            ProbeAddress probe;
            probe.vid = descriptor.idVendor;
            probe.pid = descriptor.idProduct;
            probe.location = location_of(devices[i]);

            /*
             * The serial lives in a string descriptor, so reading it means
             * opening the device. Nothing else here does, and the handle is
             * closed again immediately.
             */
            libusb_device_handle *handle = nullptr;

            if (libusb_open(devices[i], &handle) == 0)
            {
                probe.serial = read_serial(handle, descriptor);
                libusb_close(handle);
            }
            else
            {
                STLINK_LOG_DBG("could not read the serial of %04x:%04x at %s",
                               probe.vid, probe.pid, probe.location.c_str());
            }

            found.push_back(std::move(probe));
        }

        if (devices != nullptr)
        {
            libusb_free_device_list(devices, 1);
        }

        libusb_exit(context);

        return found;
    }

    Result<std::unique_ptr<ITransport>> open_transport(const ProbeAddress &probe)
    {
        libusb_context *context = nullptr;

        if (const int failed = libusb_init(&context))
        {
            return error_from(failed, "starting libusb");
        }

        libusb_device **devices = nullptr;
        const ssize_t count = libusb_get_device_list(context, &devices);

        libusb_device *wanted = nullptr;

        for (ssize_t i = 0; i < count; ++i)
        {
            if (location_of(devices[i]) == probe.location)
            {
                wanted = devices[i];
                break;
            }
        }

        if (wanted == nullptr)
        {
            libusb_free_device_list(devices, 1);
            libusb_exit(context);

            return Error(ErrorCode::NotFound, "finding the programmer again");
        }

        libusb_device_handle *handle = nullptr;
        const int opened = libusb_open(wanted, &handle);

        libusb_free_device_list(devices, 1);

        if (opened != 0)
        {
            libusb_exit(context);

            return error_from(opened, "opening the programmer");
        }

        /* A kernel driver holding the interface has to let go of it first. */
        if (libusb_kernel_driver_active(handle, kInterface) == 1)
        {
            if (const int failed = libusb_detach_kernel_driver(handle, kInterface))
            {
                STLINK_LOG_DBG("could not detach the kernel driver: libusb says %s",
                               libusb_error_name(failed));
            }
        }

        if (const int failed = libusb_claim_interface(handle, kInterface))
        {
            libusb_close(handle);
            libusb_exit(context);

            return error_from(failed, "claiming the programmer's interface");
        }

        return std::unique_ptr<ITransport>(new UsbTransport(context, handle, probe));
    }
} // namespace stlink
