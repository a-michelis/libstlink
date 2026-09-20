/**
 * @file    programmer_v3.cpp
 * @brief   A V3, which speaks the V2 command set with four exceptions.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/programmer_v3.h>

#include <algorithm>
#include <vector>

#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /** @brief The most an unaligned write may carry on a V3. */
        constexpr std::size_t kByteWriteLimitV3 = 512;

        /** @brief What a V3 will buffer of the target's trace output. */
        constexpr std::size_t kTraceBufferV3 = 8192;

        /** @brief The most rates a V3 will list. */
        constexpr std::size_t kMaxRates = 10;

        /** @brief Which wire the rate is for. Zero is SWD; JTAG is not entered over USB. */
        constexpr std::uint8_t kSwd = 0;

        /** @brief What a V3 is set to when the caller has no opinion. */
        constexpr std::uint32_t kDefaultRateV3 = 1000000;

        std::uint32_t read_u32(const std::uint8_t *from) noexcept
        {
            return static_cast<std::uint32_t>(from[0]) |
                   (static_cast<std::uint32_t>(from[1]) << 8) |
                   (static_cast<std::uint32_t>(from[2]) << 16) |
                   (static_cast<std::uint32_t>(from[3]) << 24);
        }
    } // namespace

    ProgrammerV3::ProgrammerV3(CommandChannel channel, VersionReport report) noexcept
        : ProgrammerV2(std::move(channel), report)
    {
    }

    std::size_t ProgrammerV3::byte_write_limit() const noexcept
    {
        return kByteWriteLimitV3;
    }

    std::size_t ProgrammerV3::trace_buffer_size() const noexcept
    {
        return kTraceBufferV3;
    }

    Result<std::vector<std::uint32_t>> ProgrammerV3::clock_rates()
    {
        /*
         * The reply is a status word, a count at byte 8, and the rates from
         * byte 12, each a little endian word in kilohertz.
         */
        constexpr std::size_t kListReply = 52;
        constexpr std::size_t kCountAt = 8;
        constexpr std::size_t kRatesAt = 12;

        auto ask = begin(Direction::FromDevice, kListReply);
        ask.command(Command::Debug).command(DebugCommand::ApiV3GetComFrequency).u8(kSwd);

        std::uint8_t listed[kListReply] = {};

        auto asked = channel().exchange(ask.data(), ask.size(), listed, sizeof(listed),
                                        Check::Status, "asking which debug clock rates are offered");

        if (!asked.ok())
        {
            return asked.error();
        }

        std::size_t count = listed[kCountAt];

        if (count > kMaxRates)
        {
            STLINK_LOG_WRN("the programmer listed %zu clock rates, reading the first %zu", count,
                           kMaxRates);
            count = kMaxRates;
        }

        std::vector<std::uint32_t> rates;
        rates.reserve(count);

        for (std::size_t i = 0; i < count; ++i)
        {
            const std::uint32_t khz = read_u32(&listed[kRatesAt + i * 4]);

            /* A zero is padding rather than a rate of no hertz. */
            if (khz != 0)
            {
                rates.push_back(khz * 1000);
            }
        }

        return rates;
    }

    VoidResult ProgrammerV3::set_clock(std::uint32_t hz)
    {
        auto offered = clock_rates();

        if (!offered.ok())
        {
            return offered.error();
        }

        const std::vector<std::uint32_t> rates = offered.value();

        if (rates.empty())
        {
            return Error(ErrorCode::Protocol, "the programmer offered no debug clock rates");
        }

        const std::uint32_t wanted = (hz != 0) ? hz : kDefaultRateV3;

        if (std::find(rates.begin(), rates.end(), wanted) == rates.end())
        {
            return Error(ErrorCode::InvalidArgument,
                         "this programmer does not offer that debug clock rate");
        }

        constexpr std::size_t kSetReply = 8;

        /* Told in kilohertz, which is the unit it listed them in. */
        auto tell = begin(Direction::FromDevice, kSetReply);
        tell.command(Command::Debug)
            .command(DebugCommand::ApiV3SetComFrequency)
            .u8(kSwd)
            .u8(0)
            .u32(wanted / 1000);

        std::uint8_t reply[kSetReply] = {};

        auto told = channel().exchange(tell.data(), tell.size(), reply, sizeof(reply),
                                       Check::Status, "setting the debug clock rate");

        if (!told.ok())
        {
            return told.error();
        }

        return {};
    }
} // namespace stlink
