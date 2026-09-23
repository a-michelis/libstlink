/**
 * @file    programmer.h
 * @brief   The ST-LINK itself, and the protocol it speaks.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. A programmer turns operations into the command blocks its
 * firmware expects and sends them over a transport it owns. What the target
 * chip is, and what its flash needs, belongs a layer up.
 *
 * The versions differ here and only here: a V1 frames its commands as SCSI
 * command blocks, a V2 sends them directly, a V3 adds its own. All three are
 * reached over the same transport.
 *
 * Constructed connected, destroyed disconnected, like the transport it holds.
 */

#ifndef STLINK_PV_PROGRAMMER_H
#define STLINK_PV_PROGRAMMER_H

#include <cstddef>
#include <cstdint>
#include <vector>

/*
 * DebugMode, ResetMode and CoreRegisters live in the public header rather than
 * here, because a caller choosing how to connect needs to name them. They are
 * the same types either way; this header does not redeclare them.
 */
#include <stlink/device.h>
#include <stlink/result.h>
#include <stlink/transport.h>

namespace stlink
{
    /** @brief What the programmer is currently doing. */
    enum class ProgrammerMode : std::uint8_t
    {
        Unknown = 0,
        Dfu,        /**< Firmware update. */
        MassStorage,/**< Pretending to be a disk. */
        Debug,      /**< Talking to a target. */
    };

    /** @brief What the target is doing. */
    enum class CoreState : std::uint8_t
    {
        Unknown = 0,
        Running,      /**< Executing, with no debug unit attached. */
        Halted,       /**< Stopped by the debug unit. */
        Reset,        /**< Held in reset. */
        DebugRunning, /**< Executing, with the debug unit attached. */
    };

    /** @brief What the programmer reports about itself. */
    struct ProgrammerVersion
    {
        std::uint8_t major = 0;   /**< 1, 2 or 3. */
        std::uint8_t jtag = 0;    /**< Firmware revision of the debug engine. */
        std::uint8_t swim = 0;    /**< Firmware revision of the SWIM engine, 0 when absent. */
        std::uint16_t vid = 0;
        std::uint16_t pid = 0;
    };

    /**
     * @brief One ST-LINK, and the debug protocol it speaks.
     *
     * One instance belongs to one thread at a time; nothing here is
     * internally synchronised.
     */
    class IProgrammer
    {
    public:
        virtual ~IProgrammer() = default;

        IProgrammer(const IProgrammer &) = delete;
        IProgrammer &operator=(const IProgrammer &) = delete;

        /* ---- The programmer itself ---------------------------------- */

        [[nodiscard]] virtual const ProgrammerVersion &version() const noexcept = 0;

        /** @brief Where this programmer is attached. */
        [[nodiscard]] virtual const ProbeAddress &address() const noexcept = 0;

        /** @brief What it is currently doing. */
        [[nodiscard]] virtual Result<ProgrammerMode> mode() = 0;

        /** @brief Voltage on the target's reference pin, in millivolts. */
        [[nodiscard]] virtual Result<std::uint32_t> target_voltage() = 0;

        /**
         * @brief The debug clock rates this programmer will accept, in hertz.
         *
         * A V2 offers a table fixed in its firmware; a V3 is asked, because
         * what it offers depends on how it is itself clocked. Empty when the
         * programmer cannot be told a rate at all.
         */
        [[nodiscard]] virtual Result<std::vector<std::uint32_t>> clock_rates() = 0;

        /**
         * @brief Set the debug clock to exactly @p hz.
         *
         * One of the rates clock_rates() reported, and nothing else: a rate
         * that is not on offer is refused rather than rounded. Rounding down
         * would quietly turn a request into a ceiling, and rounding up would
         * overclock a connection that may not carry it.
         *
         * Zero asks for this generation's default rate, for a caller with no
         * opinion. It is a conservative rate rather than the fastest offered,
         * because the fastest is the one a marginal connection fails at.
         */
        [[nodiscard]] virtual VoidResult set_clock(std::uint32_t hz) = 0;

        /**
         * @brief The fastest trace the programmer will carry, in hertz.
         *
         * Known from the connection, so trace_enable() can refuse a rate this
         * probe cannot sustain rather than losing the output silently. Zero
         * when the programmer does not carry trace at all.
         */
        [[nodiscard]] virtual std::uint32_t max_trace_frequency() const noexcept = 0;

