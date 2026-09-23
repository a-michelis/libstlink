/**
 * @file    device.cpp
 * @brief   Finding what is attached, and opening it.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The public surface, and the only place where a programmer and a chip are
 * put together. Everything below this file talks about probes and command
 * blocks; everything a caller sees talks about devices.
 *
 * The flash a device hands out is a placeholder that refuses everything. No
 * family is implemented yet, and IDevice::flash() returns a reference rather
 * than a pointer, so there has to be something to return. Refusing by name is
 * better than a null reference and better than pretending.
 */

#include <stlink/device.h>

#include <mutex>
#include <set>
#include <utility>

#include <stlink/chip_database.h>
#include <stlink/chip_identity.h>
#include <stlink/flash.h>
#include <stlink/log.h>
#include <stlink/programmer.h>
#include <stlink/protocol.h>
#include <stlink/trace.h>
#include <stlink/transport.h>

namespace stlink
{
    const DeviceFilter DeviceFilter::NoFilter{};

    const char *to_string(ProgrammerKind kind) noexcept
    {
        switch (kind)
        {
        case ProgrammerKind::V1:
            return "ST-LINK/V1";
        case ProgrammerKind::V2:
            return "ST-LINK/V2";
        case ProgrammerKind::V2_1:
            return "ST-LINK/V2-1";
        case ProgrammerKind::V3:
            return "STLINK-V3";
        default:
            return "unknown";
        }
    }

    namespace
    {
        /**
         * @brief Which generation a product id names, in public terms.
         *
         * Generation is what the protocol layer calls it and ProgrammerKind
         * is what a caller sees. They agree entry for entry, which is not an
         * accident but is also not something either side should rely on, so
         * the mapping is written out.
         */
        ProgrammerKind kind_of(Generation generation) noexcept
        {
            switch (generation)
            {
            case Generation::V1:
                return ProgrammerKind::V1;
            case Generation::V2:
                return ProgrammerKind::V2;
            case Generation::V2_1:
                return ProgrammerKind::V2_1;
            case Generation::V3:
                return ProgrammerKind::V3;
            default:
                return ProgrammerKind::Unknown;
            }
        }

        BasicDeviceInfo basic_from(const ProbeAddress &probe)
        {
            BasicDeviceInfo info;

            info.kind = kind_of(generation_of(probe.pid));
            info.serial = probe.serial;
            info.vid = probe.vid;
            info.pid = probe.pid;
            info.location = probe.location;

            return info;
        }

        /**
         * @brief How a device is told apart from the others while it is open.
         *
         * The serial, which is what identifies one adapter across a replug.
         * A programmer that reports none falls back to where it is attached,
         * which is not stable across a replug but is unique while it stays
         * put, and that is all this has to be.
         */
        std::string key_of(const BasicDeviceInfo &info)
        {
            return info.serial.empty() ? "@" + info.location : info.serial;
        }

        /** @brief What the filter can decide without touching a target. */
        bool accepts_probe(const DeviceFilter &filter, const BasicDeviceInfo &info)
        {
            if (filter.serial.has_value() && *filter.serial != info.serial)
            {
                return false;
            }

            if (filter.kind.has_value() && *filter.kind != info.kind)
            {
                return false;
            }

            return true;
        }

        /**
         * @brief How much flash the chip says it has, in bytes.
         *
         * Read from the chip rather than taken from the description, because
         * one part number ships in several sizes. What the word means is
         * flash_size_from()'s business; this only fetches it, word aligned,
         * as the register demands.
         */
        Result<std::uint32_t> read_flash_size(IProgrammer &programmer, const ChipDescription &chip)
        {
            if (chip.flash_size_reg == 0)
            {
                return 0u;
            }

            auto read = programmer.read_debug_register(chip.flash_size_reg & ~3u);

            if (!read.ok())
            {
                return read.error().wrap(ErrorCode::TargetRefused, "reading the flash size");
            }

            return flash_size_from(chip, read.value());
        }

        /** @brief The public view of a chip, once it has been read. */
        ChipInfo chip_info_from(const ChipDescription &chip, std::uint32_t chip_id,
                                std::uint32_t flash_size)
        {
            ChipInfo info;

            info.id = chip_id;
            info.name = chip.name;
            info.reference = chip.reference;

            info.flash = chip.flash;
            info.flash.size = flash_size;
            info.flash_page_size = chip.flash_page_size;

            info.sram = chip.sram;
            info.bootrom = chip.bootrom;
            info.option_bytes = chip.option_bytes;
            info.otp = chip.otp;

            return info;
        }

