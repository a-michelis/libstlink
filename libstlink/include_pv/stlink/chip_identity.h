/**
 * @file    chip_identity.h
 * @brief   Asking a connected target what it is.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. This is the step between having a programmer and having a device:
 * the chip has to name itself before the library can look it up, choose a
 * flash implementation, or report anything about it.
 *
 * There is no single register to read. ST puts the identity in DBGMCU_IDCODE,
 * but not at the same address on every part, and nothing on the chip says
 * which address it used. What does correlate is the core: a CM0 part keeps it
 * somewhere a CM4 part does not. So the core is identified first, from ARM's
 * own CPUID register, and the address follows from that.
 *
 * Two of those addresses answer zero rather than failing, on parts that have
 * no DBGMCU_IDCODE at all, so each has a second address behind it. A read that
 * succeeds and returns zero is therefore not an answer, and is treated as one
 * more reason to keep looking.
 */

#ifndef STLINK_PV_CHIP_IDENTITY_H
#define STLINK_PV_CHIP_IDENTITY_H

#include <cstdint>

#include <stlink/programmer.h>
#include <stlink/result.h>

namespace stlink
{
    /**
     * @brief What the CPUID register says about the core.
     *
     * Only @ref part is dispatched on; the rest is carried because it costs
     * nothing and a log line naming the revision is worth having when a chip
     * turns out to behave unlike its family.
     */
    struct CpuId
    {
        std::uint8_t implementer = 0; /**< 0x41 for a core ARM designed. */
        std::uint8_t variant = 0;
        std::uint16_t part = 0; /**< kPartCortexM4 and friends, from cortex.h. */
        std::uint8_t revision = 0;
    };

    /**
     * @brief Read and decode the CPUID register.
     *
     * Fails with TargetUnknown when the implementer is not ARM, which in
     * practice means the debug connection is not reaching a core at all
     * rather than that ST shipped something exotic. The usual cause is a
     * target that was already running code which disabled the debug unit,
     * and the usual cure is connecting under reset.
     */
    [[nodiscard]] Result<CpuId> read_cpu_id(IProgrammer &programmer);

    /**
     * @brief Read which chip is on the other side of @p programmer.
     *
     * The programmer must already be in debug mode and talking to a target.
     *
     * @return the twelve bit device identifier, never zero on success
     */
    [[nodiscard]] Result<std::uint32_t> read_chip_id(IProgrammer &programmer);
} // namespace stlink

#endif // STLINK_PV_CHIP_IDENTITY_H
