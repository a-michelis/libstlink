/**
 * @file    paths.cpp
 * @brief   Where things are, on Linux and the BSDs.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/paths.h>

#include <filesystem>

#include <unistd.h>

namespace stlink
{
    std::string executable_directory()
    {
        std::error_code failed;
        const std::filesystem::path self = std::filesystem::read_symlink("/proc/self/exe", failed);

        if (failed)
        {
            return {};
        }

        return self.parent_path().string();
    }
} // namespace stlink
