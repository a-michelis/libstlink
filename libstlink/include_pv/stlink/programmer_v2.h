/**
 * @file    programmer_v2.h
 * @brief   A programmer speaking the V2 command set.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. This is the command set nearly every programmer in use speaks: an
 * ST-LINK/V2, a V2-1, an ST-LINK/V1 with firmware J12 or later, and a V3 but
 * for four places where its firmware wants something else.
 *
 * Halting, running and stepping are not commands here. The V2 firmware
 * exposes no opcode for them, and each is a write to the target's DHCSR
 * through write_debug_register(). Only the original V1 command set has them.
 */

#ifndef STLINK_PV_PROGRAMMER_V2_H
#define STLINK_PV_PROGRAMMER_V2_H

#include <stlink/programmer_base.h>

namespace stlink
{
    class ProgrammerV2 : public ProgrammerBase
    {
    public:
        ProgrammerV2(CommandChannel channel, VersionReport report) noexcept;

        /* ---- The programmer itself ---------------------------------- */

        [[nodiscard]] Result<ProgrammerMode> mode() override;
        [[nodiscard]] Result<std::uint32_t> target_voltage() override;
        [[nodiscard]] Result<std::vector<std::uint32_t>> clock_rates() override;
        [[nodiscard]] VoidResult set_clock(std::uint32_t hz) override;

        /* ---- Getting to a target ------------------------------------ */

        [[nodiscard]] VoidResult exit_dfu() override;
        [[nodiscard]] VoidResult enter_debug(DebugMode debug) override;
        [[nodiscard]] VoidResult exit_debug() override;
        [[nodiscard]] VoidResult select_access_port(std::uint8_t ap) override;
        [[nodiscard]] Result<std::uint32_t> core_id() override;

        /* ---- Controlling the target --------------------------------- */

        [[nodiscard]] Result<CoreState> state() override;
        [[nodiscard]] VoidResult halt() override;
        [[nodiscard]] VoidResult run(bool flash_loader) override;
        [[nodiscard]] VoidResult step() override;
        [[nodiscard]] VoidResult reset() override;
        [[nodiscard]] VoidResult reset_pin(bool asserted) override;

        /* ---- Target memory ------------------------------------------ */

        [[nodiscard]] VoidResult read_memory(std::uint32_t address, std::uint8_t *data,
                                             std::size_t size) override;
        [[nodiscard]] VoidResult write_memory(std::uint32_t address, const std::uint8_t *data,
                                              std::size_t size) override;

        /* ---- Debug unit registers ----------------------------------- */

        [[nodiscard]] Result<std::uint32_t> read_debug_register(std::uint32_t address) override;
        [[nodiscard]] VoidResult write_debug_register(std::uint32_t address,
                                                      std::uint32_t value) override;

        /* ---- Core registers ----------------------------------------- */

        [[nodiscard]] Result<std::uint32_t> read_register(std::uint8_t index) override;
        [[nodiscard]] VoidResult write_register(std::uint8_t index, std::uint32_t value) override;
        [[nodiscard]] Result<CoreRegisters> read_registers() override;

        /* ---- Trace -------------------------------------------------- */

        [[nodiscard]] VoidResult trace_enable(std::uint32_t hz) override;
        [[nodiscard]] VoidResult trace_disable() override;
        [[nodiscard]] Result<std::size_t> trace_read(std::uint8_t *data,
                                                     std::size_t size) override;

    protected:
        /*
         * The four places a V3 wants something else. Everything above is
         * written once and inherited; these are the seams it reaches through.
         */

        /** @brief The most bytes one unaligned write may carry. */
        [[nodiscard]] virtual std::size_t byte_write_limit() const noexcept;

        /** @brief How much trace the programmer will buffer for us. */
        [[nodiscard]] virtual std::size_t trace_buffer_size() const noexcept;

        /**
         * @brief Confirm the last memory write landed.
         *
         * A 32-bit write answers with nothing, so the firmware is asked
         * afterwards whether it went through.
         */
        [[nodiscard]] VoidResult check_last_write();

        /** @brief Write a value to the target's DHCSR, key included. */
        [[nodiscard]] VoidResult write_dhcsr(std::uint32_t bits);
    };
} // namespace stlink

#endif // STLINK_PV_PROGRAMMER_V2_H
