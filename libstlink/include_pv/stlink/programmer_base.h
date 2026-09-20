/**
 * @file    programmer_base.h
 * @brief   What every programmer has, whichever command set it speaks.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. The plumbing only: the channel, what the version exchange
 * established, and the assembling of a command block. No opcode appears here,
 * because which opcode means what is exactly what the generations disagree
 * about.
 *
 * A command block is a fixed size buffer, zeroed, with the command written
 * into its front. How large it is and what precedes the command is framing,
 * and framing is the one thing a V1 does differently: it wraps the block in a
 * SCSI command descriptor. Both are virtual so that the derived classes write
 * the same command into whichever envelope their firmware expects.
 */

#ifndef STLINK_PV_PROGRAMMER_BASE_H
#define STLINK_PV_PROGRAMMER_BASE_H

#include <cstddef>
#include <cstdint>
#include <memory>

#include <stlink/command.h>
#include <stlink/programmer.h>
#include <stlink/protocol.h>

namespace stlink
{
    /** @brief Which way the payload of a command travels. */
    enum class Direction : std::uint8_t
    {
        ToDevice,
        FromDevice,
    };

    /**
     * @brief A command block being written.
     *
     * Holds no buffer of its own: it writes into the programmer's, which
     * lives as long as the programmer does. Writing past the end is a broken
     * invariant rather than a runtime condition, so it is caught by an
     * assertion and never silently truncated.
     */
    class CommandBlock
    {
    public:
        CommandBlock(std::uint8_t *into, std::size_t capacity, std::size_t at) noexcept;

        /** @brief Append one byte. */
        CommandBlock &u8(std::uint8_t value) noexcept;

        /** @brief Append two bytes, least significant first. */
        CommandBlock &u16(std::uint16_t value) noexcept;

        /** @brief Append four bytes, least significant first. */
        CommandBlock &u32(std::uint32_t value) noexcept;

        /** @brief Append a command byte. */
        CommandBlock &command(Command value) noexcept;

        /** @brief Append a debug command byte. */
        CommandBlock &command(DebugCommand value) noexcept;

        /** @brief Append a DFU command byte. */
        CommandBlock &command(DfuCommand value) noexcept;

        [[nodiscard]] const std::uint8_t *data() const noexcept;

        /**
         * @brief The whole block, not what has been written into it.
         *
         * A command block is always sent at its full length, with whatever
         * was not written left zero. The firmware reads a fixed number of
         * bytes and a short one is not understood.
         */
        [[nodiscard]] std::size_t size() const noexcept;

    private:
        std::uint8_t *into_;
        std::size_t capacity_;
        std::size_t at_;
    };

    /**
     * @brief The parts of a programmer that do not depend on its command set.
     *
     * Not instantiable: it implements the accessors that are answered from
     * what the version exchange already established, and leaves every
     * operation to the class that knows the opcodes.
     */
    class ProgrammerBase : public IProgrammer
    {
    public:
        ~ProgrammerBase() override;

        [[nodiscard]] const ProgrammerVersion &version() const noexcept override;
        [[nodiscard]] const ProbeAddress &address() const noexcept override;
        [[nodiscard]] std::uint32_t max_trace_frequency() const noexcept override;

    protected:
        ProgrammerBase(CommandChannel channel, VersionReport report) noexcept;

        [[nodiscard]] CommandChannel &channel() noexcept;

        /** @brief What the firmware in front of us is able to do. */
        [[nodiscard]] Capability capabilities() const noexcept;

        /**
         * @brief Refuse an operation this firmware does not have.
         *
         * @p what names the operation, so the caller is told which one and
         * not merely that something was unsupported.
         */
        [[nodiscard]] VoidResult require(Capability capability, const char *what) const;

        /** @brief Which access port memory operations go through. */
        [[nodiscard]] std::uint8_t access_port() const noexcept;
        void set_access_port(std::uint8_t ap) noexcept;

        /**
         * @brief Start a command block.
         *
         * @param direction  which way its payload travels
         * @param reply_size how much is expected back
         *
         * Both are framing: a V2 and a V3 ignore them, and a V1 writes them
         * into the SCSI descriptor it wraps the command in.
         */
        [[nodiscard]] CommandBlock begin(Direction direction, std::size_t reply_size);

        /** @brief How long a whole command block is. Sixteen bytes, unless framed. */
        [[nodiscard]] virtual std::size_t block_size() const noexcept;

        /**
         * @brief Write whatever precedes the command.
         *
         * @return where the command itself starts
         */
        [[nodiscard]] virtual std::size_t frame(std::uint8_t *block, Direction direction,
                                                std::size_t reply_size);

    private:
        CommandChannel channel_;
        VersionReport report_;
        std::uint8_t access_port_ = 0;

        /** @brief Where a command block is assembled. Reused, never reallocated. */
        std::uint8_t block_[32] = {};
    };
} // namespace stlink

#endif // STLINK_PV_PROGRAMMER_BASE_H
