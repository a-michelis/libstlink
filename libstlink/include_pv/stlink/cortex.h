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
} // namespace stlink

#endif // STLINK_PV_CORTEX_H