        /* ---- Getting to a target ------------------------------------ */

        /** @brief Leave firmware update mode, if that is where it is. */
        [[nodiscard]] virtual VoidResult exit_dfu() = 0;

        /**
         * @brief Start talking to the target.
         *
         * Only that. Holding the target in reset, catching it on the way out
         * and resetting it afterwards are a sequence rather than a command,
         * and belong to connect_to_target() a layer up.
         */
        [[nodiscard]] virtual VoidResult enter_debug(DebugMode debug) = 0;

        /** @brief Stop talking to it and let it run on its own. */
        [[nodiscard]] virtual VoidResult exit_debug() = 0;

        /** @brief Choose the access port to work through. */
        [[nodiscard]] virtual VoidResult select_access_port(std::uint8_t ap) = 0;

        /** @brief The target's CPU identifier. */
        [[nodiscard]] virtual Result<std::uint32_t> core_id() = 0;

        /* ---- Controlling the target --------------------------------- */

        [[nodiscard]] virtual Result<CoreState> state() = 0;
        [[nodiscard]] virtual VoidResult halt() = 0;
        /**
         * @brief Let the target run.
         *
         * @p flash_loader starts the loader that was written to SRAM, which
         * the programmer starts differently from ordinary code. The default
         * belongs to this declaration alone: a default argument on a virtual
         * is bound statically, so an implementation must not restate it.
         */
        [[nodiscard]] virtual VoidResult run(bool flash_loader = false) = 0;
        [[nodiscard]] virtual VoidResult step() = 0;

        /** @brief Reset the target through the debug unit. */
        [[nodiscard]] virtual VoidResult reset() = 0;

        /** @brief Drive the nRST pin directly. */
        [[nodiscard]] virtual VoidResult reset_pin(bool asserted) = 0;

        /* ---- Target memory ------------------------------------------ */

        /**
         * @brief Read target memory.
         *
         * The access width is chosen from @p address and @p size; a caller
         * that needs a particular width should align its request to it.
         */
        [[nodiscard]] virtual VoidResult read_memory(std::uint32_t address,
                                                     std::uint8_t *data, std::size_t size) = 0;

        [[nodiscard]] virtual VoidResult write_memory(std::uint32_t address,
                                                      const std::uint8_t *data, std::size_t size) = 0;

        /* ---- Debug unit registers ----------------------------------- */

        /**
         * @brief Read a debug unit register.
         *
         * Not target memory: these reach the debug port itself, and are how
         * the core is halted, stepped and inspected.
         */
        [[nodiscard]] virtual Result<std::uint32_t> read_debug_register(std::uint32_t address) = 0;

        [[nodiscard]] virtual VoidResult write_debug_register(std::uint32_t address,
                                                              std::uint32_t value) = 0;

        /* ---- Core registers ----------------------------------------- */

        [[nodiscard]] virtual Result<std::uint32_t> read_register(std::uint8_t index) = 0;
        [[nodiscard]] virtual VoidResult write_register(std::uint8_t index, std::uint32_t value) = 0;
        [[nodiscard]] virtual Result<CoreRegisters> read_registers() = 0;

        /* ---- Trace -------------------------------------------------- */

        [[nodiscard]] virtual VoidResult trace_enable(std::uint32_t hz) = 0;
        [[nodiscard]] virtual VoidResult trace_disable() = 0;

        /** @brief Read whatever trace output is waiting. */
        [[nodiscard]] virtual Result<std::size_t> trace_read(std::uint8_t *data, std::size_t size) = 0;

    protected:
        IProgrammer() = default;
    };

    /**
     * @brief Connect to a programmer and find out what it is.
     *
     * Opens a transport to @p probe, asks the version, and returns whichever
     * implementation speaks the command set it answered with. Connected when
     * it is returned and disconnected when it is destroyed.
     *
     * The product id decides how the version is asked for, since a V3 uses a
     * different command for it; the reply decides everything after that.
     */
    [[nodiscard]] Result<std::unique_ptr<IProgrammer>> open_programmer(const ProbeAddress &probe);
} // namespace stlink

#endif // STLINK_PV_PROGRAMMER_H
