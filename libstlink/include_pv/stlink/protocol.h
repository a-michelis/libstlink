/**
 * @file    protocol.h
 * @brief   What the ST-LINK protocol is made of.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. The opcodes, the product ids, and the one exchange that can be
 * decoded without a connection: the version reply. Everything here is data
 * about the protocol rather than a way of speaking it, so it is free of the
 * transport and testable on its own.
 *
 * The numbers are the firmware's, not ours. They are reproduced as written in
 * ST's own tooling and must not be renumbered to suit us.
 */

#ifndef STLINK_PV_PROTOCOL_H
#define STLINK_PV_PROTOCOL_H

#include <cstddef>
#include <cstdint>

#include <stlink/programmer.h>
#include <stlink/result.h>

namespace stlink
{
    /** @brief ST's USB vendor id. Every programmer answers to this one. */
    inline constexpr std::uint16_t kVendorId = 0x0483;

    /** @brief The first byte of every command block. */
    enum class Command : std::uint8_t
    {
        GetVersion = 0xf1,       /**< Reply is six bytes, packed. */
        Debug = 0xf2,            /**< A DebugCommand follows. */
        Dfu = 0xf3,              /**< A DfuCommand follows. */
        GetCurrentMode = 0xf5,   /**< Debug, DFU or mass storage. */
        GetTargetVoltage = 0xf7, /**< Reply is a factor and a reading. */
        GetVersionApiV3 = 0xfb,  /**< V3 only. Reply is twelve bytes, plain. */
    };

    /**
     * @brief The second byte, after Command::Debug.
     *
     * ApiV1 entries are the original ST-LINK/V1 command set; ApiV2 replaced
     * most of them and ApiV3 adds two of its own. Where both exist for the
     * same operation the firmware understands only the one matching its
     * generation, which is the whole reason the programmer classes differ.
     */
    enum class DebugCommand : std::uint8_t
    {
        EnterJtagReset = 0x00,
        GetStatus = 0x01,
        ForceDebug = 0x02,

        ApiV1ResetSys = 0x03,
        ApiV1ReadAllRegs = 0x04,
        ApiV1ReadReg = 0x05,
        ApiV1WriteReg = 0x06,

        ReadMemory32Bit = 0x07,
        WriteMemory32Bit = 0x08,
        RunCore = 0x09,
        StepCore = 0x0a,

        ApiV1SetFp = 0x0b,

        WriteMemory8Bit = 0x0d,

        ApiV1ClearFp = 0x0e,
        ApiV1WriteDebugReg = 0x0f,
        ApiV1Enter = 0x20,

        Exit = 0x21,
        ReadCoreId = 0x22,

        ApiV2Enter = 0x30,
        ApiV2ReadIdCodes = 0x31,
        ApiV2ResetSys = 0x32,
        ApiV2ReadReg = 0x33,
        ApiV2WriteReg = 0x34,
        ApiV2WriteDebugReg = 0x35,
        ApiV2ReadDebugReg = 0x36,

        ApiV2ReadAllRegs = 0x3a,
        ApiV2GetLastRwStatus = 0x3b,
        ApiV2DriveNrst = 0x3c,
        ApiV2GetLastRwStatus2 = 0x3e,
        ApiV2StartTraceRx = 0x40,
        ApiV2StopTraceRx = 0x41,
        ApiV2GetTraceCount = 0x42,
        ApiV2SwdSetFrequency = 0x43,
        ApiV2InitAccessPort = 0x4b,

        ApiV3SetComFrequency = 0x61,
        ApiV3GetComFrequency = 0x62,

        EnterSwd = 0xa3,
        EnterJtagNoReset = 0xa4,
    };

    /** @brief The second byte, after Command::Dfu. */
    enum class DfuCommand : std::uint8_t
    {
        Exit = 0x07,
    };

    /**
     * @brief What a particular firmware revision is able to do.
     *
     * Capabilities follow the revision, not the generation: a V2 gains trace
     * at J13 and a better status command at J15. They stay inside the
     * implementation, which reports Unsupported when asked for what the
     * firmware in front of it cannot do.
     */
    enum class Capability : std::uint32_t
    {
        None = 0,

        /** @brief Can deliver SWO output. A V2 gains this at revision J13. */
        Trace = 1u << 0,

        /**
         * @brief Has the read/write status command that reports a fault address.
         *
         * A V2 gains it at revision J15 and a V3 always has it. The older
         * command reports only that something went wrong.
         */
        LastRwStatus2 = 1u << 1,
    };

    constexpr Capability operator|(Capability a, Capability b) noexcept
    {
        return static_cast<Capability>(static_cast<std::uint32_t>(a) |
                                       static_cast<std::uint32_t>(b));
    }

    constexpr Capability &operator|=(Capability &a, Capability b) noexcept
    {
        a = a | b;

        return a;
    }

    constexpr bool has_capability(Capability set, Capability one) noexcept
    {
        return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(one)) != 0;
    }

    /**
     * @brief Which command set a programmer speaks.
     *
     * Not the same as the generation on the label. An ST-LINK/V1 with
     * firmware J12 or later speaks the V2 command set, so this is what
     * decides which implementation is built, and the generation is not.
     */
    enum class ProtocolApi : std::uint8_t
    {
        V1,
        V2,
        V3,
    };

    /**
     * @brief Which generation a programmer is, by its product id.
     *
     * Known before anything is sent, and needed before anything can be: the
     * version command itself differs between V3 and the rest.
     */
    enum class Generation : std::uint8_t
    {
        Unknown = 0,
        V1,
        V2,
        V2_1,
        V3,
    };

    /** @brief What generation a product id belongs to, or Unknown. */
    [[nodiscard]] Generation generation_of(std::uint16_t pid) noexcept;

    /** @brief Whether this library has anything to say to a device. */
    [[nodiscard]] inline bool is_programmer(std::uint16_t vid, std::uint16_t pid) noexcept
    {
        return vid == kVendorId && generation_of(pid) != Generation::Unknown;
    }

    /** @brief How long a version reply is, for the generation asking. */
    [[nodiscard]] constexpr std::size_t version_reply_size(Generation generation) noexcept
    {
        return generation == Generation::V3 ? 12u : 6u;
    }

    /** @brief Everything the version exchange establishes. */
    struct VersionReport
    {
        ProgrammerVersion version;

        /** @brief Which command set to speak, once the revision is known. */
        ProtocolApi api = ProtocolApi::V2;

        Capability capabilities = Capability::None;

        /** @brief The fastest trace this programmer carries, or zero. */
        std::uint32_t max_trace_frequency = 0;
    };

    /**
     * @brief Decode a version reply.
     *
     * @p generation decides how, because the two layouts share no field
     * positions: before V3 the reply is six bytes with the revisions packed
     * into the first two, and on a V3 it is twelve with a byte each.
     */
    [[nodiscard]] Result<VersionReport> parse_version(Generation generation,
                                                      const std::uint8_t *reply, std::size_t size);
} // namespace stlink

#endif // STLINK_PV_PROTOCOL_H
