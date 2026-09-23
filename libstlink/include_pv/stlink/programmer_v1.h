/**
 * @file    programmer_v1.h
 * @brief   A programmer speaking the original ST-LINK/V1 command set.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. Only an ST-LINK/V1 with firmware older than J12 needs this; from
 * J12 the same hardware speaks the V2 set and ProgrammerV2 serves it.
 *
 * Two things set it apart from everything above it.
 *
 * Its commands travel inside a SCSI command block. A V1 presents itself as a
 * mass storage device, so each command is wrapped in a 31 byte command
 * descriptor and answered, after the reply, by a 13 byte status wrapper that
 * has to be read and discarded before the next command.
 *
 * And its replies carry no status. The firmware does not put a status byte in
 * front of an answer, so nothing here asks the channel to check one: a failure
 * shows up as a reply that does not make sense rather than as a reported
 * fault. That is what the original protocol offers, not a choice made here.
 */

#ifndef STLINK_PV_PROGRAMMER_V1_H
#define STLINK_PV_PROGRAMMER_V1_H

#include <stlink/programmer_base.h>

namespace stlink
{
    class ProgrammerV1 : public ProgrammerBase
    {
    public:
        ProgrammerV1(CommandChannel channel, VersionReport report) noexcept;

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
        /** @brief Thirty-one bytes: the SCSI descriptor and the command in it. */
        [[nodiscard]] std::size_t block_size() const noexcept override;

        /** @brief Write the SCSI command descriptor, and say where the command goes. */
        [[nodiscard]] std::size_t frame(std::uint8_t *block, Direction direction,
                                        std::size_t reply_size) override;

    private:
        /**
         * @brief Send a command, read its reply, then read the status wrapper.
         *
         * Every operation ends this way. Leaving the wrapper unread would
         * leave it to be mistaken for the next command's reply.
         */
        [[nodiscard]] Result<std::size_t> exchange(const CommandBlock &block, std::uint8_t *reply,
                                                   std::size_t reply_size, const char *what);

        /** @brief Send a command that answers with nothing, and read the wrapper. */
        [[nodiscard]] VoidResult send(const CommandBlock &block, const char *what);

        /** @brief Read and discard the 13 byte status wrapper. */
        [[nodiscard]] VoidResult discard_status_wrapper(const char *what);

        /**
         * @brief The sequence number written into each descriptor.
         *
         * The programmer does not appear to check it, but it is part of the
         * descriptor and is kept moving as the protocol expects.
         */
        std::uint32_t tag_ = 0;
    };
} // namespace stlink

#endif // STLINK_PV_PROGRAMMER_V1_H
