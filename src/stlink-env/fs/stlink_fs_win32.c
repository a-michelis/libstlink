/**
  ******************************************************************************
  * @file           : stlink_fs_win32.c
  * @brief          : Filesystem lookups on Windows
  * @copyright      : Copyright (c) 2026 stlink-org. All rights reserved.
  * @date           : 2026-09-17
  * SPDX-License-Identifier: BSD-3-Clause
  *
  * This file is licensed under the BSD 3-Clause License.
  * See the LICENSE file in the project root for full license information.
  ******************************************************************************
  */

#include <windows.h>
#include <fileapi.h>
#include <strsafe.h>

#include <stlink_fs.h>

#include "logging.h"

/* Build "<dir>\*<ext>", the pattern FindFirstFile matches against. */
static bool make_pattern(char *out, size_t len, const char *dir, const char *ext) {
  return (!FAILED(StringCchCopyA(out, len, dir)) &&
          !FAILED(StringCchCatA(out, len, "\\*")) &&
          !FAILED(StringCchCatA(out, len, ext)));
}

bool stlink_exe_dir(char *buf, size_t len) {
  DWORD n = GetModuleFileNameA(NULL, buf, (DWORD)len);
  char *cut;
  char *back;

  if((n == 0) || (n >= len)) { return (false); }

  /* Either separator can appear, depending on how the process was started. */
  cut = strrchr(buf, '/');
  back = strrchr(buf, '\\');

  if((back != NULL) && ((cut == NULL) || (back > cut))) { cut = back; }

  if(cut == NULL) { return (false); }

  *cut = '\0';

  return (true);
}

bool stlink_dir_has(const char *dir, const char *ext) {
  WIN32_FIND_DATAA ffd;
  char pattern[MAX_PATH] = {0};
  HANDLE hFind;

  if(!make_pattern(pattern, sizeof(pattern), dir, ext)) { return (false); }

  hFind = FindFirstFileA(pattern, &ffd);

  if(INVALID_HANDLE_VALUE == hFind) { return (false); }

  FindClose(hFind);

  return (true);
}

void stlink_dir_foreach(const char *dir, const char *ext, void (*fn)(char *path)) {
  WIN32_FIND_DATAA ffd;
  char path[MAX_PATH] = {0};
  HANDLE hFind;

  if(!make_pattern(path, sizeof(path), dir, ext)) {
    ELOG("Path too long: %s\n", dir);
    return;
  }

  hFind = FindFirstFileA(path, &ffd);

  if(INVALID_HANDLE_VALUE == hFind) { return; }

  do {
    if(FAILED(StringCchCopyA(path, sizeof(path), dir)) ||
        FAILED(StringCchCatA(path, sizeof(path), "\\")) ||
        FAILED(StringCchCatA(path, sizeof(path), ffd.cFileName))) {
      ELOG("Path too long: %s\\%s\n", dir, ffd.cFileName);
      continue;
    }

    fn(path);
  } while (FindNextFileA(hFind, &ffd) != 0);

  FindClose(hFind);
}