        /** @brief What the programmer said about itself during the handshake. */
        ProgrammerInfo programmer_info_of(const IProgrammer &programmer)
        {
            ProgrammerInfo info;

            info.debug_firmware = programmer.version().jtag;
            info.swim_firmware = programmer.version().swim;
            info.max_trace_frequency = programmer.max_trace_frequency();

            return info;
        }

        /** @brief Whether this chip's description claims an SWO pin. */
        bool chip_has_swo(std::uint32_t chip_id)
        {
            const ChipDescription *known = ChipDatabase::instance().find(chip_id);

            return known != nullptr && has_flag(known->flags, ChipFlags::Swo);
        }

        /**
         * @brief Connect through to the chip and describe it.
         *
         * Leaves the programmer in debug mode, since every caller here either
         * keeps it or drops it immediately afterwards.
         */
        Result<ChipInfo> read_the_chip(IProgrammer &programmer, const DeviceOptions &options)
        {
            /*
             * Before connecting rather than after, because the handshake is
             * itself carried at this rate: one a marginal target cannot
             * sustain fails the connection, not merely the traffic after it.
             *
             * A programmer that cannot be told a rate at all, which is every
             * ST-LINK/V1, is only a problem for a caller who asked for a
             * particular one. Asking for the default and being told there is
             * no choice to make is not a failure to connect.
             */
            auto clocked = programmer.set_clock(options.clock_hz);

            if (!clocked.ok())
            {
                if (options.clock_hz != 0)
                {
                    return clocked.error().wrap(ErrorCode::InvalidArgument,
                                                "setting the debug clock");
                }

                STLINK_LOG_DBG("this programmer takes no clock rate, using whatever it has");
            }

            auto entered = programmer.enter_debug(options.debug, options.reset);

            if (!entered.ok())
            {
                return entered.error().wrap(ErrorCode::TargetUnknown, "connecting to the chip");
            }

            auto asked = read_chip_id(programmer);

            if (!asked.ok())
            {
                return asked.error();
            }

            const std::uint32_t chip_id = asked.value();

            const ChipDescription *known = ChipDatabase::instance().find(chip_id);

            if (known == nullptr)
            {
                /*
                 * Not an error. The chip answered, we simply have no file for
                 * it, and ChipInfo::name is documented as empty for exactly
                 * this. A caller can still read memory and halt the core.
                 */
                STLINK_LOG_WRN("the chip reports %#05x, which is not in the database", chip_id);

                ChipInfo bare;
                bare.id = chip_id;

                return bare;
            }

            auto size = read_flash_size(programmer, *known);

            if (!size.ok())
            {
                return size.error();
            }

            return chip_info_from(*known, chip_id, size.value());
        }

        /**
         * @brief The flash of a chip whose family nobody has written yet.
         *
         * Every operation refuses by name. This is what IDevice::flash()
         * returns until the families exist, and it is deliberately not a
         * partial implementation: a flash that erases but cannot write would
         * be worse than one that admits it does nothing.
         */
        class UnsupportedFlash final : public IFlash
        {
        public:
            VoidResult erase(std::uint32_t, std::size_t, const Progress &) override
            {
                return refuse();
            }

            VoidResult erase_all(const Progress &) override
            {
                return refuse();
            }

            VoidResult write(std::uint32_t, const std::uint8_t *, std::size_t,
                             const Progress &) override
            {
                return refuse();
            }

            VoidResult verify(std::uint32_t, const std::uint8_t *, std::size_t,
                              const Progress &) override
            {
                return refuse();
            }

            bool has_option_bytes() const noexcept override
            {
                return false;
            }

            VoidResult read_option_bytes(std::uint8_t *, std::size_t) override
            {
                return refuse();
            }

            VoidResult write_option_bytes(const std::uint8_t *, std::size_t) override
            {
                return refuse();
            }

            bool has_otp() const noexcept override
            {
                return false;
            }

            VoidResult read_otp(std::uint32_t, std::uint8_t *, std::size_t) override
            {
                return refuse();
            }

            VoidResult write_otp(std::uint32_t, const std::uint8_t *, std::size_t) override
            {
                return refuse();
            }

        private:
            static VoidResult refuse()
            {
                return Error(ErrorCode::NotSupported,
                             "this chip's flash family is not implemented yet");
            }
        };

        /**
         * @brief Trace, where one end or the other cannot carry it.
         *
         * Returned when the programmer has no trace engine, which is every
         * ST-LINK/V1 and an early V2, or when the chip has no SWO pin. A
         * caller is expected to have asked has_trace() first; this is what
         * happens when they did not.
         */
        class UnavailableTrace final : public ITrace
        {
        public:
            VoidResult start(std::uint32_t) override
            {
                return refuse();
            }

            VoidResult stop() override
            {
                return refuse();
            }

            bool running() const noexcept override
            {
                return false;
            }

            Result<std::size_t> read(std::uint8_t *, std::size_t) override
            {
                return refuse().error();
            }

