/**
 * @file    programmer_v3.h
 * @brief   A V3, which speaks the V2 command set with four exceptions.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. Everything a V3 does, it does with the V2 command set and the
 * same opcodes; what differs is how much it will carry and how it is told a
 * clock rate. The fourth difference, the version command, is settled before
 * this class exists and so lives in the factory.
 */

#ifndef STLINK_PV_PROGRAMMER_V3_H
#define STLINK_PV_PROGRAMMER_V3_H

#include <stlink/programmer_v2.h>

namespace stlink
{
    class ProgrammerV3 : public ProgrammerV2
    {
    public:
        ProgrammerV3(CommandChannel channel, VersionReport report) noexcept;

        /**
         * @brief Which rates this V3 currently offers.
         *
         * Read from the device rather than from a table: what a V3 can drive
         * depends on how it is itself clocked.
         */
        [[nodiscard]] Result<std::vector<std::uint32_t>> clock_rates() override;

        /** @brief Set the debug clock to exactly @p hz, which must be one of them. */
        [[nodiscard]] VoidResult set_clock(std::uint32_t hz) override;

    protected:
        [[nodiscard]] std::size_t byte_write_limit() const noexcept override;
        [[nodiscard]] std::size_t trace_buffer_size() const noexcept override;
    };
} // namespace stlink

#endif // STLINK_PV_PROGRAMMER_V3_H
