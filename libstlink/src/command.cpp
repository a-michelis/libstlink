/**
 * @file    command.cpp
 * @brief   Sending a command to the programmer and reading its reply.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/command.h>

#include <chrono>
#include <thread>
#include <utility>

#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /* What the C waited, and what the programmer is used to being given. */
        constexpr int kWaitRetries = 3;
        constexpr std::chrono::microseconds kFirstWait{1000};

        /** @brief How long to wait before the nth retry: 1ms, then 2ms, then 4ms. */
        std::chrono::microseconds wait_before(int retry) noexcept
        {
            return kFirstWait * (1 << retry);
        }
    } // namespace

    const char *to_string(CommandStatus status) noexcept
    {
        switch (status)
        {
        case CommandStatus::Ok:
            return "success";
        case CommandStatus::Fault:
            return "a fault";
        case CommandStatus::Write:
            return "a write error";
        case CommandStatus::WriteVerify:
            return "a write that did not verify";
        case CommandStatus::ApWait:
            return "the access port asking to wait";
        case CommandStatus::ApFault:
            return "an access port fault";
        case CommandStatus::ApError:
            return "an access port error";
        case CommandStatus::DpWait:
            return "the debug port asking to wait";
        case CommandStatus::DpFault:
            return "a debug port fault";
        case CommandStatus::DpError:
            return "a debug port error";
        }

        return "something it did not name";
    }

    CommandChannel::CommandChannel(std::unique_ptr<ITransport> transport) noexcept
        : transport_(std::move(transport))
    {
    }

    ITransport &CommandChannel::transport() noexcept
    {
        return *transport_;
    }

    VoidResult CommandChannel::send(const std::uint8_t *command, std::size_t command_size,
                                    const char *what)
    {
        auto sent = transport_->send(command, command_size, kDefaultTimeout);

        if (!sent.ok())
        {
            return sent.error().wrap(ErrorCode::IO, what);
        }

        if (sent.value() != command_size)
        {
            /*
             * The programmer took some of the command. Nothing in the protocol
             * says what it will do with a partial one, so say so and let the
             * reply, or the absence of one, decide.
             */
            STLINK_LOG_WRN("%s: the programmer took %zu bytes of a %zu byte command", what,
                           sent.value(), command_size);
        }

        return {};
    }

    Result<std::size_t> CommandChannel::exchange(const std::uint8_t *command,
                                                 std::size_t command_size, std::uint8_t *reply,
                                                 std::size_t reply_size, Check check,
                                                 const char *what)
    {
        for (int retry = 0;; ++retry)
        {
            if (auto failed = send(command, command_size, what); !failed.ok())
            {
                return failed.error();
            }

            if (reply_size == 0)
            {
                return std::size_t{0};
            }

            auto received = transport_->receive(reply, reply_size, kDefaultTimeout);

            if (!received.ok())
            {
                return received.error().wrap(ErrorCode::IO, what);
            }

            const std::size_t arrived = received.value();

            if ((check == Check::Status) || (check == Check::Retry))
            {
                const auto status = static_cast<CommandStatus>(reply[0]);

                if (status != CommandStatus::Ok)
                {
                    /*
                     * A target that is busy asks to be given a moment. The
                     * whole command goes again, which is what the programmer
                     * expects; only when it keeps asking does this fail.
                     */
                    if (is_wait(status) && (check == Check::Retry) && (retry < kWaitRetries))
                    {
                        const auto pause = wait_before(retry);

                        STLINK_LOG_DBG("%s: %s, waiting %lld us and trying again", what,
                                       to_string(status),
                                       static_cast<long long>(pause.count()));

                        std::this_thread::sleep_for(pause);

                        continue;
                    }

                    STLINK_LOG_DBG("%s: the programmer reports %s", what, to_string(status));

                    return Error(ErrorCode::TargetRefused, to_string(status)).wrap(
                        ErrorCode::TargetRefused, what);
                }
            }

            if ((check == Check::ReplyLength) && (arrived != reply_size))
            {
                STLINK_LOG_DBG("%s: the reply was %zu bytes, not the %zu expected", what, arrived,
                               reply_size);

                return Error(ErrorCode::Protocol, "a reply of the wrong length").wrap(
                    ErrorCode::Protocol, what);
            }

            return arrived;
        }
    }
} // namespace stlink
