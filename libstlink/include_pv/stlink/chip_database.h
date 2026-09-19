/**
 * @file    chip_database.h
 * @brief   Every chip the library knows.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. Filled by init() from the chip description files it reads.
 *
 * Read only once init() has returned. Nothing here is synchronised, because
 * everything that writes to it happens during init() and everything after
 * that only reads.
 */

#ifndef STLINK_PV_CHIP_DATABASE_H
#define STLINK_PV_CHIP_DATABASE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <stlink/chip_description.h>

namespace stlink
{
    class ChipDatabase
    {
    public:
        /** @brief The library's one database. */
        static ChipDatabase &instance();

        /**
         * @brief The chip with this identifier, or nullptr when there is none.
         *
         * The pointer stays valid for as long as the process runs.
         */
        [[nodiscard]] const ChipDescription *find(std::uint32_t chip_id) const noexcept;

        /**
         * @brief Add a description, replacing any with the same identifier.
         *
         * Later wins, which is what lets a file override a built-in chip.
         */
        void add(ChipDescription chip);

        /**
         * @brief Read every .chip file in @p directory and add what parses.
         *
         * A file that cannot be read or does not parse is logged and skipped.
         * A directory that is not there is not an error.
         *
         * @return how many descriptions were added
         */
        std::size_t add_directory(const std::string &directory);

        [[nodiscard]] std::size_t size() const noexcept;

        /** @brief Every description, for tools that list what is supported. */
        [[nodiscard]] const std::vector<ChipDescription> &all() const noexcept;

    private:
        ChipDatabase() = default;

        std::vector<ChipDescription> chips_;
    };
} // namespace stlink

#endif // STLINK_PV_CHIP_DATABASE_H
