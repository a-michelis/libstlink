/**
 * @file    chip_description.cpp
 * @brief   Reading a chip description file.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/chip_description.h>

#include <array>
#include <cerrno>
#include <cstdlib>

namespace stlink
{
    namespace
    {
        struct Named
        {
            const char *name;
            FlashFamily family;
        };

        constexpr std::array<Named, 15> kFamilies{{
            {"C0", FlashFamily::C0},
            {"C5", FlashFamily::C5},
            {"F0_F1_F3", FlashFamily::F0_F1_F3},
            {"F1_XL", FlashFamily::F1_XL},
            {"F2_F4", FlashFamily::F2_F4},
            {"F7", FlashFamily::F7},
            {"G0", FlashFamily::G0},
            {"G4", FlashFamily::G4},
            {"H5", FlashFamily::H5},
            {"H7", FlashFamily::H7},
            {"L0_L1", FlashFamily::L0_L1},
            {"L4", FlashFamily::L4},
            {"L5_U5", FlashFamily::L5_U5},
            {"WB_WL", FlashFamily::WB_WL},
            {"WB0", FlashFamily::WB0},
        }};

        bool is_space(char c) noexcept
        {
            return (c == ' ') || (c == '\t') || (c == '\r');
        }

        std::string_view trim(std::string_view text) noexcept
        {
            while (!text.empty() && is_space(text.front()))
            {
                text.remove_prefix(1);
            }

            while (!text.empty() && is_space(text.back()))
            {
                text.remove_suffix(1);
            }

            return text;
        }

        /** @brief The line with any comment removed. */
        std::string_view strip_comment(std::string_view line) noexcept
        {
            const std::size_t hash = line.find('#');

            return (hash == std::string_view::npos) ? line : line.substr(0, hash);
        }

        /**
         * @brief A hexadecimal or decimal number.
         *
         * The whole value must be consumed: "0x4z0" is an error rather than
         * four.
         */
        bool parse_number(std::string_view text, std::uint32_t &out) noexcept
        {
            if (text.empty())
            {
                return false;
            }

            const std::string owned(text);
            const char *begin = owned.c_str();
            char *end = nullptr;

            errno = 0;
            const unsigned long long value = std::strtoull(begin, &end, 0);

            if ((errno != 0) || (end == begin) || (*end != '\0'))
            {
                return false;
            }

            if (value > 0xFFFFFFFFull)
            {
                return false;
            }

            out = static_cast<std::uint32_t>(value);

            return true;
        }

        bool parse_flags(std::string_view text, ChipFlags &out) noexcept
        {
            out = ChipFlags::None;

            while (!text.empty())
            {
                const std::size_t end = text.find_first_of(" \t");
                const std::string_view word = trim(text.substr(0, end));

                if (word == "swo")
                {
                    out |= ChipFlags::Swo;
                }
                else if (word == "dualbank")
                {
                    out |= ChipFlags::DualBank;
                }
                else if (!word.empty())
                {
                    return false;
                }

                if (end == std::string_view::npos)
                {
                    break;
                }

                text = trim(text.substr(end));
            }

            return true;
        }
    } // namespace

    const char *to_string(FlashFamily family) noexcept
    {
        for (const Named &known : kFamilies)
        {
            if (known.family == family)
            {
                return known.name;
            }
        }

        return nullptr;
    }

    FlashFamily flash_family_from_string(std::string_view name) noexcept
    {
        for (const Named &known : kFamilies)
        {
            if (name == known.name)
            {
                return known.family;
            }
        }

        return FlashFamily::Unknown;
    }

    Result<ChipDescription> parse_chip_description(std::string_view text)
    {
        ChipDescription chip;

        bool seen_name = false;
        bool seen_chip_id = false;
        bool seen_family = false;

        while (!text.empty())
        {
            const std::size_t newline = text.find('\n');
            const std::string_view raw = text.substr(0, newline);

            text = (newline == std::string_view::npos) ? std::string_view{}
                                                       : text.substr(newline + 1);

            const std::string_view line = trim(strip_comment(raw));

            if (line.empty())
            {
                continue;
            }

            const std::size_t split = line.find_first_of(" \t");

            if (split == std::string_view::npos)
            {
                return Error(ErrorCode::InvalidArgument, "a chip description field has no value");
            }

            const std::string_view field = line.substr(0, split);
            const std::string_view value = trim(line.substr(split));

            if (value.empty())
            {
                return Error(ErrorCode::InvalidArgument, "a chip description field has no value");
            }

            /* Text fields. */
            if (field == "name")
            {
                chip.name = std::string(value);
                seen_name = true;
                continue;
            }

            if (field == "reference")
            {
                chip.reference = std::string(value);
                continue;
            }

            if (field == "family")
            {
                chip.family = flash_family_from_string(value);

                if (chip.family == FlashFamily::Unknown)
                {
                    return Error(ErrorCode::InvalidArgument,
                                 "a chip description names a flash family that does not exist");
                }

                seen_family = true;
                continue;
            }

            if (field == "flags")
            {
                if (!parse_flags(value, chip.flags))
                {
                    return Error(ErrorCode::InvalidArgument,
                                 "a chip description names a flag that does not exist");
                }

                continue;
            }

            /* Everything else is a number. */
            std::uint32_t number = 0;

            if (!parse_number(value, number))
            {
                return Error(ErrorCode::InvalidArgument,
                             "a chip description field is not a number");
            }

            if (field == "chip_id")
            {
                chip.chip_id = number;
                seen_chip_id = true;
            }
            else if (field == "flash.base")
            {
                chip.flash.base = number;
            }
            else if (field == "flash.size_reg")
            {
                chip.flash_size_reg = number;
            }
            else if (field == "flash.page_size")
            {
                chip.flash_page_size = number;
            }
            else if (field == "sram.base")
            {
                chip.sram.base = number;
            }
            else if (field == "sram.size")
            {
                chip.sram.size = number;
            }
            else if (field == "bootrom.base")
            {
                chip.bootrom.base = number;
            }
            else if (field == "bootrom.size")
            {
                chip.bootrom.size = number;
            }
            else if (field == "option.base")
            {
                chip.option_bytes.base = number;
            }
            else if (field == "option.size")
            {
                chip.option_bytes.size = number;
            }
            else if (field == "otp.base")
            {
                chip.otp.base = number;
            }
            else if (field == "otp.size")
            {
                chip.otp.size = number;
            }
            else
            {
                return Error(ErrorCode::InvalidArgument,
                             "a chip description names a field that does not exist");
            }
        }

        if (!seen_name || !seen_chip_id || !seen_family)
        {
            return Error(ErrorCode::InvalidArgument,
                         "a chip description is missing name, chip_id or family");
        }

        return chip;
    }
} // namespace stlink
