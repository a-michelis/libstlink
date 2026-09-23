/**
 * @file    cortex.h
 * @brief   The ARM debug registers, as every Cortex-M places them.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. These are ARM's, not ST's: the addresses and bits are fixed by the
 * ARMv7-M and ARMv8-M architecture reference manuals and are the same on every
 * part this library talks to. See section C1.6 of the ARMv7-M manual.
 *
 * They are here because halting, running and stepping a target are not
 * programmer commands at all on a V2 or a V3: the firmware exposes no opcode
 * for them, and the operation is a write to DHCSR like any other.
 */

#ifndef STLINK_PV_CORTEX_H
#define STLINK_PV_CORTEX_H

#include <cstdint>

namespace stlink
{
    /** @brief Debug halting control and status register. */
    inline constexpr std::uint32_t kDhcsr = 0xe000edf0;

    /** @brief Debug core register selector, for reading a core register. */
    inline constexpr std::uint32_t kDcrsr = 0xe000edf4;

    /** @brief Debug core register data, where the selected one appears. */
    inline constexpr std::uint32_t kDcrdr = 0xe000edf8;

    /**
     * @brief The password DHCSR demands.
     *
     * A write to DHCSR without this in its top half is ignored, which is why
     * every write below carries it.
     */
    inline constexpr std::uint32_t kDhcsrKey = 0xa05f << 16;

    /* Written to DHCSR. */
    inline constexpr std::uint32_t kDhcsrDebugEnable = 1u << 0;
    inline constexpr std::uint32_t kDhcsrHalt = 1u << 1;
    inline constexpr std::uint32_t kDhcsrStep = 1u << 2;

    /** @brief Hold interrupts off, so a step or a loader runs undisturbed. */
    inline constexpr std::uint32_t kDhcsrMaskInterrupts = 1u << 3;

    /* Read from DHCSR. */
    inline constexpr std::uint32_t kDhcsrRegisterReady = 1u << 16;
    inline constexpr std::uint32_t kDhcsrHalted = 1u << 17;
    inline constexpr std::uint32_t kDhcsrSleeping = 1u << 18;
    inline constexpr std::uint32_t kDhcsrLockedUp = 1u << 19;
    inline constexpr std::uint32_t kDhcsrInstructionRetired = 1u << 24;
    inline constexpr std::uint32_t kDhcsrResetSince = 1u << 25;

    /**
     * @brief Application interrupt and reset control register.
     *
     * Section B3.2.6 of the ARMv7-M manual. How the core is reset when the
     * board's nRST pin is not wired, which is common enough to matter.
     */
    inline constexpr std::uint32_t kAircr = 0xe000ed0c;

    /** @brief The password AIRCR demands; a write without it is ignored. */
    inline constexpr std::uint32_t kAircrKey = 0x05fa << 16;

    /** @brief Ask the core to reset itself. */
    inline constexpr std::uint32_t kAircrSystemReset = 1u << 2;

    /**
     * @brief Debug exception and monitor control register.
     *
     * Section C1.6.5. This lives in the debug power domain, so what is armed
     * here survives a reset of the core, which is the whole trick behind
     * catching a target at its reset vector rather than after it has run.
     */
    inline constexpr std::uint32_t kDemcr = 0xe000edfc;

    /** @brief Halt the core as it comes out of reset, before it executes. */
    inline constexpr std::uint32_t kDemcrResetVectorCatch = 1u << 0;

    /** @brief Enable the trace and debug blocks. */
    inline constexpr std::uint32_t kDemcrTraceEnable = 1u << 24;

    /**
     * @brief CPUID base register, which says which core this is.
     *
     * Section B3.2.3 of the ARMv7-M manual. Reading it is how the library
     * finds out what it is talking to before it knows anything about the
     * chip around it, which matters because where a chip keeps its own
     * identity depends on which core ST put in it.
     */
    inline constexpr std::uint32_t kCpuid = 0xe000ed00;

    /** @brief The implementer CPUID reports for a core ARM designed. */
    inline constexpr std::uint8_t kImplementerArm = 0x41;

    /* The part numbers CPUID reports. ARM's, and the same on every vendor's
     * silicon, which is what makes them safe to dispatch on. */
    inline constexpr std::uint16_t kPartCortexM0 = 0xc20;
    inline constexpr std::uint16_t kPartCortexM0Plus = 0xc60;
    inline constexpr std::uint16_t kPartCortexM3 = 0xc23;
    inline constexpr std::uint16_t kPartCortexM4 = 0xc24;
    inline constexpr std::uint16_t kPartCortexM7 = 0xc27;
    inline constexpr std::uint16_t kPartCortexM33 = 0xd21;
} // namespace stlink

#endif // STLINK_PV_CORTEX_H
