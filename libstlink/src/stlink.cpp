/**
 * @file    stlink.cpp
 * @brief   Starting the library.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/stlink.h>

#include <cstdlib>
#include <filesystem>

#include <stlink/chip_database.h>
#include <stlink/log.h>
#include <stlink/paths.h>

namespace stlink
{
    namespace
    {
        bool g_started = false;

        /** @brief The name of the variable that overrides where chips are read from. */
        constexpr const char *kChipsDirVariable = "STLINK_CHIPS_DIR";

        bool is_directory(const std::string &path)
        {
            if (path.empty())
            {
                return false;
            }

            std::error_code failed;

            return std::filesystem::is_directory(path, failed);
        }

        /**
         * @brief Where the chip descriptions are, looking in order at
         *        STLINK_CHIPS_DIR, the installed location beside the program,
         *        and a chips directory next to it.
         *
         * Empty when none of them is there.
         */
        std::string find_chips_dir()
        {
            if (const char *from_environment = std::getenv(kChipsDirVariable))
            {
                if (is_directory(from_environment))
                {
                    return from_environment;
                }

                STLINK_LOG_WRN("%s names %s, which is not a directory; looking elsewhere",
                               kChipsDirVariable, from_environment);
            }

            const std::string program = executable_directory();

            if (!program.empty())
            {
                const std::filesystem::path beside(program);

                const std::string installed =
                    (beside / ".." / "share" / "stlink" / "config" / "chips").lexically_normal().string();

                if (is_directory(installed))
                {
                    return installed;
                }

                const std::string alongside = (beside / "chips").lexically_normal().string();

                if (is_directory(alongside))
                {
                    return alongside;
                }
            }

            return {};
        }
    }

    void init()
    {
        const std::string chips_dir = find_chips_dir();

        if (chips_dir.empty())
        {
            STLINK_LOG_ERR("no chip descriptions found: set %s, or install them beside the "
                           "program in ../share/stlink/config/chips or ./chips",
                           kChipsDirVariable);

            g_started = true;

            return;
        }

        init(chips_dir);
    }

    void init(const std::string &chips_dir)
    {
        if (g_started)
        {
            return;
        }

        g_started = true;

        ChipDatabase &chips = ChipDatabase::instance();
        const std::size_t loaded = chips.add_directory(chips_dir);

        if (loaded == 0)
        {
            STLINK_LOG_WRN("no chip descriptions were read from %s; no chip will be recognised",
                           chips_dir.c_str());

            return;
        }

        STLINK_LOG_INF("libstlink started with %zu chip descriptions from %s",
                       loaded, chips_dir.c_str());
    }
} // namespace stlink
