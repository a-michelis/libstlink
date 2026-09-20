/**
 * @file    programmer.cpp
 * @brief   Connecting to a programmer and finding out what it is.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The version exchange happens here rather than in a programmer, because
 * until it has happened there is no way to know which programmer to build.
 * What makes that possible is that the product id already says how to ask:
 * a V3 answers a different command, and every other generation answers the
 * same one in the same sixteen byte block.
 */

#include <stlink/programmer.h>

#include <cstring>

#include <stlink/command.h>
#include <stlink/log.h>
#include <stlink/programmer_v1.h>
#include <stlink/programmer_v2.h>
#include <stlink/programmer_v3.h>
#include <stlink/protocol.h>

namespace stlink
{
    namespace
    {
        /** @brief What a V2 and a V3 send. A V1 frames this differently. */
        constexpr std::size_t kPlainBlockSize = 16;

        /**
         * @brief Ask the programmer what it is.
         *
         * Built by hand rather than through a programmer's own framing: there
         * is no programmer yet, and this is the exchange that decides which
         * one there will be.
         */
        Result<VersionReport> ask_version(CommandChannel &channel, Generation generation)
        {
            std::uint8_t block[kPlainBlockSize] = {};

            block[0] = static_cast<std::uint8_t>(generation == Generation::V3
                                                     ? Command::GetVersionApiV3
                                                     : Command::GetVersion);

            const std::size_t expected = version_reply_size(generation);

            std::uint8_t reply[16] = {};

            auto sent = channel.exchange(block, sizeof(block), reply, expected,
                                         Check::ReplyLength, "asking the programmer its version");

            if (!sent.ok())
            {
                return sent.error();
            }

            return parse_version(generation, reply, sent.value());
        }
    } // namespace

    Result<std::unique_ptr<IProgrammer>> open_programmer(const ProbeAddress &probe)
    {
        const Generation generation = generation_of(probe.pid);

        if (probe.vid != kVendorId || generation == Generation::Unknown)
        {
            return Error(ErrorCode::NotFound, "this device is not an ST-LINK");
        }

        auto opened = open_transport(probe);

        if (!opened.ok())
        {
            return opened.error().wrap(ErrorCode::IO, "connecting to the programmer");
        }

        /*
         * The channel carries the exchange that decides which programmer to
         * build, and is then moved into it: the connection is made once and
         * never reopened.
         */
        CommandChannel channel(opened.value());

        auto asked = ask_version(channel, generation);

        if (!asked.ok())
        {
            return asked.error().wrap(ErrorCode::Protocol, "identifying the programmer");
        }

        const VersionReport report = asked.value();

        STLINK_LOG_INF("ST-LINK V%u, debug firmware J%u, at %s", report.version.major,
                       report.version.jtag,
                       probe.location.empty() ? "an unknown place" : probe.location.c_str());

        std::unique_ptr<IProgrammer> programmer;

        switch (report.api)
        {
        case ProtocolApi::V3:
            programmer.reset(new ProgrammerV3(std::move(channel), report));
            break;

        case ProtocolApi::V2:
            programmer.reset(new ProgrammerV2(std::move(channel), report));
            break;

        case ProtocolApi::V1:
            programmer.reset(new ProgrammerV1(std::move(channel), report));
            break;

        default:
            return Error(ErrorCode::Internal, "the programmer named a command set we do not have");
        }

        /*
         * A programmer sitting in firmware update mode answers its
         * bootloader's command set and stalls on everything else, so a caller
         * handed one as it is would find every question failing for no
         * visible reason. Leaving it is not invasive: it puts the probe into
         * the mode it is normally in, and it is what the C does at connect.
         *
         * Being unable to ask is not fatal. The programmer is very likely
         * usable, and refusing to return one over a question we could not put
         * would be worse than letting the caller find out.
         */
        auto mode = programmer->mode();

        if (!mode.ok())
        {
            STLINK_LOG_WRN("could not ask the programmer what mode it is in: %s",
                           mode.error().describe().c_str());
        }
        else if (mode.value() == ProgrammerMode::Dfu)
        {
            STLINK_LOG_INF("the programmer is in firmware update mode, leaving it");

            auto left = programmer->exit_dfu();

            if (!left.ok())
            {
                return left.error().wrap(ErrorCode::IO, "leaving firmware update mode");
            }
        }

        return std::move(programmer);
    }
} // namespace stlink
