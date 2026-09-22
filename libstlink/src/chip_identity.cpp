/**
 * @file    chip_identity.cpp
 * @brief   Asking a connected target what it is.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The addresses here are ST's, taken from the reference manual named beside
 * each one. The core part numbers they are chosen by are ARM's and live in
 * cortex.h, because they are the same on everyone's silicon.
 */

#include <stlink/chip_identity.h>

#include <stlink/cortex.h>
#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /* Where DBGMCU_IDCODE sits, by the core that implies it. */

        /** @brief STM32H7. RM0433 page 3189. */
        constexpr std::uint32_t kIdcodeH7 = 0x5c001000;

        /**
         * @brief STM32F0, L0 and G0.
         *
         * RM0091 page 914, RM0377 page 813, RM0444 page 1367.
         */
        constexpr std::uint32_t kIdcodeCortexM0 = 0x40015800;

        /** @brief STM32L5. RM0438 page 2157. */
        constexpr std::uint32_t kIdcodeL5 = 0xe0044000;

        /** @brief STM32H5, which reports zero at the L5 address. RM0481. */
        constexpr std::uint32_t kIdcodeH5 = 0x44024000;

        /**
         * @brief Everywhere else: F1, F2, F3, F7, L1, L4, G4, WB.
         *
         * RM0008 page 1087, RM0033 page 1326, RM0316 page 1095, RM0385 page
         * 1676, RM0038 page 861, RM0351 page 1840, RM0440 page 2086, RM0434
         * page 1406.
         */
        constexpr std::uint32_t kIdcodeDefault = 0xe0042000;

        /**
         * @brief STM32WB0 and WL3x, which have no DBGMCU_IDCODE at all.
         *
         * RM0491 page 115, RM0530 page 105, RM0505 page 167, RM0511 page 176.
         * ST's own tooling takes the part number out of JTAG_ID instead, and
         * so do we.
         */
        constexpr std::uint32_t kJtagIdWb0 = 0x40000004;
        constexpr unsigned kJtagPartNumberShift = 12;

        /**
         * @brief The debug port both an H7 and an H5 answer with.
         *
         * One identifier, two very different parts, which is why the core
         * part number decides between them rather than this.
         */
        constexpr std::uint32_t kCoreIdM7M33Swd = 0x6ba02477;
        constexpr std::uint32_t kCoreIdM7M33Jtag = 0x6ba00477;

        /** @brief What an F4 revision A reports, and what it means. */
        constexpr std::uint32_t kIdReportedByF4RevisionA = 0x411;
        constexpr std::uint32_t kIdF4 = 0x413;

        /** @brief The identifier is the bottom twelve bits; the rest is revision. */
        constexpr std::uint32_t kDeviceIdMask = 0xfff;

        /** @brief Read one debug register, saying what it was for if it fails. */
        Result<std::uint32_t> read_at(IProgrammer &programmer, std::uint32_t address,
                                      const char *context)
        {
            auto read = programmer.read_debug_register(address);

            if (!read.ok())
            {
                return read.error().wrap(ErrorCode::TargetUnknown, context);
            }

            return read.value();
        }
    } // namespace

    Result<CpuId> read_cpu_id(IProgrammer &programmer)
    {
        auto read = read_at(programmer, kCpuid, "reading the core identity");

        if (!read.ok())
        {
            return read.error();
        }

        const std::uint32_t raw = read.value();

        CpuId cpu;

        cpu.implementer = static_cast<std::uint8_t>((raw >> 24) & 0x7f);
        cpu.variant = static_cast<std::uint8_t>((raw >> 20) & 0xf);
        cpu.part = static_cast<std::uint16_t>((raw >> 4) & 0xfff);
        cpu.revision = static_cast<std::uint8_t>(raw & 0xf);

        /*
         * Not a judgement about ST's choice of core. A reply that does not
         * name ARM means the debug connection is not reaching a core at all,
         * which is what a target running code that turned the debug unit off
         * looks like from here.
         */
        if (cpu.implementer != kImplementerArm)
        {
            STLINK_LOG_ERR("the target answered CPUID with %#010x, which names no ARM core", raw);

            return Error(ErrorCode::TargetUnknown,
                         "the target did not answer as an ARM core, try connecting under reset");
        }

        STLINK_LOG_DBG("core part %#05x, revision r%up%u", cpu.part, cpu.variant, cpu.revision);

        return cpu;
    }

    Result<std::uint32_t> read_chip_id(IProgrammer &programmer)
    {
        auto asked_core = programmer.core_id();

        if (!asked_core.ok())
        {
            return asked_core.error().wrap(ErrorCode::TargetUnknown, "reading the debug port identity");
        }

        const std::uint32_t core_id = asked_core.value();

        auto asked_cpu = read_cpu_id(programmer);

        if (!asked_cpu.ok())
        {
            return asked_cpu.error();
        }

        const CpuId cpu = asked_cpu.value();

        const bool h7_or_h5_port = core_id == kCoreIdM7M33Swd || core_id == kCoreIdM7M33Jtag;

        std::uint32_t raw = 0;

        if (h7_or_h5_port && cpu.part == kPartCortexM7)
        {
            auto read = read_at(programmer, kIdcodeH7, "reading the chip identity");

            if (!read.ok())
            {
                return read.error();
            }

            raw = read.value();
        }
        else if (cpu.part == kPartCortexM0 || cpu.part == kPartCortexM0Plus)
        {
            auto read = read_at(programmer, kIdcodeCortexM0, "reading the chip identity");

            if (!read.ok())
            {
                return read.error();
            }

            raw = read.value();

            /*
             * Zero here is an answer of a sort: these parts have no
             * DBGMCU_IDCODE and read as zero rather than refusing, so the
             * identity has to come out of JTAG_ID instead.
             */
            if (raw == 0)
            {
                auto jtag = read_at(programmer, kJtagIdWb0, "reading the chip identity from JTAG_ID");

                if (!jtag.ok())
                {
                    return jtag.error();
                }

                raw = jtag.value() >> kJtagPartNumberShift;
            }
        }
        else if (cpu.part == kPartCortexM33)
        {
            auto read = read_at(programmer, kIdcodeL5, "reading the chip identity");

            if (!read.ok())
            {
                return read.error();
            }

            raw = read.value();

            /* An H5 reads as zero at the L5 address and has its own. */
            if (raw == 0)
            {
                auto h5 = read_at(programmer, kIdcodeH5, "reading the chip identity");

                if (!h5.ok())
                {
                    return h5.error();
                }

                raw = h5.value();
            }
        }
        else /* CM3, CM4, and a CM7 that is not an H7 */
        {
            auto read = read_at(programmer, kIdcodeDefault, "reading the chip identity");

            if (!read.ok())
            {
                return read.error();
            }

            raw = read.value();
        }

        if (raw == 0)
        {
            return Error(ErrorCode::TargetUnknown, "the chip did not report an identity");
        }

        std::uint32_t chip_id = raw & kDeviceIdMask;

        /*
         * Errata: an F4 revision A reports the F2's identifier. The two share
         * a debug port as well, so the core is the only thing that separates
         * them, and an F2 is a CM3.
         */
        if (chip_id == kIdReportedByF4RevisionA && cpu.part == kPartCortexM4)
        {
            STLINK_LOG_DBG("reported %#05x on a Cortex-M4, reading it as %#05x per the F4 errata",
                           chip_id, kIdF4);

            chip_id = kIdF4;
        }

        STLINK_LOG_DBG("chip identity %#05x", chip_id);

        return chip_id;
    }
} // namespace stlink
