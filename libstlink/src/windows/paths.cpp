/**
 * @file    paths.cpp
 * @brief   Where things are, on Windows.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/paths.h>

#include <filesystem>
#include <vector>

#include <windows.h>

namespace stlink
{
    std::string executable_directory()
    {
        std::vector<char> path(MAX_PATH);

        for (;;)
        {
            const DWORD written = GetModuleFileNameA(nullptr, path.data(),
                                                     static_cast<DWORD>(path.size()));

            if (written == 0)
            {
                return {};
            }

            /* Truncated: the buffer was filled exactly and there is more. */
            if (written < path.size())
            {
                return std::filesystem::path(path.data()).parent_path().string();
            }

            path.resize(path.size() * 2);
        }
    }
} // namespace stlink
