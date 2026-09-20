/**
 * @file    programmer_v2.cpp
 * @brief   A programmer speaking the V2 command set.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/programmer_v2.h>

#include <cstring>
#include <iterator>
#include <vector>

#include <stlink/cortex.h>
#include <stlink/log.h>

namespace stlink
{
    namespace
    {
        /* A reply that is a status byte and a padding byte, and nothing else. */
        constexpr std::size_t kStatusReply = 2;

        /** @brief Read a little endian word out of a reply. */
        std::uint32_t read_u32(const std::uint8_t *from) noexcept
        {
            return static_cast<std::uint32_t>(from[0]) |
                   (static_cast<std::uint32_t>(from[1]) << 8) |
                   (static_cast<std::uint32_t>(from[2]) << 16) |
                   (static_cast<std::uint32_t>(from[3]) << 24);
        }

        std::uint16_t read_u16(const std::uint8_t *from) noexcept
        {
            return static_cast<std::uint16_t>(from[0] | (from[1] << 8));
        }

        /*
         * A V2 answers a register read with a status byte and three of
         * padding before the value; the original V1 command set answers with
         * the value alone. Four, here, is the V2 layout.
         */
        constexpr std::size_t kValueOffset = 4;

        /** @brief The most a 32-bit memory transfer may carry in one command. */
        constexpr std::size_t kMemoryTransferLimit = 6144;

        /** @brief What a V2 will buffer of the target's trace output. */
        constexpr std::size_t kTraceBufferV2 = 2048;

        /** @brief The most an unaligned write may carry on a V2. */
        constexpr std::size_t kByteWriteLimitV2 = 64;

        /* What the firmware calls the modes it can be in. */
        constexpr std::uint8_t kModeDfu = 0;
        constexpr std::uint8_t kModeMassStorage = 1;
        constexpr std::uint8_t kModeDebug = 2;

        /* The nRST pin, driven directly. */
        constexpr std::uint8_t kNrstLow = 0x00;
        constexpr std::uint8_t kNrstHigh = 0x01;

        /**
         * @brief The clock rates a V2 offers, and the divisors that mean them.
         *
         * Fixed in the firmware, which takes a divisor rather than a rate and
         * accepts only these. Ordered fastest first, as reported.
         */
        struct ClockRate
        {
            std::uint32_t hz;
            std::uint16_t divisor;
        };

        constexpr ClockRate kClockRatesV2[] = {
            {4000000, 0},  {1800000, 1},  {1200000, 2}, {950000, 3},
            {480000, 7},   {240000, 15},  {125000, 31}, {100000, 40},
            {50000, 79},   {25000, 158},  {15000, 265}, {5000, 798},
        };

        /** @brief The firmware revision at which a V2 will accept a clock rate. */
        constexpr std::uint8_t kClockSettableFrom = 22;

        /** @brief What a V2 is set to when the caller has no opinion. */
        constexpr std::uint32_t kDefaultRateV2 = 1800000;
    } // namespace

    ProgrammerV2::ProgrammerV2(CommandChannel channel, VersionReport report) noexcept
        : ProgrammerBase(std::move(channel), report)
    {
    }

    /* ---- The programmer itself ---------------------------------------- */

    Result<ProgrammerMode> ProgrammerV2::mode()
    {
        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::GetCurrentMode);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::None, "asking what mode the programmer is in");

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

    Result<std::uint32_t> ProgrammerV2::target_voltage()
    {
        constexpr std::size_t kReply = 8;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::GetTargetVoltage);

        std::uint8_t reply[kReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::ReplyLength, "reading the target voltage");

        if (!sent.ok())
        {
            return sent.error();
        }

        /*
         * Two readings of the same converter: one of an internal reference of
         * known value, one of the target's pin. The ratio is what matters, so
         * the converter's own scale cancels out.
         */
        const std::uint32_t reference = read_u32(&reply[0]);
        const std::uint32_t measured = read_u32(&reply[4]);

        if (reference == 0 || measured == 0)
        {
            return Error(ErrorCode::Protocol, "the programmer could not measure the target voltage");
        }

        /* The reference is 2.4V, and the answer is in millivolts. */
        return static_cast<std::uint32_t>(2400ull * measured / reference);
    }

    Result<std::vector<std::uint32_t>> ProgrammerV2::clock_rates()
    {
        if (version().major != 2 || version().jtag < kClockSettableFrom)
        {
            /* Not an error: this firmware simply offers none. */
            return std::vector<std::uint32_t>{};
        }

        std::vector<std::uint32_t> rates;
        rates.reserve(std::size(kClockRatesV2));

        for (const ClockRate &rate : kClockRatesV2)
        {
            rates.push_back(rate.hz);
        }

        return rates;
    }

    VoidResult ProgrammerV2::set_clock(std::uint32_t hz)
    {
        if (version().major != 2 || version().jtag < kClockSettableFrom)
        {
            return Error(ErrorCode::NotSupported,
                         "this firmware is too old to be told a debug clock rate");
        }

        const std::uint32_t wanted = (hz != 0) ? hz : kDefaultRateV2;

        std::uint16_t divisor = 0;
        bool offered = false;

        for (const ClockRate &rate : kClockRatesV2)
        {
            if (rate.hz == wanted)
            {
                divisor = rate.divisor;
                offered = true;

                break;
            }
        }

        if (!offered)
        {
            /* Rounding either way would change what was asked for without saying so. */
            return Error(ErrorCode::InvalidArgument,
                         "this programmer does not offer that debug clock rate");
        }

        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2SwdSetFrequency).u16(divisor);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "setting the debug clock rate");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    /* ---- Getting to a target ------------------------------------------ */

    VoidResult ProgrammerV2::exit_dfu()
    {
        auto block = begin(Direction::FromDevice, 0);
        block.command(Command::Dfu).command(DfuCommand::Exit);

        return channel().send(block.data(), block.size(), "leaving firmware update mode");
    }

    VoidResult ProgrammerV2::enter_debug(DebugMode debug, ResetMode reset)
    {
        if (debug == DebugMode::Jtag)
        {
            /*
             * No firmware this library has seen accepts a JTAG entry over
             * USB; the command exists but the vtable it came from left the
             * slot empty. Refusing says so, rather than sending a command
             * that will not be understood.
             */
            return Error(ErrorCode::NotSupported, "entering debug over JTAG");
        }

        if (reset == ResetMode::UnderReset)
        {
            /* Hold the target still, so it cannot run away before we attach. */
            auto held = reset_pin(true);

            if (!held.ok())
            {
                return held.error().wrap(ErrorCode::IO, "holding the target in reset to connect");
            }
        }

        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug)
            .command(DebugCommand::ApiV2Enter)
            .command(DebugCommand::EnterSwd);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "entering debug over SWD");

        if (!sent.ok())
        {
            return sent.error();
        }

        if (reset == ResetMode::UnderReset)
        {
            auto released = reset_pin(false);

            if (!released.ok())
            {
                return released.error().wrap(ErrorCode::IO, "releasing the target after connecting");
            }
        }

        return {};
    }

    VoidResult ProgrammerV2::exit_debug()
    {
        auto block = begin(Direction::FromDevice, 0);
        block.command(Command::Debug).command(DebugCommand::Exit);

        return channel().send(block.data(), block.size(), "leaving debug mode");
    }

    VoidResult ProgrammerV2::select_access_port(std::uint8_t ap)
    {
        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2InitAccessPort).u8(ap);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "opening an access port");

        if (!sent.ok())
        {
            return sent.error();
        }

        /* Only once the firmware agreed, or later reads would go to a port that is not open. */
        set_access_port(ap);

        return {};
    }

    Result<std::uint32_t> ProgrammerV2::core_id()
    {
        constexpr std::size_t kReply = 12;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2ReadIdCodes);

        std::uint8_t reply[kReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Status, "reading the core id");

        if (!sent.ok())
        {
            return sent.error();
        }

        return read_u32(&reply[kValueOffset]);
    }

    /* ---- Controlling the target ---------------------------------------- */

    Result<CoreState> ProgrammerV2::state()
    {
        auto status = read_debug_register(kDhcsr);

        if (!status.ok())
        {
            return status.error().wrap(ErrorCode::IO, "reading whether the target is running");
        }

        const std::uint32_t dhcsr = status.value();

        if ((dhcsr & kDhcsrHalt) != 0)
        {
            return CoreState::Halted;
        }

        if ((dhcsr & kDhcsrResetSince) != 0)
        {
            return CoreState::Reset;
        }

        /*
         * Running, and the debug bit says whether we are attached to it. The
         * C never made this distinction; the interface has the state, so it
         * is reported rather than flattened.
         */
        return (dhcsr & kDhcsrDebugEnable) != 0 ? CoreState::DebugRunning : CoreState::Running;
    }

    VoidResult ProgrammerV2::write_dhcsr(std::uint32_t bits)
    {
        return write_debug_register(kDhcsr, kDhcsrKey | bits);
    }

    VoidResult ProgrammerV2::halt()
    {
        return write_dhcsr(kDhcsrHalt | kDhcsrDebugEnable);
    }

    VoidResult ProgrammerV2::run(bool flash_loader)
    {
        /*
         * A flash loader runs with interrupts masked. It is a short routine
         * written into SRAM, and anything the target would otherwise service
         * would run from flash that is being erased underneath it.
         */
        return write_dhcsr(kDhcsrDebugEnable | (flash_loader ? kDhcsrMaskInterrupts : 0));
    }

    VoidResult ProgrammerV2::step()
    {
        /*
         * Three writes, because there is no step command: mask interrupts
         * while halted, so the step does not land inside a handler; step;
         * then halt again with the mask lifted.
         */
        auto masked = write_dhcsr(kDhcsrHalt | kDhcsrMaskInterrupts | kDhcsrDebugEnable);

        if (!masked.ok())
        {
            return masked.error().wrap(ErrorCode::IO, "holding interrupts off to step");
        }

        auto stepped = write_dhcsr(kDhcsrStep | kDhcsrMaskInterrupts | kDhcsrDebugEnable);

        if (!stepped.ok())
        {
            return stepped.error().wrap(ErrorCode::IO, "stepping the target");
        }

        return write_dhcsr(kDhcsrHalt | kDhcsrDebugEnable);
    }

    VoidResult ProgrammerV2::reset()
    {
        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2ResetSys);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "resetting the target");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    VoidResult ProgrammerV2::reset_pin(bool asserted)
    {
        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug)
            .command(DebugCommand::ApiV2DriveNrst)
            .u8(asserted ? kNrstLow : kNrstHigh);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "driving the target's reset pin");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    /* ---- Target memory -------------------------------------------------- */

    VoidResult ProgrammerV2::read_memory(std::uint32_t address, std::uint8_t *data,
                                         std::size_t size)
    {
        /* Nothing to do, and so nothing to be wrong about. */
        if (size == 0)
        {
            return {};
        }

        if (data == nullptr)
        {
            return Error(ErrorCode::InvalidArgument, "reading target memory into nothing");
        }

        /* The only read the firmware has is a word read, so both ends must be aligned. */
        if ((address % 4) != 0 || (size % 4) != 0)
        {
            return Error(ErrorCode::InvalidArgument,
                         "reading target memory at an address or length that is not a multiple of four");
        }

        if (size > kMemoryTransferLimit)
        {
            return Error(ErrorCode::InvalidArgument, "reading more target memory than one command carries");
        }

        auto block = begin(Direction::FromDevice, size);
        block.command(Command::Debug)
            .command(DebugCommand::ReadMemory32Bit)
            .u32(address)
            .u16(static_cast<std::uint16_t>(size))
            .u8(access_port());

        auto sent = channel().exchange(block.data(), block.size(), data, size, Check::ReplyLength,
                                       "reading target memory");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    VoidResult ProgrammerV2::write_memory(std::uint32_t address, const std::uint8_t *data,
                                          std::size_t size)
    {
        /* Nothing to do, and so nothing to be wrong about. */
        if (size == 0)
        {
            return {};
        }

        if (data == nullptr)
        {
            return Error(ErrorCode::InvalidArgument, "writing target memory from nothing");
        }

        const bool aligned = (address % 4) == 0 && (size % 4) == 0;

        if (!aligned && size > byte_write_limit())
        {
            return Error(ErrorCode::InvalidArgument,
                         "writing more unaligned target memory than one command carries");
        }

        if (aligned && size > kMemoryTransferLimit)
        {
            return Error(ErrorCode::InvalidArgument, "writing more target memory than one command carries");
        }

        auto block = begin(Direction::ToDevice, 0);
        block.command(Command::Debug)
            .command(aligned ? DebugCommand::WriteMemory32Bit : DebugCommand::WriteMemory8Bit)
            .u32(address)
            .u16(static_cast<std::uint16_t>(size))
            .u8(access_port());

        auto announced = channel().send(block.data(), block.size(), "announcing a memory write");

        if (!announced.ok())
        {
            return announced.error();
        }

        /* The data follows on the same channel, as its own transfer. */
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
         * A byte write answers with nothing at all and cannot be confirmed.
         * A word write can be, and is: a write that faulted is otherwise
         * indistinguishable from one that landed.
         */
        if (!aligned)
        {
            return {};
        }

        return check_last_write();
    }

    VoidResult ProgrammerV2::check_last_write()
    {
        const bool detailed = has_capability(capabilities(), Capability::LastRwStatus2);

        const std::size_t reply_size = detailed ? 12u : kStatusReply;

        auto block = begin(Direction::FromDevice, reply_size);
        block.command(Command::Debug)
            .command(detailed ? DebugCommand::ApiV2GetLastRwStatus2
                              : DebugCommand::ApiV2GetLastRwStatus);

        std::uint8_t reply[12] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, reply_size,
                                       Check::Status, "confirming a memory write");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    /* ---- Debug unit registers -------------------------------------------- */

    Result<std::uint32_t> ProgrammerV2::read_debug_register(std::uint32_t address)
    {
        /*
         * The native command does not honour a selected access port, so on
         * anything but AP0 the debug registers are reached as memory. They
         * are memory mapped, so this reads the same thing either way.
         */
        if (access_port() != 0)
        {
            std::uint8_t word[4] = {};

            auto read = read_memory(address, word, sizeof(word));

            if (!read.ok())
            {
                return read.error();
            }

            return read_u32(word);
        }

        constexpr std::size_t kReply = 8;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2ReadDebugReg).u32(address);

        std::uint8_t reply[kReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "reading a debug register");

        if (!sent.ok())
        {
            return sent.error();
        }

        return read_u32(&reply[kValueOffset]);
    }

    VoidResult ProgrammerV2::write_debug_register(std::uint32_t address, std::uint32_t value)
    {
        /* See read_debug_register: the same detour, for the same reason. */
        if (access_port() != 0)
        {
            const std::uint8_t word[4] = {
                static_cast<std::uint8_t>(value & 0xff),
                static_cast<std::uint8_t>((value >> 8) & 0xff),
                static_cast<std::uint8_t>((value >> 16) & 0xff),
                static_cast<std::uint8_t>((value >> 24) & 0xff),
            };

            return write_memory(address, word, sizeof(word));
        }

        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug)
            .command(DebugCommand::ApiV2WriteDebugReg)
            .u32(address)
            .u32(value);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "writing a debug register");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    /* ---- Core registers --------------------------------------------------- */

    Result<std::uint32_t> ProgrammerV2::read_register(std::uint8_t index)
    {
        constexpr std::size_t kReply = 8;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug)
            .command(DebugCommand::ApiV2ReadReg)
            .u8(index)
            .u8(access_port());

        std::uint8_t reply[kReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "reading a core register");

        if (!sent.ok())
        {
            return sent.error();
        }

        return read_u32(&reply[kValueOffset]);
    }

    VoidResult ProgrammerV2::write_register(std::uint8_t index, std::uint32_t value)
    {
        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug)
            .command(DebugCommand::ApiV2WriteReg)
            .u8(index)
            .u32(value)
            .u8(access_port());

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Retry, "writing a core register");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    Result<CoreRegisters> ProgrammerV2::read_registers()
    {
        /* A status word, then sixteen general registers and five more. */
        constexpr std::size_t kReply = 88;

        auto block = begin(Direction::FromDevice, kReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2ReadAllRegs).u8(access_port());

        std::uint8_t reply[kReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Status, "reading the core registers");

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

    /* ---- Trace ------------------------------------------------------------- */

    std::size_t ProgrammerV2::byte_write_limit() const noexcept
    {
        return kByteWriteLimitV2;
    }

    std::size_t ProgrammerV2::trace_buffer_size() const noexcept
    {
        return kTraceBufferV2;
    }

    VoidResult ProgrammerV2::trace_enable(std::uint32_t hz)
    {
        auto allowed = require(Capability::Trace, "delivering trace output");

        if (!allowed.ok())
        {
            return allowed;
        }

        if (hz > max_trace_frequency())
        {
            return Error(ErrorCode::InvalidArgument,
                         "asking for trace faster than this programmer carries");
        }

        auto block = begin(Direction::ToDevice, kStatusReply);
        block.command(Command::Debug)
            .command(DebugCommand::ApiV2StartTraceRx)
            .u16(static_cast<std::uint16_t>(2 * trace_buffer_size()))
            .u32(hz);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Status, "starting trace");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    VoidResult ProgrammerV2::trace_disable()
    {
        auto allowed = require(Capability::Trace, "delivering trace output");

        if (!allowed.ok())
        {
            return allowed;
        }

        auto block = begin(Direction::ToDevice, kStatusReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2StopTraceRx);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::Status, "stopping trace");

        if (!sent.ok())
        {
            return sent.error();
        }

        return {};
    }

    Result<std::size_t> ProgrammerV2::trace_read(std::uint8_t *data, std::size_t size)
    {
        auto allowed = require(Capability::Trace, "delivering trace output");

        if (!allowed.ok())
        {
            return allowed.error();
        }

        if (data == nullptr && size != 0)
        {
            return Error(ErrorCode::InvalidArgument, "reading trace into nothing");
        }

        /* How much is waiting, before reading any of it. */
        auto block = begin(Direction::FromDevice, kStatusReply);
        block.command(Command::Debug).command(DebugCommand::ApiV2GetTraceCount);

        std::uint8_t reply[kStatusReply] = {};

        auto sent = channel().exchange(block.data(), block.size(), reply, sizeof(reply),
                                       Check::ReplyLength, "asking how much trace is waiting");

        if (!sent.ok())
        {
            return sent.error();
        }

        const std::size_t waiting = read_u16(reply);

        if (waiting == 0)
        {
            return std::size_t{0};
        }

        if (waiting > size)
        {
            /*
             * Reading part of it would leave the rest to be misread as the
             * start of a packet later, so this is refused rather than
             * truncated.
             */
            return Error(ErrorCode::InvalidArgument, "more trace is waiting than there is room for");
        }

        auto read = channel().transport().receive_trace(data, waiting, kDefaultTimeout);

        if (!read.ok())
        {
            return read.error().wrap(ErrorCode::IO, "reading trace output");
        }

        const std::size_t arrived = read.value();

        if (arrived != waiting)
        {
            STLINK_LOG_WRN("trace: %zu bytes were waiting and %zu arrived", waiting, arrived);
        }

        return arrived;
    }
} // namespace stlink
