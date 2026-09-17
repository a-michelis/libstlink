/**
  ******************************************************************************
  * @file           : chipid.c
  * @brief          : Chip-ID parametres
  * @copyright      : Copyright (c) 2026 stlink-org. All rights reserved.
  * @date           : 2026-07-27
  * SPDX-License-Identifier: BSD-3-Clause
  *
  * This file is licensed under the BSD 3-Clause License.
  * See the LICENSE file in the project root for full license information.
  ******************************************************************************
  */

#include "chipid.h"

#include <stlink_fs.h>

#include "logging.h"


static struct stlink_chipid_params *devicelist;

void dump_a_chip(struct stlink_chipid_params *dev) {
  DLOG("# Device Type: %s\n", dev->dev_type);
  DLOG("# Reference Manual: RM%s\n", dev->ref_manual_id);
  DLOG("#\n");
  DLOG("chip_id 0x%x\n", dev->chip_id);
  DLOG("flash_type %d\n", dev->flash_type);
  DLOG("flash_size_reg 0x%x\n", dev->flash_size_reg);
  DLOG("flash_pagesize 0x%x\n", dev->flash_pagesize);
  DLOG("sram_size 0x%x\n", dev->sram_size);
  DLOG("bootrom_base 0x%x\n", dev->bootrom_base);
  DLOG("bootrom_size 0x%x\n", dev->bootrom_size);
  DLOG("option_base 0x%x\n", dev->option_base);
  DLOG("option_size 0x%x\n", dev->option_size);
  DLOG("flags %d\n\n", dev->flags);
  DLOG("otp_base %d\n\n", dev->otp_base);
  DLOG("otp_size %d\n\n", dev->otp_size);
}

struct stlink_chipid_params *stlink_chipid_get_params(uint32_t chip_id) {
  struct stlink_chipid_params *params = NULL;
  for(params = devicelist; params != NULL; params = params->next)
    if(params->chip_id == chip_id) {
      DLOG("detected chip_id parameters\n\n");
      dump_a_chip(params);
      break;
    }

  return (params);
}

void process_chipfile(char *fname) {
  FILE *fp;
  char *p, buf[256];
  char word[64], value[64];
  struct stlink_chipid_params *ts;
  int32_t nc;

  // fprintf (stderr, "processing chip-id file %s.\n", fname);
  fp = fopen(fname, "r");

  if(!fp) {
    perror(fname);
    return;
  }

  ts = calloc(1, sizeof(struct stlink_chipid_params));

  while (fgets(buf, sizeof(buf), fp) != NULL) {

    if(strncmp(buf, "#", strlen("#")) == 0)
      continue; // ignore comments

    if((strncmp(buf, "\n", strlen("\n")) == 0) ||
        (strncmp(buf, " ", strlen(" ")) == 0))
      continue; // ignore empty lines

    if(sscanf(buf, "%63s %63s", word, value) != 2) {
      fprintf(stderr, "Failed to read keyword or value\n");
      continue;
    }

    if(strcmp(word, "dev_type") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      ts->dev_type = strdup(buf + nc);
    } else if(strcmp(word, "ref_manual_id") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      ts->ref_manual_id = strdup(buf + nc);
    } else if(strcmp(word, "chip_id") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->chip_id) < 1) {
        fprintf(stderr, "Failed to parse chip-id\n");
      }
    } else if(strcmp(word, "flash_type") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      // Match human readable flash_type with enum stm32_flash_type { }.
      if(strcmp(value, "C0") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_C0;
      } else if(strcmp(value, "F0_F1_F3") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_F0_F1_F3;
      } else if(strcmp(value, "F1_XL") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_F1_XL;
      } else if(strcmp(value, "F2_F4") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_F2_F4;
      } else if(strcmp(value, "F7") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_F7;
      } else if(strcmp(value, "G0") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_G0;
      } else if(strcmp(value, "G4") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_G4;
      } else if(strcmp(value, "H7") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_H7;
      } else if(strcmp(value, "L0_L1") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_L0_L1;
      } else if(strcmp(value, "L4") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_L4;
      } else if(strcmp(value, "L5_U5") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_L5_U5;
      } else if(strcmp(value, "WB_WL") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_WB_WL;
      } else if(strcmp(value, "WB0") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_WB0;
      } else if(strcmp(value, "H5") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_H5;
      } else if(strcmp(value, "C5") == 0) {
        ts->flash_type = STM32_FLASH_TYPE_C5;
      } else {
        ts->flash_type = STM32_FLASH_TYPE_UNKNOWN;
      }
    } else if(strcmp(word, "flash_size_reg") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->flash_size_reg) < 1) {
        fprintf(stderr, "Failed to parse flash size reg\n");
      }
    } else if(strcmp(word, "flash_pagesize") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->flash_pagesize) < 1) {
        fprintf(stderr, "Failed to parse flash page size\n");
      }
    } else if(strcmp(word, "sram_size") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->sram_size) < 1) {
        fprintf(stderr, "Failed to parse SRAM size\n");
      }
    } else if(strcmp(word, "bootrom_base") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->bootrom_base) < 1) {
        fprintf(stderr, "Failed to parse BootROM base\n");
      }
    } else if(strcmp(word, "bootrom_size") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->bootrom_size) < 1) {
        fprintf(stderr, "Failed to parse BootROM size\n");
      }
    } else if(strcmp(word, "option_base") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->option_base) < 1) {
        fprintf(stderr, "Failed to parse option base\n");
      }
    } else if(strcmp(word, "option_size") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->option_size) < 1) {
        fprintf(stderr, "Failed to parse option size\n");
      }
    } else if(strcmp(word, "flags") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      p = strtok(buf, " \t\n");

      while ((p = strtok(NULL, " \t\n"))) {
        if(strcmp(p, "none") == 0) {
          // NOP
        } else if(strcmp(p, "dualbank") == 0) {
          ts->flags |= CHIP_F_HAS_DUAL_BANK;
        } else if(strcmp(p, "swo") == 0) {
          ts->flags |= CHIP_F_HAS_SWO_TRACING;
        } else {
          fprintf(stderr, "Unknown flags word in %s: '%s'\n", fname, p);
        }
      }

      sscanf(value, "%x", &ts->flags);
    } else if(strcmp(word, "otp_base") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->otp_base) < 1) {
        fprintf(stderr, "Failed to parse option size\n");
      }
    } else if(strcmp(word, "otp_size") == 0) {
      buf[strlen(buf) - 1] = 0; // chomp newline
      sscanf(buf, "%*s %n", &nc);
      if(sscanf(value, "%i", &ts->otp_size) < 1) {
        fprintf(stderr, "Failed to parse option size\n");
      }
    } else {
      fprintf(stderr, "Unknown keyword in %s: %s\n", fname, word);
    }
  }
  fclose(fp);
  ts->next = devicelist;
  devicelist = ts;
}


