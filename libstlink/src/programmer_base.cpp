/**
 * @file    programmer_base.cpp
 * @brief   What every programmer has, whichever command set it speaks.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/programmer_base.h>

#include <cassert>
#include <cstring>

namespace stlink
{
    namespace
    {
        /** @brief What a V2 and a V3 send. A V1 wraps this in fifteen more. */
        constexpr std::size_t kPlainBlockSize = 16;
    } // namespace

    /* ---- CommandBlock ------------------------------------------------- */

    CommandBlock::CommandBlock(std::uint8_t *into, std::size_t capacity, std::size_t at) noexcept
        : into_(into), capacity_(capacity), at_(at)
    {
        assert(into != nullptr);
        assert(at <= capacity);
    }

    CommandBlock &CommandBlock::u8(std::uint8_t value) noexcept
    {
        assert(at_ < capacity_);

        into_[at_++] = value;

        return *this;
    }

    CommandBlock &CommandBlock::u16(std::uint16_t value) noexcept
    {
        u8(static_cast<std::uint8_t>(value & 0xff));
        u8(static_cast<std::uint8_t>(value >> 8));

        return *this;
    }

    CommandBlock &CommandBlock::u32(std::uint32_t value) noexcept
    {
        u16(static_cast<std::uint16_t>(value & 0xffff));
        u16(static_cast<std::uint16_t>(value >> 16));

        return *this;
    }

    CommandBlock &CommandBlock::command(Command value) noexcept
    {
        return u8(static_cast<std::uint8_t>(value));
    }

    CommandBlock &CommandBlock::command(DebugCommand value) noexcept
    {
        return u8(static_cast<std::uint8_t>(value));
    }

    CommandBlock &CommandBlock::command(DfuCommand value) noexcept
    {
        return u8(static_cast<std::uint8_t>(value));
    }

    const std::uint8_t *CommandBlock::data() const noexcept
    {
        return into_;
    }

    std::size_t CommandBlock::size() const noexcept
    {
        return capacity_;
    }

    /* ---- ProgrammerBase ----------------------------------------------- */

    ProgrammerBase::ProgrammerBase(CommandChannel channel, VersionReport report) noexcept
        : channel_(std::move(channel)), report_(report)
    {
    }

    ProgrammerBase::~ProgrammerBase() = default;

    const ProgrammerVersion &ProgrammerBase::version() const noexcept
    {
        return report_.version;
    }

    const ProbeAddress &ProgrammerBase::address() const noexcept
    {
        return channel_.transport().address();
    }

    std::uint32_t ProgrammerBase::max_trace_frequency() const noexcept
    {
        return report_.max_trace_frequency;
    }

    CommandChannel &ProgrammerBase::channel() noexcept
    {
        return channel_;
    }

    Capability ProgrammerBase::capabilities() const noexcept
    {
        return report_.capabilities;
    }

    VoidResult ProgrammerBase::require(Capability capability, const char *what) const
    {
        if (has_capability(report_.capabilities, capability))
        {
            return {};
        }

        return Error(ErrorCode::NotSupported, what);
    }

    std::uint8_t ProgrammerBase::access_port() const noexcept
    {
        return access_port_;
    }

    void ProgrammerBase::set_access_port(std::uint8_t ap) noexcept
    {
        access_port_ = ap;
    }

    std::size_t ProgrammerBase::block_size() const noexcept
    {
        return kPlainBlockSize;
    }

    std::size_t ProgrammerBase::frame(std::uint8_t *, Direction, std::size_t)
    {
        /* Nothing precedes the command, so it starts at the front. */
        return 0;
    }

    CommandBlock ProgrammerBase::begin(Direction direction, std::size_t reply_size)
    {
        const std::size_t size = block_size();

        assert(size <= sizeof(block_));

        std::memset(block_, 0, size);

        const std::size_t at = frame(block_, direction, reply_size);

        return CommandBlock(block_, size, at);
    }
} // namespace stlink
