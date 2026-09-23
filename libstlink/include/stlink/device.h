/**
 * @file    device.h
 * @brief   An STM32, reached through an ST-LINK.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This is the whole public surface. How bytes reach the programmer, what
 * commands the programmer speaks, and what a given family's flash needs, are
 * the library's business.
 *
 *     auto found = Discovery::find({.chip_id = 0x413});
 *     if (found.empty()) { return; }
 *
 *     auto opened = IDevice::create(found.front());
 *     if (!opened.ok()) { STLINK_LOG_ERR("%s", opened.error().describe().c_str()); return; }
 *
 *     auto device = opened.value();
 *     auto status = device->halt();
 *
 * Two ways to look. enumerate() lists the attached programmers and touches no
 * target, so it disturbs nothing that is running. find() goes further and
 * connects through to the chip, which is what lets it filter on what the chip
 * is, and what lets create() go straight to the right implementation.
 */

#ifndef STLINK_DEVICE_H
#define STLINK_DEVICE_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <stlink/export.h>
#include <stlink/result.h>

namespace stlink
{
    /** @brief Which ST-LINK generation a programmer is. */
    enum class ProgrammerKind : std::uint8_t
    {
        Unknown = 0,
        V1,   /**< Framed as mass storage. */
        V2,
        V2_1, /**< V2 on a Nucleo or Discovery board. */
        V3,
    };

    STLINK_API const char *to_string(ProgrammerKind kind) noexcept;

    /**
     * @brief A span of the chip's address space.
     *
     * A size of zero means the chip does not have this region.
     */
    struct STLINK_API MemoryRegion
    {
        std::uint32_t base = 0;
        std::uint32_t size = 0;
    };

    /**
     * @brief What the chip is, and what it has.
     *
     * The sizes here were read from the chip rather than assumed: where a
     * family stores its flash size, and how its flash is driven, stay inside
     * the library.
     */
    struct STLINK_API ChipInfo
    {
        std::uint32_t id = 0;   /**< As the chip reported it. */
        std::string name;       /**< Empty when the chip is not in the database. */
        std::string reference;  /**< The reference manual that documents it. */

        MemoryRegion flash;
        std::uint32_t flash_page_size = 0;

        MemoryRegion sram;
        MemoryRegion bootrom;
        MemoryRegion option_bytes;
        MemoryRegion otp;
    };

    /**
     * @brief One attached programmer, known without touching the target.
     *
     * Everything here comes from the connection to the programmer itself, so
     * listing these disturbs nothing that is running.
     */
    struct STLINK_API BasicDeviceInfo
    {
        ProgrammerKind kind = ProgrammerKind::Unknown;

        /** @brief Empty when the programmer did not report one. */
        std::string serial;

        std::uint16_t vid = 0;
        std::uint16_t pid = 0;

        /**
         * @brief Where it is attached, for diagnostics.
         *
         * Not stable across a replug, and never to be parsed.
         */
        std::string location;
    };

    /**
     * @brief What the programmer reports about itself once it has been asked.
     *
     * Not in BasicDeviceInfo, because enumerate() never asks: it reads what
     * USB already knows and opens nothing. These come from the version
     * exchange, so they are known only once a connection has been made.
     */
    struct STLINK_API ProgrammerInfo
    {
        /** @brief The debug engine's firmware revision, the J in "J41". */
        std::uint8_t debug_firmware = 0;

        /** @brief The SWIM engine's revision, or zero when it has none. */
        std::uint8_t swim_firmware = 0;

        /**
         * @brief The fastest trace this programmer will carry, in hertz.
         *
         * Zero when it carries none at all, which is every ST-LINK/V1 at any
         * revision and a V2 below a certain firmware. See IDevice::has_trace().
         */
        std::uint32_t max_trace_frequency = 0;
    };

    /**
     * @brief A programmer and the chip behind it.
     *
     * Read only, and a plain value: copy it, keep it, pass it around.
     */
    struct STLINK_API DeviceInfo
    {
        /** @brief The programmer, exactly as enumerate() would report it. */
        BasicDeviceInfo core_info;

        /** @brief What the programmer said about itself when it was asked. */
        ProgrammerInfo programmer;

        /** @brief The chip on the other side. */
        ChipInfo chip;
    };

