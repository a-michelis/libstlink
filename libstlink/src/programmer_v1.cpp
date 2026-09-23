/**
 * @file    programmer_v1.cpp
 * @brief   A programmer speaking the original ST-LINK/V1 command set.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/programmer_v1.h>

#include <cstring>

#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /** @brief The whole block: a SCSI command descriptor with a command in it. */
        constexpr std::size_t kBlockSize = 31;

        /** @brief Where the command starts, once the descriptor is written. */
        constexpr std::size_t kCommandAt = 15;

        /** @brief What the firmware is told every command's length is. */
        constexpr std::uint8_t kScsiCommandLength = 0x0a;

        /** @brief The signature every command descriptor begins with. */
        constexpr std::uint8_t kSignature[4] = {'U', 'S', 'B', 'C'};

        /** @brief Marks a descriptor whose payload travels from the device. */
        constexpr std::uint8_t kFromDevice = 0x80;

        /** @brief The status wrapper that follows each operation. */
        constexpr std::size_t kStatusWrapperSize = 13;

        /* A V1 answers a register read with the value alone, at the front. */
        constexpr std::size_t kValueOffset = 0;

        /** @brief The most a memory transfer may carry in one command. */
        constexpr std::size_t kMemoryTransferLimit = 6144;

        /** @brief The most an unaligned write may carry. */
        constexpr std::size_t kByteWriteLimit = 64;

        /* What the firmware calls the modes it can be in. */
        constexpr std::uint8_t kModeDfu = 0;
        constexpr std::uint8_t kModeMassStorage = 1;
        constexpr std::uint8_t kModeDebug = 2;

        /* What GETSTATUS answers with. */
        constexpr std::uint8_t kCoreRunning = 0x80;
        constexpr std::uint8_t kCoreHalted = 0x81;

        std::uint32_t read_u32(const std::uint8_t *from) noexcept
        {
            return static_cast<std::uint32_t>(from[0]) |
                   (static_cast<std::uint32_t>(from[1]) << 8) |
                   (static_cast<std::uint32_t>(from[2]) << 16) |
                   (static_cast<std::uint32_t>(from[3]) << 24);
        }

        void write_u32(std::uint8_t *into, std::uint32_t value) noexcept
        {
            into[0] = static_cast<std::uint8_t>(value & 0xff);
            into[1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
            into[2] = static_cast<std::uint8_t>((value >> 16) & 0xff);
            into[3] = static_cast<std::uint8_t>((value >> 24) & 0xff);
        }
    } // namespace

    ProgrammerV1::ProgrammerV1(CommandChannel channel, VersionReport report) noexcept
        : ProgrammerBase(std::move(channel), report)
    {
    }

    /* ---- Framing, which is the whole difference on the wire ------------ */

    std::size_t ProgrammerV1::block_size() const noexcept
    {
        return kBlockSize;
    }

    std::size_t ProgrammerV1::frame(std::uint8_t *block, Direction direction,
                                    std::size_t reply_size)
    {
        std::memcpy(block, kSignature, sizeof(kSignature));

        write_u32(&block[4], tag_);
        write_u32(&block[8], static_cast<std::uint32_t>(reply_size));

        block[12] = (direction == Direction::FromDevice) ? kFromDevice : 0;
        block[13] = 0; /* Logical unit. There is only ever the one. */
        block[14] = kScsiCommandLength;

        return kCommandAt;
    }

    VoidResult ProgrammerV1::discard_status_wrapper(const char *what)
    {
        std::uint8_t wrapper[kStatusWrapperSize] = {};

        auto read = channel().transport().receive(wrapper, sizeof(wrapper), kDefaultTimeout);

        if (!read.ok())
        {
            return read.error().wrap(ErrorCode::IO, what);
        }

        /*
         * Its contents are not examined: the firmware reports nothing in it
         * that the reply has not already said. Reading it is what matters,
         * since anything left behind becomes the next reply.
         */
        ++tag_;

        return {};
    }

    Result<std::size_t> ProgrammerV1::exchange(const CommandBlock &block, std::uint8_t *reply,
                                               std::size_t reply_size, const char *what)
    {
        /*
         * Check::None throughout: a V1 puts no status byte in front of its
         * replies, so there is nothing for the channel to check.
         */
        auto sent = channel().exchange(block.data(), block.size(), reply, reply_size,
                                       Check::None, what);

        if (!sent.ok())
        {
            return sent.error();
        }

        const std::size_t arrived = sent.value();

        auto discarded = discard_status_wrapper(what);

        if (!discarded.ok())
        {
            return discarded.error();
        }

        return arrived;
    }

    VoidResult ProgrammerV1::send(const CommandBlock &block, const char *what)
    {
        auto sent = channel().send(block.data(), block.size(), what);

        if (!sent.ok())
        {
            return sent;
        }

        return discard_status_wrapper(what);
    }

    /* ---- The programmer itself ----------------------------------------- */

    Result<ProgrammerMode> ProgrammerV1::mode()
    {
        constexpr std::size_t kReply = 2;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::GetCurrentMode);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "asking what mode the programmer is in");

        if (!sent.ok())
        {
            return sent.error();
        }

        if (sent.value() < 1)
        {
            return Error(ErrorCode::Protocol, "the programmer named no mode");
        }

        switch (reply[0])
        {
        case kModeDfu:
            return ProgrammerMode::Dfu;

        case kModeMassStorage:
            return ProgrammerMode::MassStorage;

        case kModeDebug:
            return ProgrammerMode::Debug;

        default:
            return ProgrammerMode::Unknown;
        }
    }

    Result<std::uint32_t> ProgrammerV1::target_voltage()
    {
        constexpr std::size_t kReply = 8;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::GetTargetVoltage);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "reading the target voltage");

        if (!sent.ok())
        {
            return sent.error();
        }

        const std::uint32_t reference = read_u32(&reply[0]);
        const std::uint32_t measured = read_u32(&reply[4]);

        if (reference == 0 || measured == 0)
        {
            return Error(ErrorCode::Protocol,
                         "the programmer could not measure the target voltage");
        }

        return static_cast<std::uint32_t>(2400ull * measured / reference);
    }

    Result<std::vector<std::uint32_t>> ProgrammerV1::clock_rates()
    {
        /* Not an error: this firmware offers none. */
        return std::vector<std::uint32_t>{};
    }

    VoidResult ProgrammerV1::set_clock(std::uint32_t)
    {
        return Error(ErrorCode::NotSupported,
                     "the original command set has no way to be told a debug clock rate");
    }

    /* ---- Getting to a target -------------------------------------------- */

    VoidResult ProgrammerV1::exit_dfu()
    {
        auto block = begin(Direction::FromDevice, 0);
        block.command(Command::Dfu).command(DfuCommand::Exit);

        return send(block, "leaving firmware update mode");
    }

    VoidResult ProgrammerV1::enter_debug(DebugMode debug)
    {
        if (debug == DebugMode::Jtag)
        {
            return Error(ErrorCode::NotSupported, "entering debug over JTAG");
        }

        /*
         * Connecting under reset is refused a layer up rather than here,
         * because what this firmware lacks is the command to drive nRST, and
         * that refusal comes from reset_pin() when the sequence reaches it.
         */
        auto block = begin(Direction::FromDevice, 0);
        block.command(Command::Debug)
            .command(DebugCommand::ApiV1Enter)
            .command(DebugCommand::EnterSwd);

        return send(block, "entering debug over SWD");
    }

    VoidResult ProgrammerV1::exit_debug()
    {
        auto block = begin(Direction::FromDevice, 0);
        block.command(Command::Debug).command(DebugCommand::Exit);

        return send(block, "leaving debug mode");
    }

    VoidResult ProgrammerV1::select_access_port(std::uint8_t)
    {
        /*
         * Opening an access port arrived with the V2 command set. A V1 reaches
         * whatever is on AP0 and nothing else.
         */
        return Error(ErrorCode::NotSupported, "opening an access port on this firmware");
    }

    Result<std::uint32_t> ProgrammerV1::core_id()
    {
        constexpr std::size_t kReply = 4;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ReadCoreId);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "reading the core id");

        if (!sent.ok())
        {
            return sent.error();
        }

        return read_u32(&reply[kValueOffset]);
    }

    /* ---- Controlling the target ------------------------------------------ */

    Result<CoreState> ProgrammerV1::state()
    {
        constexpr std::size_t kReply = 2;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::GetStatus);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply),
                             "reading whether the target is running");

        if (!sent.ok())
        {
            return sent.error();
        }

        if (sent.value() < 1)
        {
            return CoreState::Unknown;
        }

        switch (reply[0])
        {
        case kCoreRunning:
            return CoreState::Running;

        case kCoreHalted:
            return CoreState::Halted;

        default:
            /*
             * The original command set reports running or halted and nothing
             * else: it has no notion of a target held in reset, nor of
             * whether the debug unit is attached.
             */
            return CoreState::Unknown;
        }
    }

    VoidResult ProgrammerV1::halt()
    {
        constexpr std::size_t kReply = 2;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ForceDebug);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "halting the target");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    VoidResult ProgrammerV1::run(bool flash_loader)
    {
        if (flash_loader)
        {
            /*
             * Running with interrupts masked is a DHCSR write, and the
             * original command set cannot write DHCSR. The loader is started
             * anyway, because refusing would leave no way to flash at all.
             */
            STLINK_LOG_WRN("starting a flash loader without masking interrupts, "
                           "which this firmware cannot do");
        }

        constexpr std::size_t kReply = 2;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::RunCore);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "starting the target");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    VoidResult ProgrammerV1::step()
    {
        constexpr std::size_t kReply = 2;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::StepCore);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "stepping the target");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    VoidResult ProgrammerV1::reset()
    {
        constexpr std::size_t kReply = 2;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ApiV1ResetSys);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "resetting the target");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    VoidResult ProgrammerV1::reset_pin(bool)
    {
        /* Driving nRST directly arrived with the V2 command set. */
        return Error(ErrorCode::NotSupported,
                     "driving the target's reset pin on this firmware");
    }

    /* ---- Target memory ---------------------------------------------------- */

    VoidResult ProgrammerV1::read_memory(std::uint32_t address, std::uint8_t *data,
                                         std::size_t size)
    {
        if (size == 0)
        {
            return {};
        }

        if (data == nullptr)
        {
            return Error(ErrorCode::InvalidArgument, "reading target memory into nothing");
        }

        if ((address % 4) != 0 || (size % 4) != 0)
        {
            return Error(
                ErrorCode::InvalidArgument,
                "reading target memory at an address or length that is not a multiple of four");
        }

        if (size > kMemoryTransferLimit)
        {
            return Error(ErrorCode::InvalidArgument,
                         "reading more target memory than one command carries");
        }

        auto block = begin(Direction::FromDevice, size);
        block.command(Command::Debug)
            .command(DebugCommand::ReadMemory32Bit)
            .u32(address)
            .u16(static_cast<std::uint16_t>(size));

        auto sent = exchange(block, data, size, "reading target memory");

        if (!sent.ok())
        {
            return sent.error();
        }

        if (sent.value() != size)
        {
            return Error(ErrorCode::Protocol, "the programmer returned less memory than asked for");
        }

        return {};
    }

    VoidResult ProgrammerV1::write_memory(std::uint32_t address, const std::uint8_t *data,
                                          std::size_t size)
    {
        if (size == 0)
        {
            return {};
        }

        if (data == nullptr)
        {
            return Error(ErrorCode::InvalidArgument, "writing target memory from nothing");
        }

        const bool aligned = (address % 4) == 0 && (size % 4) == 0;

        if (!aligned && size > kByteWriteLimit)
        {
            return Error(ErrorCode::InvalidArgument,
                         "writing more unaligned target memory than one command carries");
        }

        if (aligned && size > kMemoryTransferLimit)
        {
            return Error(ErrorCode::InvalidArgument,
                         "writing more target memory than one command carries");
        }

        auto block = begin(Direction::ToDevice, 0);
        block.command(Command::Debug)
            .command(aligned ? DebugCommand::WriteMemory32Bit : DebugCommand::WriteMemory8Bit)
            .u32(address)
            .u16(static_cast<std::uint16_t>(size));

        /* The descriptor first, with no wrapper: the operation is not over. */
        auto announced = channel().send(block.data(), block.size(), "announcing a memory write");

        if (!announced.ok())
        {
            return announced;
        }

        auto written = channel().transport().send(data, size, kDefaultTimeout);

        if (!written.ok())
        {
            return written.error().wrap(ErrorCode::IO, "writing target memory");
        }

        if (written.value() != size)
        {
            return Error(ErrorCode::IO, "the programmer took only part of a memory write");
        }

        /*
         * Now the operation is over, and the wrapper follows the data rather
         * than the command. There is no command to confirm the write with:
         * the read/write status command arrived with the V2 set.
         */
        return discard_status_wrapper("writing target memory");
    }

    /* ---- Debug unit registers ---------------------------------------------- */

    Result<std::uint32_t> ProgrammerV1::read_debug_register(std::uint32_t address)
    {
        /*
         * The original command set has no command for this at all. The debug
         * registers are memory mapped, so they are read as memory, which
         * reaches the same words by the only route this firmware offers.
         */
        std::uint8_t word[4] = {};

        auto read = read_memory(address, word, sizeof(word));

        if (!read.ok())
        {
            return read.error().wrap(ErrorCode::IO, "reading a debug register");
        }

        return read_u32(word);
    }

    VoidResult ProgrammerV1::write_debug_register(std::uint32_t address, std::uint32_t value)
    {
        /* See read_debug_register: the same route, for the same reason. */
        std::uint8_t word[4] = {};

        write_u32(word, value);

        auto written = write_memory(address, word, sizeof(word));

        if (!written.ok())
        {
            return written.error().wrap(ErrorCode::IO, "writing a debug register");
        }

        return {};
    }

    /* ---- Core registers ------------------------------------------------------ */

    Result<std::uint32_t> ProgrammerV1::read_register(std::uint8_t index)
    {
        constexpr std::size_t kReply = 4;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ApiV1ReadReg).u8(index);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "reading a core register");

        if (!sent.ok())
        {
            return sent.error();
        }

        return read_u32(&reply[kValueOffset]);
    }

    VoidResult ProgrammerV1::write_register(std::uint8_t index, std::uint32_t value)
    {
        constexpr std::size_t kReply = 2;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ApiV1WriteReg).u8(index).u32(value);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "writing a core register");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    Result<CoreRegisters> ProgrammerV1::read_registers()
    {
        /* Sixteen general registers and five more, with nothing in front. */
        constexpr std::size_t kReply = 84;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ApiV1ReadAllRegs);

        std::uint8_t reply[kReply] = {};

        auto sent = exchange(block, reply, sizeof(reply), "reading the core registers");

        if (!sent.ok())
        {
            return sent.error();
        }

        CoreRegisters registers;

        for (std::size_t i = 0; i < 16; ++i)
        {
            registers.r[i] = read_u32(&reply[kValueOffset + i * 4]);
        }

        registers.xpsr = read_u32(&reply[kValueOffset + 64]);
        registers.main_sp = read_u32(&reply[kValueOffset + 68]);
        registers.process_sp = read_u32(&reply[kValueOffset + 72]);
        registers.rw = read_u32(&reply[kValueOffset + 76]);
        registers.rw2 = read_u32(&reply[kValueOffset + 80]);

        return registers;
    }

    /* ---- Trace, of which there is none ---------------------------------------- */

    VoidResult ProgrammerV1::trace_enable(std::uint32_t)
    {
        return Error(ErrorCode::NotSupported, "delivering trace output");
    }

    VoidResult ProgrammerV1::trace_disable()
    {
        return Error(ErrorCode::NotSupported, "delivering trace output");
    }

    Result<std::size_t> ProgrammerV1::trace_read(std::uint8_t *, std::size_t)
    {
        return Error(ErrorCode::NotSupported, "delivering trace output");
    }
} // namespace stlink
