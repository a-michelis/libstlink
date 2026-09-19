/**
 * @file    usb_transport.cpp
 * @brief   Reaching a programmer over USB, through WinUSB.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * WinUSB ships with Windows and is what the ST driver package binds, so a
 * Windows build needs no third party USB library at all.
 */

#include <stlink/transport.h>

#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <windows.h>

#include <setupapi.h>
#include <winusb.h>

#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /*
         * The device interface an ST-LINK publishes for its debug endpoint. A
         * V2 gets it from the driver package, a V3 from the MS OS descriptors
         * in its own firmware, and on a composite V3 it belongs to the first
         * interface rather than to the device. Enumerating it therefore finds
         * the debug endpoint of either generation directly, with no device
         * tree to walk and no composite parent to climb to.
         */
        constexpr GUID kStlinkInterface = {
            0xDBCE1CD9, 0xA320, 0x4B51, {0xA3, 0x65, 0xA0, 0xC3, 0xF3, 0xC5, 0xFB, 0x29}};

        /* Windows has no language independent form of a string descriptor request. */
        constexpr USHORT kLangIdEnglishUs = 0x0409;

        constexpr std::size_t kSerialBytes = 12;
        constexpr std::size_t kSerialDigits = kSerialBytes * 2;

        /** @brief Which endpoints a programmer uses, decided by its product id. */
        struct Endpoints
        {
            UCHAR command = 0;
            UCHAR reply = 0;
            UCHAR trace = 0;
        };

        bool is_v3(std::uint16_t pid) noexcept
        {
            switch (pid)
            {
            case 0x374d:
            case 0x374e:
            case 0x374f:
            case 0x3753:
            case 0x3754:
            case 0x3757:
                return true;
            default:
                return false;
            }
        }

        Endpoints endpoints_for(std::uint16_t pid) noexcept
        {
            Endpoints ep;

            ep.reply = 0x81;

            if (is_v3(pid))
            {
                ep.command = 0x01;
                ep.trace = 0x82;
            }
            else
            {
                ep.command = 0x02;
                ep.trace = 0x83;
            }

            return ep;
        }

        ErrorCode code_for(DWORD error) noexcept
        {
            switch (error)
            {
            case ERROR_SEM_TIMEOUT:
            case ERROR_TIMEOUT:
                return ErrorCode::Timeout;
            case ERROR_ACCESS_DENIED:
                return ErrorCode::AccessDenied;
            case ERROR_SHARING_VIOLATION:
                return ErrorCode::Busy;
            case ERROR_DEVICE_NOT_CONNECTED:
            case ERROR_NO_SUCH_DEVICE:
            case ERROR_FILE_NOT_FOUND:
            case ERROR_PATH_NOT_FOUND:
                return ErrorCode::Disconnected;
            case ERROR_NOT_ENOUGH_MEMORY:
            case ERROR_OUTOFMEMORY:
                return ErrorCode::OutOfMemory;
            case ERROR_INVALID_PARAMETER:
                return ErrorCode::InvalidArgument;
            case ERROR_MORE_DATA:
                return ErrorCode::Protocol;
            default:
                return ErrorCode::IO;
            }
        }

        /**
         * @brief Turn the last Win32 failure into an error.
         *
         * Called immediately after the failing call, since anything else may
         * overwrite what GetLastError() reports.
         */
        Error last_error(const char *context)
        {
            const DWORD error = GetLastError();

            STLINK_LOG_DBG("%s: Windows error %lu", context, static_cast<unsigned long>(error));

            return Error(code_for(error), context);
        }

        /**
         * @brief Pull the identifiers out of a device interface path.
         *
         * A path reads \\?\usb#vid_0483&pid_374e&mi_00#<instance>#<guid>, and
         * Windows writes it in lower case, but nothing promises that, so the
         * search is done without regard to case.
         */
        bool ids_from_path(const std::string &path, std::uint16_t &vid, std::uint16_t &pid)
        {
            std::string lowered;
            lowered.reserve(path.size());

            for (const char c : path)
            {
                lowered.push_back(static_cast<char>(
                    std::tolower(static_cast<unsigned char>(c))));
            }

            const std::size_t vid_at = lowered.find("vid_");
            const std::size_t pid_at = lowered.find("pid_");

            if ((vid_at == std::string::npos) || (pid_at == std::string::npos))
            {
                return false;
            }

            unsigned int vid_value = 0;
            unsigned int pid_value = 0;

            if (std::sscanf(lowered.c_str() + vid_at + 4, "%4x", &vid_value) != 1)
            {
                return false;
            }

            if (std::sscanf(lowered.c_str() + pid_at + 4, "%4x", &pid_value) != 1)
            {
                return false;
            }

            vid = static_cast<std::uint16_t>(vid_value);
            pid = static_cast<std::uint16_t>(pid_value);

            return true;
        }

        /** @brief One ST-LINK debug interface, as SetupAPI reported it. */
        struct Interface
        {
            std::string path;
            std::string location;
            std::uint16_t vid = 0;
            std::uint16_t pid = 0;
        };

        /** @brief Every ST-LINK debug interface attached, without opening any. */
        std::vector<Interface> find_interfaces(std::uint16_t vid)
        {
            std::vector<Interface> found;

            HDEVINFO set = SetupDiGetClassDevsA(&kStlinkInterface, nullptr, nullptr,
                                                DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

            if (set == INVALID_HANDLE_VALUE)
            {
                STLINK_LOG_DBG("no ST-LINK interfaces: Windows error %lu",
                               static_cast<unsigned long>(GetLastError()));

                return found;
            }

            for (DWORD index = 0;; ++index)
            {
                SP_DEVICE_INTERFACE_DATA interface_data{};
                interface_data.cbSize = sizeof(interface_data);

                if (!SetupDiEnumDeviceInterfaces(set, nullptr, &kStlinkInterface, index,
                                                 &interface_data))
                {
                    break;
                }

                /* Asking with no buffer is how the length of the path is discovered. */
                DWORD needed = 0;
                SetupDiGetDeviceInterfaceDetailA(set, &interface_data, nullptr, 0, &needed,
                                                 nullptr);

                if (needed == 0)
                {
                    continue;
                }

                std::vector<char> buffer(needed, '\0');
                auto *detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A *>(buffer.data());
                detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

                SP_DEVINFO_DATA device_data{};
                device_data.cbSize = sizeof(device_data);

                if (!SetupDiGetDeviceInterfaceDetailA(set, &interface_data, detail, needed,
                                                      nullptr, &device_data))
                {
                    continue;
                }

                Interface found_one;
                found_one.path = detail->DevicePath;

                if (!ids_from_path(found_one.path, found_one.vid, found_one.pid))
                {
                    STLINK_LOG_DBG("skipping an interface whose path names no identifiers: %s",
                                   found_one.path.c_str());
                    continue;
                }

                if (found_one.vid != vid)
                {
                    continue;
                }

                /* Reads as Port_#0002.Hub_#0004, which is what Device Manager shows. */
                std::array<char, 128> location{};

                if (SetupDiGetDeviceRegistryPropertyA(set, &device_data,
                                                      SPDRP_LOCATION_INFORMATION, nullptr,
                                                      reinterpret_cast<PBYTE>(location.data()),
                                                      static_cast<DWORD>(location.size() - 1),
                                                      nullptr))
                {
                    found_one.location = location.data();
                }

                found.push_back(std::move(found_one));
            }

            SetupDiDestroyDeviceInfoList(set);

            return found;
        }

        /** @brief What CreateFile and WinUsb_Initialize produce together. */
        struct OpenInterface
        {
            HANDLE file = INVALID_HANDLE_VALUE;
            WINUSB_INTERFACE_HANDLE winusb = nullptr;

            void close() noexcept
            {
                if (winusb != nullptr)
                {
                    WinUsb_Free(winusb);
                    winusb = nullptr;
                }

                if (file != INVALID_HANDLE_VALUE)
                {
                    CloseHandle(file);
                    file = INVALID_HANDLE_VALUE;
                }
            }
        };

        Result<OpenInterface> open_path(const std::string &path)
        {
            OpenInterface opened;

            /* WinUsb_Initialize requires a handle opened for overlapped use. */
            opened.file = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

            if (opened.file == INVALID_HANDLE_VALUE)
            {
                return last_error("opening the programmer");
            }

            if (!WinUsb_Initialize(opened.file, &opened.winusb))
            {
                Error why = last_error("taking over the programmer's interface");
                opened.close();

                return why;
            }

            return opened;
        }

        /**
         * @brief The serial written out, from a string descriptor.
         *
         * Older programmers answer with the twelve raw bytes rather than the
         * twenty four characters the descriptor promises. Both are accepted
         * and both come back written out, so one adapter has one serial
         * whichever firmware it runs.
         */
        std::string serial_from_descriptor(const std::uint8_t *raw, std::size_t size)
        {
            /* A string descriptor is a length, a type, then UTF-16 characters. */
            if ((size < 2) || (raw[1] != USB_STRING_DESCRIPTOR_TYPE))
            {
                return {};
            }

            const std::size_t characters = (size - 2) / 2;
            std::string text;
            text.reserve(characters);

            for (std::size_t i = 0; i < characters; ++i)
            {
                text.push_back(static_cast<char>(raw[2 + (i * 2)]));
            }

            if (text.size() == kSerialDigits)
            {
                return text;
            }

            if (text.size() == kSerialBytes)
            {
                std::string written;
                written.reserve(kSerialDigits);

                for (const char c : text)
                {
                    char pair[3] = {};
                    std::snprintf(pair, sizeof(pair), "%02X",
                                  static_cast<unsigned>(static_cast<unsigned char>(c)));
                    written += pair;
                }

                return written;
            }

            return {};
        }

        /** @brief The serial of one interface, which costs an open to read. */
        std::string read_serial(const std::string &path)
        {
            auto opened = open_path(path);

            if (!opened.ok())
            {
                STLINK_LOG_DBG("could not read a serial: %s", opened.error().describe().c_str());

                return {};
            }

            OpenInterface device = opened.value();

            USB_DEVICE_DESCRIPTOR descriptor{};
            ULONG moved = 0;

            if (!WinUsb_GetDescriptor(device.winusb, USB_DEVICE_DESCRIPTOR_TYPE, 0, 0,
                                      reinterpret_cast<PUCHAR>(&descriptor), sizeof(descriptor),
                                      &moved) ||
                (descriptor.iSerialNumber == 0))
            {
                device.close();

                return {};
            }

            std::array<std::uint8_t, 256> raw{};

            if (!WinUsb_GetDescriptor(device.winusb, USB_STRING_DESCRIPTOR_TYPE,
                                      descriptor.iSerialNumber, kLangIdEnglishUs, raw.data(),
                                      static_cast<ULONG>(raw.size()), &moved))
            {
                device.close();

                return {};
            }

            device.close();

            return serial_from_descriptor(raw.data(), moved);
        }

        /** @brief A programmer reached through WinUSB. */
        class UsbTransport final : public ITransport
        {
        public:
            UsbTransport(OpenInterface device, ProbeAddress address) noexcept
                : device_(device), address_(std::move(address)),
                  endpoints_(endpoints_for(address_.pid))
            {
            }

            ~UsbTransport() override
            {
                device_.close();
            }

            const ProbeAddress &address() const noexcept override
            {
                return address_;
            }

            Result<std::size_t> send(const std::uint8_t *data, std::size_t size,
                                     Timeout timeout) override
            {
                if (size == 0)
                {
                    return Error(ErrorCode::InvalidArgument, "sending a command");
                }

                if (auto failed = set_timeout(endpoints_.command, timeout); !failed.ok())
                {
                    return failed.error();
                }

                ULONG moved = 0;

                /* WinUsb_WritePipe does not write through this, despite the type. */
                if (!WinUsb_WritePipe(device_.winusb, endpoints_.command,
                                      const_cast<PUCHAR>(data), static_cast<ULONG>(size), &moved,
                                      nullptr))
                {
                    Error why = last_error("sending a command");
                    recover(endpoints_.command);

                    return why;
                }

                return static_cast<std::size_t>(moved);
            }

            Result<std::size_t> receive(std::uint8_t *data, std::size_t size,
                                        Timeout timeout) override
            {
                return read(endpoints_.reply, data, size, timeout, "reading a reply");
            }

            Result<std::size_t> receive_trace(std::uint8_t *data, std::size_t size,
                                              Timeout timeout) override
            {
                return read(endpoints_.trace, data, size, timeout, "reading trace output");
            }

            VoidResult reset(Channel channel) override
            {
                const UCHAR endpoint = endpoint_for(channel);

                if (!WinUsb_ResetPipe(device_.winusb, endpoint))
                {
                    return last_error("clearing a stalled channel");
                }

                return {};
            }

        private:
            UCHAR endpoint_for(Channel channel) const noexcept
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

            /**
             * @brief Give a pipe the timeout the caller asked for.
             *
             * WinUSB waits forever by default, and a synchronous transfer on
             * a pipe with no timeout cannot be cancelled from the thread that
             * started it, so this has to succeed before any transfer runs.
             */
            VoidResult set_timeout(UCHAR endpoint, Timeout timeout)
            {
                ULONG value = timeout;

                if (!WinUsb_SetPipePolicy(device_.winusb, endpoint, PIPE_TRANSFER_TIMEOUT,
                                          sizeof(value), &value))
                {
                    return last_error("giving a channel its timeout");
                }

                return {};
            }

            /**
             * @brief Put a pipe back in a known state after a failure.
             *
             * Whatever the abandoned transfer left queued must not turn up as
             * the answer to the next one, which is what the contract promises
             * and what resetting the pipe delivers.
             */
            void recover(UCHAR endpoint) noexcept
            {
                if (!WinUsb_ResetPipe(device_.winusb, endpoint))
                {
                    STLINK_LOG_DBG("could not reset a channel after a failure: Windows error %lu",
                                   static_cast<unsigned long>(GetLastError()));
                }
            }

            Result<std::size_t> read(UCHAR endpoint, std::uint8_t *data, std::size_t size,
                                     Timeout timeout, const char *context)
            {
                if (size == 0)
                {
                    return Error(ErrorCode::InvalidArgument, context);
                }

                if (auto failed = set_timeout(endpoint, timeout); !failed.ok())
                {
                    return failed.error();
                }

                ULONG moved = 0;

                if (!WinUsb_ReadPipe(device_.winusb, endpoint, data, static_cast<ULONG>(size),
                                     &moved, nullptr))
                {
                    Error why = last_error(context);
                    recover(endpoint);

                    return why;
                }

                return static_cast<std::size_t>(moved);
            }

            OpenInterface device_;
            ProbeAddress address_;
            Endpoints endpoints_;
        };

        /**
         * @brief Pin a freshly opened interface to the behaviour the protocol needs.
         *
         * These are WinUSB's documented defaults except for the first, which
         * is not, and a driver package may ship different ones in its INF, so
         * all three are set rather than assumed.
         */
        VoidResult configure(const OpenInterface &device, const Endpoints &endpoints)
        {
            const UCHAR off = FALSE;

            struct Policy
            {
                UCHAR endpoint;
                ULONG name;
                const char *what;
            };

            const std::array<Policy, 5> policies{{
                /*
                 * A reply longer than the buffer offered must fail. WinUSB
                 * defaults this to TRUE, and with AUTO_FLUSH left FALSE it
                 * then keeps the excess and hands it back at the head of the
                 * next read, which desynchronises the command stream for good
                 * and reports nothing. libusb fails such a transfer, and so
                 * must this.
                 */
                {endpoints.reply, ALLOW_PARTIAL_READS, "refusing oversized replies"},
                {endpoints.trace, ALLOW_PARTIAL_READS, "refusing oversized trace reads"},
                /* A short reply completes the transfer rather than continuing it. */
                {endpoints.reply, IGNORE_SHORT_PACKETS, "completing on a short reply"},
                {endpoints.trace, IGNORE_SHORT_PACKETS, "completing on short trace output"},
                /* No zero length packet after a command that fills a packet. */
                {endpoints.command, SHORT_PACKET_TERMINATE, "not padding a command"},
            }};

            for (const Policy &policy : policies)
            {
                if (!WinUsb_SetPipePolicy(device.winusb, policy.endpoint, policy.name,
                                          sizeof(off), const_cast<UCHAR *>(&off)))
                {
                    return last_error(policy.what);
                }
            }

            return {};
        }
    } // namespace

    std::vector<ProbeAddress> enumerate_probes(std::uint16_t vid)
    {
        std::vector<ProbeAddress> found;

        for (const Interface &attached : find_interfaces(vid))
        {
            ProbeAddress probe;
            probe.vid = attached.vid;
            probe.pid = attached.pid;
            probe.location = attached.location;
            probe.serial = read_serial(attached.path);

            found.push_back(std::move(probe));
        }

        return found;
    }

    Result<std::unique_ptr<ITransport>> open_transport(const ProbeAddress &probe)
    {
        for (const Interface &attached : find_interfaces(probe.vid))
        {
            if (attached.location != probe.location)
            {
                continue;
            }

            auto opened = open_path(attached.path);

            if (!opened.ok())
            {
                return opened.error();
            }

            OpenInterface device = opened.value();
            const Endpoints endpoints = endpoints_for(probe.pid);

            if (auto failed = configure(device, endpoints); !failed.ok())
            {
                Error why = failed.error();
                device.close();

                return why;
            }

            return std::unique_ptr<ITransport>(new UsbTransport(device, probe));
        }

        return Error(ErrorCode::NotFound, "finding the programmer again");
    }
} // namespace stlink
