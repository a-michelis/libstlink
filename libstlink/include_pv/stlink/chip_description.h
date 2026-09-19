/**
 * @file    chip_description.h
 * @brief   What a chip description file says.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. This is the whole of a .chip file, including the parts a caller
 * never sees: which flash mechanism drives the chip, where it keeps its own
 * flash size, and its quirks. The public ChipInfo is what is left once the
 * chip has been read.
 *
 * See libstlink/resources/chips/README.md for the file format.
 */

#ifndef STLINK_PV_CHIP_DESCRIPTION_H
#define STLINK_PV_CHIP_DESCRIPTION_H

#include <cstdint>
#include <string>
#include <string_view>

#include <stlink/device.h>
#include <stlink/result.h>

namespace stlink
{
    /**
     * @brief Which flash mechanism a chip uses.
     *
     * Not a chip series: several series share one, and this is what decides
     * which implementation drives the flash controller.
     */
    enum class FlashFamily : std::uint8_t
    {
        Unknown = 0,
        C0,
        C5,
        F0_F1_F3,
        F1_XL,
        F2_F4,
        F7,
        G0,
        G4,
        H5,
        H7,
        L0_L1,
        L4,
        L5_U5,
        WB_WL,
        WB0,
    };

    /** @brief The name a .chip file uses, or nullptr for Unknown. */
    const char *to_string(FlashFamily family) noexcept;

    /** @brief The family a .chip file names, or Unknown when it names none of them. */
    FlashFamily flash_family_from_string(std::string_view name) noexcept;

    /** @brief A chip's quirks, as the file's flags line lists them. */
    enum class ChipFlags : std::uint32_t
    {
        None = 0,
        Swo = 1u << 0,      /**< Has a trace output pin. */
        DualBank = 1u << 1, /**< Flash is in two independently erasable banks. */
    };

    constexpr ChipFlags operator|(ChipFlags a, ChipFlags b) noexcept
    {
        return static_cast<ChipFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
    }

    constexpr ChipFlags &operator|=(ChipFlags &a, ChipFlags b) noexcept
    {
        a = a | b;
        return a;
    }

    /** @brief True when @p set contains @p one. */
    constexpr bool has_flag(ChipFlags set, ChipFlags one) noexcept
    {
        return (static_cast<std::uint32_t>(set) & static_cast<std::uint32_t>(one)) != 0;
    }

    /** @brief One chip, exactly as its file describes it. */
    struct ChipDescription
    {
        std::string name;
        std::string reference;
        std::uint32_t chip_id = 0;
        FlashFamily family = FlashFamily::Unknown;

        MemoryRegion flash;
        /**
         * @brief Where the chip reports its own flash size.
         *
         * The size is read from the chip rather than taken from the file,
         * because one part number ships in several sizes. flash.size is
         * therefore zero here and filled in once a device is connected.
         */
        std::uint32_t flash_size_reg = 0;
        std::uint32_t flash_page_size = 0;

        MemoryRegion sram;
        MemoryRegion bootrom;
        MemoryRegion option_bytes;
        MemoryRegion otp;

        ChipFlags flags = ChipFlags::None;
    };

    /**
     * @brief Read one chip description.
     *
     * Takes the file's text rather than its path, so that the same parser
     * serves the descriptions compiled into the library and any loaded from
     * disk.
     *
     * An unknown field name is an error rather than something to skip, so a
     * typo is reported instead of quietly leaving a field at zero.
     */
    [[nodiscard]] Result<ChipDescription> parse_chip_description(std::string_view text);
} // namespace stlink

#endif // STLINK_PV_CHIP_DESCRIPTION_H
