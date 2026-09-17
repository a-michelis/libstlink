/**
  ******************************************************************************
  * @file           : stlink_fs.h
  * @brief          : Filesystem lookups that differ per platform
  * @copyright      : Copyright (c) 2026 stlink-org. All rights reserved.
  * @date           : 2026-09-17
  * SPDX-License-Identifier: BSD-3-Clause
  *
  * This file is licensed under the BSD 3-Clause License.
  * See the LICENSE file in the project root for full license information.
  ******************************************************************************
  */

#ifndef STLINK_FS_H
#define STLINK_FS_H

#include <stdbool.h>
#include <stddef.h>

/*
 * The three lookups that have no portable spelling: where the running
 * executable lives, and walking a directory for files with a given extension.
 * One implementation per platform, picked in cmake rather than with
 * preprocessor branches at the call site.
 */

/**
 * @brief Directory holding the running executable, without a trailing separator
 * @return false where the platform will not say, leaving buf untouched
 */
bool stlink_exe_dir(char *buf, size_t len);

/**
 * @brief Whether a directory holds at least one file with this extension
 * @param ext extension to match, including the dot
 */
bool stlink_dir_has(const char *dir, const char *ext);

/**
 * @brief Call fn once per file in dir carrying this extension
 * @param ext extension to match, including the dot
 */
void stlink_dir_foreach(const char *dir, const char *ext, void (*fn)(char *path));

#endif // STLINK_FS_H