/* == Locating the chip description files == */

/*
 * Resolved at run time rather than fixed at build time, so that an unpacked
 * archive, or a tree moved after it was installed, still finds its own files.
 * Candidates are tried in order:
 *
 *   0. a directory named by the caller, which is exclusive: nothing below is
 *      tried, and finding nothing there is an error
 *   1. the tree this was built from, in a debug build only
 *   2. STLINK_CHIPS_DIR in the environment
 *   3. beside the executable, in the installed layout
 *   4. beside the executable, in a flat archive
 *
 * Named subdirectories are used rather than the executable's own directory,
 * so that looking beside the executable costs a couple of lookups that fail
 * cheaply, instead of scanning something like /usr/bin on every start.
 */

#define CHIP_FILE_EXT ".chip"

/* Join base and tail, and report whether the result holds chip files. */
static bool candidate_has_chips(char *out, size_t len, const char *base, const char *tail) {
  int written = snprintf(out, len, "%s/%s", base, tail);

  if((written < 0) || ((size_t)written >= len)) { return (false); }

  DLOG("Looking for chip description files in %s\n", out);

  return (stlink_dir_has(out, CHIP_FILE_EXT));
}

void init_chipids(char *dir_to_scan) {
  char exe[1024];
  char path[1024];
  const char *chosen = NULL;
  const char *from_env = getenv("STLINK_CHIPS_DIR");

  devicelist = NULL;

  /*
   * A directory named by the caller is exclusive rather than preferred: they
   * are pointing at something particular, very likely to test it, so reading
   * some other set of chip files instead would hide their mistake rather than
   * work around it. Nothing else is tried, and an empty directory is an error.
   */
  if((dir_to_scan != NULL) && (*dir_to_scan != '\0')) {
    if(!stlink_dir_has(dir_to_scan, CHIP_FILE_EXT)) {
      ELOG("No chip description file in %s\n", dir_to_scan);
      return;
    }

    ILOG("Reading chip description files from %s\n", dir_to_scan);
    stlink_dir_foreach(dir_to_scan, CHIP_FILE_EXT, process_chipfile);

    return;
  }

#ifdef STLINK_CHIPS_SRC_DIR
  /*
   * A debug build goes straight to the tree it was built from: it is for
   * working on the code rather than for installing, so the files it should be
   * reading are the ones in the checkout. The macro is only defined for that
   * configuration, so a release build carries no source path at all.
   */
  DLOG("Looking for chip description files in %s\n", STLINK_CHIPS_SRC_DIR);

  if(stlink_dir_has(STLINK_CHIPS_SRC_DIR, CHIP_FILE_EXT)) { chosen = STLINK_CHIPS_SRC_DIR; }
#endif

  if((chosen == NULL) && (from_env != NULL) && (*from_env != '\0')) {
    DLOG("Looking for chip description files in %s\n", from_env);

    if(stlink_dir_has(from_env, CHIP_FILE_EXT)) { chosen = from_env; }
  }

  if((chosen == NULL) && stlink_exe_dir(exe, sizeof(exe))) {
    if(candidate_has_chips(path, sizeof(path), exe, "../share/stlink/config/chips")) {
      chosen = path;
    } else if(candidate_has_chips(path, sizeof(path), exe, "chips")) {
      chosen = path;
    }
  }

  if(chosen == NULL) {
    ELOG("Can't find any chip description file. Set STLINK_CHIPS_DIR, or run "
         "with -v to see every path that was tried.\n");
    return;
  }

  ILOG("Reading chip description files from %s\n", chosen);
  stlink_dir_foreach(chosen, CHIP_FILE_EXT, process_chipfile);
}
