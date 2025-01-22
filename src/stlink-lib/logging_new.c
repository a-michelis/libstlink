/*
 * File: logging.c
 *
 * UglyLogging: Slow, yet another wheel reinvented, but enough to make the rest of our code pretty enough.
 * Ugly, low performance, configurable level, logging "framework"
 */

#define __STDC_WANT_LIB_EXT1__ 1

#include <stdint.h>
#include <stdio.h>

#include <stdarg.h>
#include <time.h>

#include "logging_new.h"

void _stlink_loghandler_default_func(uint8_t level, const char* sender, const char* message);
void _stlink_loghandler_color_func(uint8_t level, const char* sender, const char* message);

bool _stlink_log_enabled = true;
uint8_t _stlink_loglevel = 3;
uint8_t _stlink_tracelevel = 0;
uint8_t _stlink_log_max = 3;
void(*_stlink_log_handler)(uint8_t level, const char* sender, const char* message) = _stlink_loghandler_color_func;

const char cnorm[15][9] = {
		"\033[0m\0",
		"\033[0;31m\0",
		"\033[0;33m\0",
		"\033[0;34m\0",
		"\033[0;35m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
		"\033[0;100m\0",
};

const char cbold[15][11] = {
		"\033[0m\0",
		"\033[0;1;31m\0",
		"\033[0;1;33m\0",
		"\033[0;1;34m\0",
		"\033[0;1;35m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
		"\033[0;1;100m\0",
};

void stlink_log_enable() {
    _stlink_log_enabled = true;
}

void stlink_log_disable() {
    _stlink_log_enabled = false;
}

void stlink_loghandler_custom(void (*func)(uint8_t, const char *, const char *)) {
    if (func == NULL) return;
    _stlink_log_handler = func;
}

void stlink_loghandler_default() {
    _stlink_log_handler = _stlink_loghandler_default_func;
}

void stlink_loghandler_color() {
    _stlink_log_handler = _stlink_loghandler_color_func;
}

void stlink_set_loglevel(st_loglevel level) {
    if (level == STLL_NONE) return;
    if (level > STLL_DEBUG) return;
    _stlink_loglevel = level;
    if (_stlink_loglevel < STLL_DEBUG) _stlink_log_max = _stlink_loglevel;
    else _stlink_log_max = STLL_DEBUG + _stlink_tracelevel;
}

void stlink_set_tracelevel(uint8_t level) {
    if (level > 9) level = 9;
    _stlink_tracelevel = level;
    if (_stlink_loglevel < STLL_DEBUG) _stlink_log_max = _stlink_loglevel;
    else _stlink_log_max = STLL_DEBUG + _stlink_tracelevel;
}


char log_buffer[0x10000];
void __stlink_log(uint8_t level, const char *sender, const char *fmt, ...) {
    if (level > _stlink_log_max ) return;
    if (sender == NULL || fmt == NULL) return;
    
    va_list args;
    va_start(args, fmt);
    ssize_t len = vsnprintf(log_buffer, 0xFFFF, fmt, args);
    va_end(args);
    if (len < 0) return;
    
    log_buffer[len] = '\0';
    _stlink_log_handler(level, sender, log_buffer);
}


void _stlink_loghandler_default_func(uint8_t level, const char *sender, const char *message) {
    
    if (level < STLL_WARN)
    {
        fprintf(stderr, "[ERR] ");
    }
    else if (level < STLL_INFO)
    {
        fprintf(stderr, "[WRN] ");
    }
    else if (level < STLL_DEBUG)
    {
        fprintf(stderr, "[INF] ");
    }
    else
    {
        fprintf(stderr, "[TR%d] ", level - STLL_DEBUG);
    }
    
    time_t rawtime;
    struct tm * timeinfo;
    
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    
    fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] %s : %s\n",
           timeinfo->tm_year + 1900,
           timeinfo->tm_mon + 1,
           timeinfo->tm_mday,
           timeinfo->tm_hour,
           timeinfo->tm_min,
           timeinfo->tm_sec,
           sender,
           message);
}
void _stlink_loghandler_color_func(uint8_t level, const char* sender, const char* message){
    if (level < STLL_WARN)
    {
        fprintf(stderr, "[%sERR\033[0m] ", cnorm[level]);
    }
    else if (level < STLL_INFO)
    {
        fprintf(stderr, "[%sWRN\033[0m] ", cnorm[level]);
    }
    else if (level < STLL_DEBUG)
    {
        fprintf(stderr, "[%sINF\033[0m] ", cnorm[level]);
    }
    else
    {
        fprintf(stderr, "[%sTR%d\033[0m] ", cnorm[level], level - STLL_DEBUG);
    }
    
    time_t rawtime;
    struct tm * timeinfo;
    
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    
    fprintf(stderr, "[\033[32m%04d-%02d-%02d %02d:%02d:%02d\033[0m] %s%s\033[0m : %s%s\033[0m\n",
           timeinfo->tm_year + 1900,
           timeinfo->tm_mon + 1,
           timeinfo->tm_mday,
           timeinfo->tm_hour,
           timeinfo->tm_min,
           timeinfo->tm_sec,
           cbold[level],
           sender,
           cnorm[level],
           message);
}

int32_t ugly_libusb_log_level(st_loglevel v) {
#ifdef __FreeBSD__
    // FreeBSD includes its own reimplementation of libusb.
  // Its libusb_set_debug() function expects a lib_debug_level
  // instead of a lib_log_level and is verbose enough to drown out
  // all other output.
    switch (v) {
        case STLL_INFO:
            return (1);
        case STLL_WARN:
            return (0);
        case STLL_ERR:
            return (0);
        default:
            return (3);

    }
  return (0);
#else
    switch (v) {
        case STLL_INFO:
            return (3);
        case STLL_WARN:
            return (2);
        case STLL_ERR:
            return (1);
        default:
            return (4);

    }
#endif
}
