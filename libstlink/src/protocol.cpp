/**
 * @file    protocol.cpp
 * @brief   Product ids, and decoding the version reply.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/protocol.h>

namespace stlink
{
    namespace
    {
        /* The product ids ST has shipped, grouped as the firmware groups them. */

        constexpr std::uint16_t kPidV1 = 0x3744;

        constexpr std::uint16_t kPidV2 = 0x3748;
        constexpr std::uint16_t kPidV2Audio = 0x374a;
        constexpr std::uint16_t kPidV2Nucleo = 0x374b;

        constexpr std::uint16_t kPidV2_1 = 0x3752;

        constexpr std::uint16_t kPidV3UsbLoader = 0x374d;
        constexpr std::uint16_t kPidV3E = 0x374e;
        constexpr std::uint16_t kPidV3S = 0x374f;
        constexpr std::uint16_t kPidV3TwoVcp = 0x3753;
        constexpr std::uint16_t kPidV3NoMsd = 0x3754;
        constexpr std::uint16_t kPidV3P = 0x3757;

        /* What each generation can carry, in hertz. */
        constexpr std::uint32_t kMaxTraceV2 = 2000000;
        constexpr std::uint32_t kMaxTraceV3 = 24000000;

        /*
         * Firmware revisions at which a V2 gains something. The numbers are
         * the "J" revision printed by ST's own tools: J13 and J15.
         */
        constexpr std::uint8_t kV2TraceFrom = 13;
        constexpr std::uint8_t kV2LastRwStatus2From = 15;

        /*
         * An ST-LINK/V1 older than this speaks the original command set. From
         * J12 it speaks the V2 one and gains SWD along with it, which is why
         * the generation on the label does not decide the protocol.
         */
        constexpr std::uint8_t kV1SpeaksApiV2From = 12;

        constexpr std::uint16_t little_endian_16(const std::uint8_t *at) noexcept
        {
            return static_cast<std::uint16_t>(at[0] | (at[1] << 8));
        }
    } // namespace

    Generation generation_of(std::uint16_t pid) noexcept
    {
        switch (pid)
        {
        case kPidV1:
            return Generation::V1;

        case kPidV2:
        case kPidV2Audio:
        case kPidV2Nucleo:
            return Generation::V2;

        case kPidV2_1:
            return Generation::V2_1;

        case kPidV3UsbLoader:
        case kPidV3E:
        case kPidV3S:
        case kPidV3TwoVcp:
        case kPidV3NoMsd:
        case kPidV3P:
            return Generation::V3;

        default:
            return Generation::Unknown;
        }
    }

    Result<VersionReport> parse_version(Generation generation, const std::uint8_t *reply,
                                        std::size_t size)
    {
        const std::size_t expected = version_reply_size(generation);

        if (reply == nullptr || size < expected)
        {
            return Error(ErrorCode::Protocol, "the version reply was too short");
        }

        VersionReport report;

        if (generation == Generation::V3)
        {
            /*
             * Twelve bytes, a field each. The layout is not documented by ST;
             * it is the one OpenOCD established and every tool has used since.
             */
            report.version.major = reply[0];
            report.version.swim = reply[1];
            report.version.jtag = reply[2];
            report.version.vid = little_endian_16(&reply[8]);
            report.version.pid = little_endian_16(&reply[10]);

            report.api = ProtocolApi::V3;
            report.capabilities = Capability::Trace | Capability::LastRwStatus2;
            report.max_trace_frequency = kMaxTraceV3;

            return report;
        }

        /*
         * Six bytes, with the three revisions packed into the first two:
         *
         *   b0            b1            b2 b3      b4 b5
         *   4b   | 6b ------ | 6b       | 2B      | 2B
         *   major| jtag      | swim     | vendor  | product
         */
        report.version.major = static_cast<std::uint8_t>((reply[0] & 0xf0) >> 4);
        report.version.jtag =
            static_cast<std::uint8_t>(((reply[0] & 0x0f) << 2) | ((reply[1] & 0xc0) >> 6));
        report.version.swim = static_cast<std::uint8_t>(reply[1] & 0x3f);
        report.version.vid = little_endian_16(&reply[2]);
        report.version.pid = little_endian_16(&reply[4]);

        if (report.version.major == 1)
        {
            report.api = report.version.jtag >= kV1SpeaksApiV2From ? ProtocolApi::V2
                                                                  : ProtocolApi::V1;

            /* A V1 carries no trace at any revision. */
            return report;
        }

        report.api = ProtocolApi::V2;

        if (report.version.jtag >= kV2TraceFrom)
        {
            report.capabilities |= Capability::Trace;
            report.max_trace_frequency = kMaxTraceV2;
        }

        if (report.version.jtag >= kV2LastRwStatus2From)
        {
            report.capabilities |= Capability::LastRwStatus2;
        }

        return report;
    }
} // namespace stlink
