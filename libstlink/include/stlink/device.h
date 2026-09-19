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
     * @brief A programmer and the chip behind it.
     *
     * Read only, and a plain value: copy it, keep it, pass it around.
     */
    struct STLINK_API DeviceInfo
    {
        /** @brief The programmer, exactly as enumerate() would report it. */
        BasicDeviceInfo core_info;

        /** @brief The chip on the other side. */
        ChipInfo chip;
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
         */
        [[nodiscard]] static Result<std::unique_ptr<IDevice>> create(const DeviceInfo &info);

        /**
         * @brief Connect to a programmer found by enumerate().
         *
         * The chip is read on connection, since a basic sheet does not say
         * what it is.
         */
        [[nodiscard]] static Result<std::unique_ptr<IDevice>> create(const BasicDeviceInfo &info);

        /** @brief The programmer and chip this device was created from. */
        [[nodiscard]] virtual const DeviceInfo &info() const noexcept = 0;

        /* ---- Running and stopping ----------------------------------- */

        [[nodiscard]] virtual VoidResult halt() = 0;
        [[nodiscard]] virtual VoidResult run() = 0;
        [[nodiscard]] virtual VoidResult step() = 0;
        [[nodiscard]] virtual VoidResult reset() = 0;

        /** @brief Whether the chip is currently running. */
        [[nodiscard]] virtual Result<bool> is_running() = 0;

        /* ---- Memory -------------------------------------------------- */

        [[nodiscard]] virtual VoidResult read(std::uint32_t address,
                                               std::uint8_t *data, std::size_t size) = 0;

        [[nodiscard]] virtual VoidResult write(std::uint32_t address,
                                                const std::uint8_t *data, std::size_t size) = 0;

        /* ---- Registers ----------------------------------------------- */

        [[nodiscard]] virtual Result<std::uint32_t> read_register(std::uint8_t index) = 0;
        [[nodiscard]] virtual VoidResult write_register(std::uint8_t index, std::uint32_t value) = 0;

        /* ---- Flash --------------------------------------------------- */

        /**
         * @brief Flash, as this chip's family implements it.
         *
         * Owned by the device and valid for as long as it is.
         */
        [[nodiscard]] virtual IFlash &flash() noexcept = 0;

    protected:
        IDevice() = default;
    };
} // namespace stlink

#endif // STLINK_DEVICE_H