            std::uint32_t max_frequency() const noexcept override
            {
                return 0;
            }

        private:
            static VoidResult refuse()
            {
                return Error(ErrorCode::NotSupported,
                             "trace needs both a programmer that carries it and a chip with the pin");
            }
        };

        /** @brief Trace, where both ends have it. */
        class ProgrammerTrace final : public ITrace
        {
        public:
            explicit ProgrammerTrace(IProgrammer &programmer) noexcept : programmer_(programmer) {}

            VoidResult start(std::uint32_t hz) override
            {
                auto started = programmer_.trace_enable(hz);

                if (started.ok())
                {
                    running_ = true;
                }

                return started;
            }

            VoidResult stop() override
            {
                auto stopped = programmer_.trace_disable();

                /*
                 * Cleared either way. A stop that failed still leaves us with
                 * no reason to believe trace is arriving, and claiming it is
                 * running would be the more misleading of the two answers.
                 */
                running_ = false;

                return stopped;
            }

            bool running() const noexcept override
            {
                return running_;
            }

            Result<std::size_t> read(std::uint8_t *data, std::size_t size) override
            {
                return programmer_.trace_read(data, size);
            }

            std::uint32_t max_frequency() const noexcept override
            {
                return programmer_.max_trace_frequency();
            }

        private:
            IProgrammer &programmer_;
            bool running_ = false;
        };

        /**
         * @brief Which devices are open, so that a second one is refused.
         *
         * Process wide, because the thing being protected is the programmer,
         * and two of these in one process would fight over it exactly as two
         * processes would.
         */
        std::mutex &registry_lock()
        {
            static std::mutex lock;

            return lock;
        }

        std::set<std::string> &registry()
        {
            static std::set<std::string> open;

            return open;
        }

        bool claim(const std::string &key)
        {
            const std::lock_guard<std::mutex> held(registry_lock());

            return registry().insert(key).second;
        }

        void release(const std::string &key)
        {
            const std::lock_guard<std::mutex> held(registry_lock());

            registry().erase(key);
        }

        class Device final : public IDevice
        {
        public:
            Device(std::unique_ptr<IProgrammer> programmer, DeviceInfo info, std::string key,
                   bool chip_has_swo)
                : programmer_(std::move(programmer)), info_(std::move(info)), key_(std::move(key)),
                  trace_(*programmer_),
                  /*
                   * Both ends, decided once here rather than on every call.
                   * The programmer's half is whether it reports a trace rate
                   * at all; the chip's half is whether its description claims
                   * the pin.
                   */
                  has_trace_(chip_has_swo && programmer_->max_trace_frequency() != 0)
            {
            }

            ~Device() override
            {
                /*
                 * Let the target go before the connection does, so a chip
                 * that was running when we found it is running when we leave.
                 * A failure here is not something a destructor can report and
                 * not something a caller could act on, so it is logged.
                 */
                if (!programmer_->exit_debug().ok())
                {
                    STLINK_LOG_DBG("could not leave debug mode on the way out");
                }

                release(key_);
            }

            const DeviceInfo &info() const noexcept override
            {
                return info_;
            }

            VoidResult halt() override
            {
                return programmer_->halt();
            }

            VoidResult run() override
            {
                return programmer_->run();
            }

            VoidResult step() override
            {
                return programmer_->step();
            }

            VoidResult reset() override
            {
                return programmer_->reset();
            }

            Result<bool> is_running() override
            {
                auto state = programmer_->state();

                if (!state.ok())
                {
                    return state.error();
                }

                const CoreState current = state.value();

                return current == CoreState::Running || current == CoreState::DebugRunning;
            }

            VoidResult read(std::uint32_t address, std::uint8_t *data, std::size_t size) override
            {
                return programmer_->read_memory(address, data, size);
            }

            VoidResult write(std::uint32_t address, const std::uint8_t *data,
                             std::size_t size) override
            {
                return programmer_->write_memory(address, data, size);
            }

            Result<std::uint32_t> read_register(std::uint8_t index) override
            {
                return programmer_->read_register(index);
            }

            VoidResult write_register(std::uint8_t index, std::uint32_t value) override
            {
                return programmer_->write_register(index, value);
            }

            VoidResult reset_pin(bool asserted) override
            {
                return programmer_->reset_pin(asserted);
            }

            Result<std::uint32_t> target_voltage() override
            {
                return programmer_->target_voltage();
            }

            Result<std::vector<std::uint32_t>> clock_rates() override
            {
                return programmer_->clock_rates();
            }

            VoidResult set_clock(std::uint32_t hz) override
            {
                return programmer_->set_clock(hz);
            }

            Result<CoreRegisters> read_registers() override
            {
                return programmer_->read_registers();
            }