    /** @brief Which wire protocol to reach the target over. */
    enum class DebugMode : std::uint8_t
    {
        Swd,
        Jtag, /**< Not implemented over USB by any firmware; refused. */
    };

    /** @brief How the target is held while the debug connection is made. */
    enum class ResetMode : std::uint8_t
    {
        Normal,     /**< Connect to a running target. */
        UnderReset, /**< Hold nRST low while connecting. */
        HotPlug,    /**< Attach without resetting anything. */
    };

    /**
     * @brief How to connect. Every field has a usable default.
     *
     * UnderReset is the thing to reach for when a target will not answer.
     * Firmware that disables the debug unit early, or puts the core to sleep,
     * leaves nothing to connect to by the time an ordinary attach happens;
     * holding nRST low while connecting gets in first.
     */
    struct STLINK_API DeviceOptions
    {
        DebugMode debug = DebugMode::Swd;
        ResetMode reset = ResetMode::Normal;

        /**
         * @brief Debug clock in hertz, or zero for this generation's default.
         *
         * Must be one of the rates the programmer offers; a rate that is not
         * on offer is refused rather than rounded, because rounding up
         * overclocks a connection that may not carry it and rounding down
         * turns a request into a ceiling without saying so. Zero asks for a
         * conservative default rather than the fastest available.
         */
        std::uint32_t clock_hz = 0;
    };

    /** @brief The Cortex-M core registers, as the programmer returns them. */
    struct STLINK_API CoreRegisters
    {
        std::uint32_t r[16] = {};
        std::uint32_t xpsr = 0;
        std::uint32_t main_sp = 0;
        std::uint32_t process_sp = 0;
        std::uint32_t rw = 0;
        std::uint32_t rw2 = 0;
    };

    /** @brief Which devices to report. Everything is optional. */
    struct STLINK_API DeviceFilter
    {
        /** @brief Only the programmer with this serial. */
        std::optional<std::string> serial;

        /** @brief Only programmers of this generation. */
        std::optional<ProgrammerKind> kind;

        /** @brief Only devices whose chip identifies as this. */
        std::optional<std::uint32_t> chip_id;

        /** @brief Accepts everything. */
        static const DeviceFilter NoFilter;
    };

    /** @brief Finding what is attached. */
    class STLINK_API Discovery
    {
    public:
        /**
         * @brief Every attached programmer, without touching any target.
         *
         * Cheap, and safe to call while other targets are running. There is
         * no filter because the only thing to match on is the serial, which a
         * caller can do itself.
         */
        [[nodiscard]] static std::vector<BasicDeviceInfo> enumerate();

        /**
         * @brief Every attached device the filter accepts, chip included.
         *
         * Connects to each programmer and through it to the chip, which is
         * what makes filtering on the chip possible. Use this when the intent
         * is to open something; use enumerate() to merely look.
         *
         * An empty result means none matched, which is not an error.
         */
        [[nodiscard]] static std::vector<DeviceInfo> find(const DeviceFilter &filter = DeviceFilter::NoFilter);
    };

    class IFlash;
    class ITrace;

    /**
     * @brief An STM32, and the programmer reaching it.
     *
     * One instance belongs to one thread at a time; nothing here is
     * internally synchronised. A device is connected when it is created and
     * disconnected when it is destroyed.
     */
    class STLINK_API IDevice
    {
    public:
        virtual ~IDevice() = default;

        IDevice(const IDevice &) = delete;
        IDevice &operator=(const IDevice &) = delete;

        /**
         * @brief Connect to the device described by @p info.
         *
         * One device per programmer: creating a second for a programmer that
         * is already open fails rather than returning the first.
         *
         * @param info    the device to connect to
         * @param options how to connect; the default is SWD without resetting
         * @return the connected device, or why it could not be
         */
        [[nodiscard]] static Result<std::unique_ptr<IDevice>> create(
            const DeviceInfo &info, const DeviceOptions &options = {});

        /**
         * @brief Connect to a programmer found by enumerate().
         *
         * The chip is read on connection, since a basic sheet does not say
         * what it is.
         *
         * @param info    the programmer to connect through
         * @param options how to connect; the default is SWD without resetting
         * @return the connected device, or why it could not be
         */
        [[nodiscard]] static Result<std::unique_ptr<IDevice>> create(
            const BasicDeviceInfo &info, const DeviceOptions &options = {});

