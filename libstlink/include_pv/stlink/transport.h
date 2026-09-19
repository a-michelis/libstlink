/**
 * @file    transport.h
 * @brief   How bytes reach the programmer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. A transport moves bytes and knows nothing about debugging: whether
 * they are a command, a reply or trace output is the programmer's concern.
 *
 * Three channels, because that is what the ST-LINK protocol uses: a command
 * goes out, a reply comes back, and trace output arrives on its own. They are
 * named rather than numbered so that no endpoint number, which is a USB idea,
 * appears in this interface.
 *
 * Lifetime is ownership. A transport is connected when it is constructed and
 * disconnected when it is destroyed, so there is no open() or close() to
 * forget and no half-open state to represent.
 */

#ifndef STLINK_PV_TRANSPORT_H
#define STLINK_PV_TRANSPORT_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <stlink/result.h>

namespace stlink
{
    /** @brief Which of the protocol's channels a transfer concerns. */
    enum class Channel : std::uint8_t
    {
        Command, /**< Out: the command being issued. */
        Reply,   /**< In: the answer to it. */
        Trace,   /**< In: trace output, arriving on its own schedule. */
    };

    /** @brief How long to wait for a transfer, in milliseconds. */
    using Timeout = std::uint32_t;

    /** @brief What the ST-LINK protocol uses by default. */
    inline constexpr Timeout kDefaultTimeout = 3000;

    /**
     * @brief One attached probe, as discovery found it.
     *
     * Everything here is known without talking to the device. What kind of
     * programmer it is follows from the product id, and that is decided a
     * layer up rather than here.
     */
    struct ProbeAddress
    {
        std::uint16_t vid = 0;
        std::uint16_t pid = 0;

        /** @brief Empty when the probe did not report one. */
        std::string serial;

        /**
         * @brief Where it is attached, in whatever form the transport uses.
         *
         * A bus and address, or a device path. For diagnostics, and for
         * reaching this probe again while it stays plugged in. Not stable
         * across a replug, and never parsed.
         */
        std::string location;
    };

    /**
     * @brief A connection to one programmer.
     *
     * Constructed connected, destroyed disconnected. One instance belongs to
     * one thread at a time; nothing here is internally synchronised.
     */
    class ITransport
    {
    public:
        virtual ~ITransport() = default;

        ITransport(const ITransport &) = delete;
        ITransport &operator=(const ITransport &) = delete;

        /** @brief Where this transport is connected. */
        [[nodiscard]] virtual const ProbeAddress &address() const noexcept = 0;

        /**
         * @brief Send a command.
         *
         * @return how many bytes were accepted, which may be fewer than offered
         */
        [[nodiscard]] virtual Result<std::size_t> send(const std::uint8_t *data, std::size_t size,
                                                       Timeout timeout) = 0;

        /**
         * @brief Read a reply.
         *
         * A reply shorter than the buffer is not an error and reports what
         * arrived. One longer than the buffer is an error: a reply that does
         * not fit means the exchange is no longer understood.
         *
         * @return how many bytes arrived
         */
        [[nodiscard]] virtual Result<std::size_t> receive(std::uint8_t *data, std::size_t size,
                                                          Timeout timeout) = 0;

        /** @brief Read whatever trace output is waiting. Same rules as receive(). */
        [[nodiscard]] virtual Result<std::size_t> receive_trace(std::uint8_t *data, std::size_t size,
                                                                Timeout timeout) = 0;

        /**
         * @brief Clear a stalled channel and discard what it had queued.
         *
         * After any failed transfer, nothing belonging to it may turn up in a
         * later one. Where a transport cannot promise that on its own, this is
         * how the programmer restores it.
         */
        [[nodiscard]] virtual VoidResult reset(Channel channel) = 0;

    protected:
        ITransport() = default;
    };

    /**
     * @brief Every attached programmer with this vendor id.
     *
     * Implemented by whichever backend was compiled in. Nothing is opened for
     * longer than it takes to read a serial, so this is safe to call while
     * other programmers are in use.
     */
    [[nodiscard]] std::vector<ProbeAddress> enumerate_probes(std::uint16_t vid);

    /**
     * @brief Connect to one of the programmers enumerate_probes() reported.
     *
     * The transport is connected when it is returned and disconnected when it
     * is destroyed.
     */
    [[nodiscard]] Result<std::unique_ptr<ITransport>> open_transport(const ProbeAddress &probe);
} // namespace stlink

#endif // STLINK_PV_TRANSPORT_H
