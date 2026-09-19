/**
 * @file    stlink.h
 * @brief   Starting the library.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *     stlink::init();
 *
 *     for (const auto &found : stlink::Discovery::find()) { ... }
 *
 * Call init() once before anything else. What it sets up is torn down when
 * the process ends, so there is nothing to call afterwards.
 *
 * This is also where the C API will live, for callers that are not C++. It
 * will sit beside these declarations in its own extern "C" block, taking
 * opaque pointers and plain char arrays rather than the types here.
 */

#ifndef STLINK_STLINK_H
#define STLINK_STLINK_H

#include <string>

#include <stlink/export.h>

namespace stlink
{
    /**
     * @brief Start the library, reading the chip descriptions it was installed
     *        with.
     *
     * Nothing else in the library works until this has been called, and it is
     * called once: a second call does nothing. Changing what is on disk means
     * starting the program again.
     */
    STLINK_API void init();

    /**
     * @brief Start the library, reading chip descriptions from @p chips_dir
     *        and nowhere else.
     *
     * The installed descriptions are not read at all, so this directory is
     * the whole of what the library will recognise. That is how a chip can be
     * supported without rebuilding, as long as its flash already behaves like
     * a family the library implements.
     *
     * A file that cannot be read or does not parse is logged and skipped, so
     * one bad file does not stop the library from starting.
     */
    STLINK_API void init(const std::string &chips_dir);
} // namespace stlink

#endif // STLINK_STLINK_H
