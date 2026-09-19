/**
 * @file    paths.h
 * @brief   Where things are on this machine.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Internal. One implementation per operating system, in src/${OS}/.
 */

#ifndef STLINK_PV_PATHS_H
#define STLINK_PV_PATHS_H

#include <string>

namespace stlink
{
    /**
     * @brief The directory holding the running executable.
     *
     * Empty when the operating system would not say, which is not expected
     * and leaves the caller to fall back on something else.
     */
    [[nodiscard]] std::string executable_directory();
} // namespace stlink

#endif // STLINK_PV_PATHS_H
