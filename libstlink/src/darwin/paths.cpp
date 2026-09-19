/**
 * @file    paths.cpp
 * @brief   Where things are, on macOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/paths.h>

#include <filesystem>
#include <vector>

#include <mach-o/dyld.h>

namespace stlink
{
    std::string executable_directory()
    {
        std::uint32_t size = 0;

        /* Asks how much room the path needs; fails because zero is not enough. */
        _NSGetExecutablePath(nullptr, &size);

        std::vector<char> path(size + 1, '\0');

        if (_NSGetExecutablePath(path.data(), &size) != 0)
        {
            return {};
        }

        std::error_code failed;
        const std::filesystem::path self =
            std::filesystem::canonical(std::filesystem::path(path.data()), failed);

        if (failed)
        {
            return std::filesystem::path(path.data()).parent_path().string();
        }

        return self.parent_path().string();
    }
} // namespace stlink