        /** @brief The programmer and chip this device was created from. */
        [[nodiscard]] virtual const DeviceInfo &info() const noexcept = 0;

        /* ---- Running and stopping ----------------------------------- */

        [[nodiscard]] virtual VoidResult halt() = 0;
        [[nodiscard]] virtual VoidResult run() = 0;
        [[nodiscard]] virtual VoidResult step() = 0;
        [[nodiscard]] virtual VoidResult reset() = 0;

        /** @brief Whether the chip is currently running. */
        [[nodiscard]] virtual Result<bool> is_running() = 0;

        /**
         * @brief Drive the target's reset pin directly.
         *
         * Not the same as reset(), which goes through the debug unit. This is
         * the physical line, and holding it asserted is how a board is kept
         * in reset while something else is arranged.
         */
        [[nodiscard]] virtual VoidResult reset_pin(bool asserted) = 0;

        /* ---- The programmer ------------------------------------------ */

        /** @brief Voltage on the target's reference pin, in millivolts. */
        [[nodiscard]] virtual Result<std::uint32_t> target_voltage() = 0;

        /**
         * @brief The debug clock rates this programmer will accept, in hertz.
         *
         * Empty when the programmer cannot be told a rate at all.
         */
        [[nodiscard]] virtual Result<std::vector<std::uint32_t>> clock_rates() = 0;

        /**
         * @brief Set the debug clock to exactly @p hz.
         *
         * One of the rates clock_rates() reported and nothing else. Zero asks
         * for this generation's conservative default. See DeviceOptions for
         * why a rate that is not on offer is refused rather than rounded.
         */
        [[nodiscard]] virtual VoidResult set_clock(std::uint32_t hz) = 0;

        /* ---- Memory -------------------------------------------------- */

        [[nodiscard]] virtual VoidResult read(std::uint32_t address,
                                               std::uint8_t *data, std::size_t size) = 0;

        [[nodiscard]] virtual VoidResult write(std::uint32_t address,
                                                const std::uint8_t *data, std::size_t size) = 0;

        /* ---- Registers ----------------------------------------------- */

        [[nodiscard]] virtual Result<std::uint32_t> read_register(std::uint8_t index) = 0;
        [[nodiscard]] virtual VoidResult write_register(std::uint8_t index, std::uint32_t value) = 0;

        /**
         * @brief Every core register in one transfer.
         *
         * The programmer returns the whole file at once, so this costs one
         * round trip where reading all of them singly costs twenty one. That
         * is what a debugger wants after a step or a halt.
         */
        [[nodiscard]] virtual Result<CoreRegisters> read_registers() = 0;

        /* ---- The debug unit ------------------------------------------ */

        /**
         * @brief Read a debug unit register.
         *
         * Not target memory: these reach the debug port itself, and are how
         * the core is halted, stepped and inspected underneath the calls
         * above. Reaching for them means stepping outside what this library
         * promises about the chip's state, so prefer the calls above where
         * they do the job.
         */
        [[nodiscard]] virtual Result<std::uint32_t> read_debug_register(std::uint32_t address) = 0;

        [[nodiscard]] virtual VoidResult write_debug_register(std::uint32_t address,
                                                               std::uint32_t value) = 0;

        /* ---- Flash --------------------------------------------------- */

        /**
         * @brief Flash, as this chip's family implements it.
         *
         * Owned by the device and valid for as long as it is.
         */
        [[nodiscard]] virtual IFlash &flash() noexcept = 0;

        /* ---- Trace --------------------------------------------------- */

        /**
         * @brief Whether trace output can be delivered at all.
         *
         * Both ends have to have it. The programmer must carry trace, which
         * no ST-LINK/V1 does at any revision and a V2 does only from a
         * certain firmware, and the chip must actually have the SWO pin,
         * which a fair number do not.
         *
         * Askable up front so a caller can grey out a log window rather than
         * discover the fact through a NotSupported failure.
         */
        [[nodiscard]] virtual bool has_trace() const noexcept = 0;

        /**
         * @brief Trace output, owned by the device and valid as long as it is.
         *
         * When has_trace() is false every operation on it refuses.
         */
        [[nodiscard]] virtual ITrace &trace() noexcept = 0;

    protected:
        IDevice() = default;
    };
} // namespace stlink

#endif // STLINK_DEVICE_H
