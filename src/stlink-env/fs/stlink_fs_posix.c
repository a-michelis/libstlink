/**
  ******************************************************************************
  * @file           : stlink_fs_posix.c
  * @brief          : Filesystem lookups on Unix (POSIX) systems
  * @copyright      : Copyright (c) 2026 stlink-org. All rights reserved.
  * @date           : 2026-09-17
  * SPDX-License-Identifier: BSD-3-Clause
  *
  * This file is licensed under the BSD 3-Clause License.
  * See the LICENSE file in the project root for full license information.
  ******************************************************************************
  */

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <stlink_fs.h>

#include "logging.h"

static bool has_ext(const char *name, const char *ext) {
  size_t nl = strlen(name);
  size_t el = strlen(ext);

  return ((nl > el) && (strcmp(name + nl - el, ext) == 0));
}

bool stlink_exe_dir(char *buf, size_t len) {
  char *cut;

#if defined(__APPLE__)
  uint32_t n = (uint32_t)len;

  if(_NSGetExecutablePath(buf, &n) != 0) { return (false); }
#else
  ssize_t n = readlink("/proc/self/exe", buf, len - 1);

  if((n <= 0) || ((size_t)n >= (len - 1))) { return (false); }

  buf[n] = '\0';
#endif

  cut = strrchr(buf, '/');

  if(cut == NULL) { return (false); }

  *cut = '\0';

  return (true);
}

bool stlink_dir_has(const char *dir, const char *ext) {
  DIR *d = opendir(dir);
  struct dirent *entry;
  bool found = false;

  if(d == NULL) { return (false); }

  while ((entry = readdir(d)) != NULL) {
    if(has_ext(entry->d_name, ext)) {
      found = true;
      break;
    }
  }

  closedir(d);

  return (found);
}

void stlink_dir_foreach(const char *dir, const char *ext, void (*fn)(char *path)) {
  DIR *d = opendir(dir);
  struct dirent *entry;

  if(d == NULL) {
    perror(dir);
    return;
  }

  while ((entry = readdir(d)) != NULL) {
    char path[1024];

    if(!has_ext(entry->d_name, ext)) { continue; }

    if(snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name) >= (int)sizeof(path)) {
      ELOG("Path too long: %s/%s\n", dir, entry->d_name);
      continue;
    }

    fn(path);
  }

  closedir(d);
}
