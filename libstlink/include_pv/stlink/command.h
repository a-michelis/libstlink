/**
 * @file    command.h
 * @brief   Sending a command to the programmer and reading its reply.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. One exchange: a command goes out, a reply comes back, and the
 * first byte of that reply says whether the programmer could do what was
 * asked. Everything above this builds command blocks and reads fields out of
 * replies; everything below it moves bytes.
 *
 * How a command block is framed is the programmer's business and not this
 * file's: a V1 wraps one in a SCSI command block, a V2 and a V3 send it as it
 * is. What is here is the part all three share.
 */

#ifndef STLINK_PV_COMMAND_H
#define STLINK_PV_COMMAND_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include <stlink/result.h>
#include <stlink/transport.h>

namespace stlink
{
    /**
     * @brief What the programmer said about the command it just ran.
     *
     * The first byte of a reply, for every command that answers with one.
     */
    enum class CommandStatus : std::uint8_t
    {
        Ok = 0x80,
        Fault = 0x81,
        Write = 0x0c,
        WriteVerify = 0x0d,
        ApWait = 0x10,
        ApFault = 0x11,
        ApError = 0x12,
        DpWait = 0x14,
        DpFault = 0x15,
        DpError = 0x16,
    };

    /** @brief What a status means, as a phrase that completes "the programmer reports". */
    const char *to_string(CommandStatus status) noexcept;

    /** @brief Whether a status is the target asking to be given a moment. */
    constexpr bool is_wait(CommandStatus status) noexcept
    {
        return (status == CommandStatus::ApWait) || (status == CommandStatus::DpWait);
    }

    /**
     * @brief How much of a reply to insist on.
     *
     * Commands differ in what they promise. Some answer with a status byte,
     * some answer with data of a known length and no status at all, and a few
     * answer with whatever they have.
     */
    enum class Check : std::uint8_t
    {
        None,        /**< Take whatever came back. */
        ReplyLength, /**< The reply must fill the buffer offered. */
        Status,      /**< The first byte must say Ok. */
        Retry,       /**< And a target asking to wait is given three tries. */
    };

    /**
     * @brief The command and reply exchange, over a transport it owns.
     *
     * One instance belongs to one thread at a time.
     */
    class CommandChannel
    {
    public:
        explicit CommandChannel(std::unique_ptr<ITransport> transport) noexcept;

        CommandChannel(const CommandChannel &) = delete;
        CommandChannel &operator=(const CommandChannel &) = delete;

        /** @brief The transport underneath, for the few things that need it. */
        [[nodiscard]] ITransport &transport() noexcept;

        /**
         * @brief Send a command and read its reply.
         *
         * @param command      the block to send
         * @param command_size how long it is
         * @param reply        where the answer goes, which may be @p command
         * @param reply_size   how much room there is, zero for no reply at all
         * @param check        how much of the reply to insist on
         * @param what         the command's name, for diagnostics
         *
         * @return how many bytes the reply held
         *
         * A target that asks to wait is retried under Check::Retry, three
         * times, waiting 1ms then 2ms then 4ms. The whole command is sent
         * again each time, which is what the programmer expects.
         */
        [[nodiscard]] Result<std::size_t> exchange(const std::uint8_t *command,
                                                    std::size_t command_size,
                                                    std::uint8_t *reply, std::size_t reply_size,
                                                    Check check, const char *what);

        /** @brief Send a command that answers with nothing. */
        [[nodiscard]] VoidResult send(const std::uint8_t *command, std::size_t command_size,
                                       const char *what);

    private:
        std::unique_ptr<ITransport> transport_;
    };
} // namespace stlink

#endif // STLINK_PV_COMMAND_H
