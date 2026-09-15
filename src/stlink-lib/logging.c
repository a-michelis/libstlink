/**
  ******************************************************************************
  * @file           : logging.c
  * @brief          : UglyLogging logging "framework"
  * @copyright      : Copyright (c) 2026 stlink-org. All rights reserved.
  * @date           : 2026-07-27
  * SPDX-License-Identifier: BSD-3-Clause
  *
  * This file is licensed under the BSD 3-Clause License.
  * See the LICENSE file in the project root for full license information.
  ******************************************************************************
  */

#define __STDC_WANT_LIB_EXT1__ 1

#include "logging.h"


static int32_t max_level = UDEBUG;

int32_t ugly_init(int32_t maximum_threshold) {
  max_level = maximum_threshold;
  return (0);
}

int32_t ugly_log(int32_t level, const char *tag, const char *format, ...) {
  if(level > max_level) {
    return (0);
  }

  fflush(stdout); // flush to maintain order of streams

  va_list args;
  va_start(args, format);
  time_t mytt = time(NULL);

  struct tm *ptt;
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L) // C11
  struct tm tt;
  ptt = &tt;
# if defined (_WIN32) || defined(__STDC_LIB_EXT1__)
  localtime_s(&tt, &mytt);
# else
  localtime_r(&mytt, &tt);
# endif
#else
  ptt = localtime(&mytt);
#endif

  fprintf(stderr, "%d-%02d-%02dT%02d:%02d:%02d ", ptt->tm_year + 1900,
          ptt->tm_mon + 1, ptt->tm_mday, ptt->tm_hour, ptt->tm_min, ptt->tm_sec);

  switch (level) {
  case UDEBUG:
    fprintf(stderr, "DEBUG %s: ", tag);
    break;
  case UINFO:
    fprintf(stderr, "INFO %s: ", tag);
    break;
  case UWARN:
    fprintf(stderr, "WARN %s: ", tag);
    break;
  case UERROR:
    fprintf(stderr, "ERROR %s: ", tag);
    break;
  default:
    fprintf(stderr, "%d %s: ", level, tag);
    break;
  }

  vfprintf(stderr, format, args);
  fflush(stderr);
  va_end(args);
  return (1);
}