            Result<std::uint32_t> read_debug_register(std::uint32_t address) override
            {
                return programmer_->read_debug_register(address);
            }

            VoidResult write_debug_register(std::uint32_t address, std::uint32_t value) override
            {
                return programmer_->write_debug_register(address, value);
            }

            IFlash &flash() noexcept override
            {
                return flash_;
            }

            bool has_trace() const noexcept override
            {
                return has_trace_;
            }

            ITrace &trace() noexcept override
            {
                if (has_trace_)
                {
                    return trace_;
                }

                return unavailable_trace_;
            }

        private:
            /* programmer_ first: trace_ holds a reference to what it owns. */
            std::unique_ptr<IProgrammer> programmer_;
            DeviceInfo info_;
            std::string key_;
            UnsupportedFlash flash_;
            ProgrammerTrace trace_;
            UnavailableTrace unavailable_trace_;
            bool has_trace_ = false;
        };

        /** @brief Every attached probe this library has anything to say to. */
        std::vector<ProbeAddress> programmers_attached()
        {
            std::vector<ProbeAddress> kept;

            for (auto &probe : enumerate_probes(kVendorId))
            {
                /*
                 * The vendor id alone is not enough. ST ships a great deal
                 * more than programmers under it, and a virtual COM port
                 * answering to 0x0483 has no business in a list of probes.
                 */
                if (is_programmer(probe.vid, probe.pid))
                {
                    kept.push_back(std::move(probe));
                }
            }

            return kept;
        }
    } // namespace

    std::vector<BasicDeviceInfo> Discovery::enumerate()
    {
        std::vector<BasicDeviceInfo> found;

        for (const auto &probe : programmers_attached())
        {
            found.push_back(basic_from(probe));
        }

        return found;
    }

    std::vector<DeviceInfo> Discovery::find(const DeviceFilter &filter)
    {
        std::vector<DeviceInfo> found;

        for (const auto &probe : programmers_attached())
        {
            const BasicDeviceInfo basic = basic_from(probe);

            /* Refuse on what is free before paying to connect. */
            if (!accepts_probe(filter, basic))
            {
                continue;
            }

            auto opened = open_programmer(probe);

            if (!opened.ok())
            {
                STLINK_LOG_DBG("skipping %s: %s", basic.location.c_str(),
                               opened.error().describe().c_str());

                continue;
            }

            auto programmer = opened.value();

            auto chip = read_the_chip(*programmer, DeviceOptions{});

            if (!chip.ok())
            {
                STLINK_LOG_DBG("skipping %s: %s", basic.location.c_str(),
                               chip.error().describe().c_str());

                continue;
            }

            DeviceInfo info;

            info.core_info = basic;
            info.programmer = programmer_info_of(*programmer);
            info.chip = chip.value();

            if (filter.chip_id.has_value() && *filter.chip_id != info.chip.id)
            {
                continue;
            }

            found.push_back(std::move(info));

            /* Leave the target as we found it; this was only a look. */
            if (!programmer->exit_debug().ok())
            {
                STLINK_LOG_DBG("could not leave debug mode on %s", basic.location.c_str());
            }
        }

        return found;
    }

    Result<std::unique_ptr<IDevice>> IDevice::create(const BasicDeviceInfo &info,
                                                    const DeviceOptions &options)
    {
        const std::string key = key_of(info);

        if (!claim(key))
        {
            return Error(ErrorCode::Busy, "this programmer is already open");
        }

        ProbeAddress probe;

        probe.vid = info.vid;
        probe.pid = info.pid;
        probe.serial = info.serial;
        probe.location = info.location;

        auto opened = open_programmer(probe);

        if (!opened.ok())
        {
            release(key);

            return opened.error().wrap(ErrorCode::IO, "opening the device");
        }

        auto programmer = opened.value();

        auto chip = read_the_chip(*programmer, options);

        if (!chip.ok())
        {
            release(key);

            return chip.error();
        }

        DeviceInfo full;

        full.core_info = info;
        full.programmer = programmer_info_of(*programmer);
        full.chip = chip.value();

        const bool swo = chip_has_swo(full.chip.id);

        return std::unique_ptr<IDevice>(
            new Device(std::move(programmer), std::move(full), key, swo));
    }

    Result<std::unique_ptr<IDevice>> IDevice::create(const DeviceInfo &info,
                                                    const DeviceOptions &options)
    {
        /*
         * The chip on a full sheet was read when it was found, and a chip
         * does not become a different one in the meantime. It is read again
         * anyway: the sheet may be older than the board in front of us, and a
         * device that silently disagrees with the sheet it was opened from
         * would be a miserable thing to debug.
         */
        return create(info.core_info, options);
    }
} // namespace stlink
