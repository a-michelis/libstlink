/**
 * @file    chip_database.cpp
 * @brief   Every chip the library knows.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/chip_database.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <stlink/log.h>

namespace stlink
{
    ChipDatabase &ChipDatabase::instance()
    {
        static ChipDatabase database;

        return database;
    }

    const ChipDescription *ChipDatabase::find(std::uint32_t chip_id) const noexcept
    {
        for (const ChipDescription &chip : chips_)
        {
            if (chip.chip_id == chip_id)
            {
                return &chip;
            }
        }

        return nullptr;
    }

    void ChipDatabase::add(ChipDescription chip)
    {
        const auto existing = std::find_if(chips_.begin(), chips_.end(),
                                           [&chip](const ChipDescription &known)
                                           { return known.chip_id == chip.chip_id; });

        if (existing != chips_.end())
        {
            STLINK_LOG_DBG("replacing chip 0x%03x %s with %s", chip.chip_id,
                           existing->name.c_str(), chip.name.c_str());
            *existing = std::move(chip);

            return;
        }

        chips_.push_back(std::move(chip));
    }

    std::size_t ChipDatabase::add_directory(const std::string &directory)
    {
        std::error_code failed;

        if (!std::filesystem::is_directory(directory, failed))
        {
            STLINK_LOG_DBG("no chip descriptions read: %s is not a directory", directory.c_str());

            return 0;
        }

        std::size_t added = 0;

        for (const auto &entry : std::filesystem::directory_iterator(directory, failed))
        {
            if (!entry.is_regular_file() || (entry.path().extension() != ".chip"))
            {
                continue;
            }

            const std::string path = entry.path().string();
            std::ifstream file(entry.path());

            if (!file)
            {
                STLINK_LOG_WRN("skipping %s: it could not be opened", path.c_str());
                continue;
            }

            std::ostringstream text;
            text << file.rdbuf();

            auto parsed = parse_chip_description(text.str());

            if (!parsed.ok())
            {
                STLINK_LOG_WRN("skipping %s: %s", path.c_str(),
                               parsed.error().describe().c_str());
                continue;
            }

            add(parsed.value());
            ++added;
        }

        if (failed)
        {
            STLINK_LOG_WRN("chip descriptions in %s were only partly read: %s",
                           directory.c_str(), failed.message().c_str());
        }

        return added;
    }

    std::size_t ChipDatabase::size() const noexcept
    {
        return chips_.size();
    }

    const std::vector<ChipDescription> &ChipDatabase::all() const noexcept
    {
        return chips_;
    }
} // namespace stlink
