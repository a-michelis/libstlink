/*
 * File: logging.h
 *
 * UglyLogging: Slow, yet another wheel reinvented, but enough to make the rest of our code pretty enough.
 * Ugly, low performance, configurable level, logging "framework"
 */

#ifndef LOGGING_NEW_H
#define LOGGING_NEW_H

#include <stdint.h>
#include <stdbool.h>

#define STLINK_LE(fmt, ...) __stlink_log(STLL_ERR, __func__, fmt, ##__VA_ARGS__)
#define STLINK_LW(fmt, ...) __stlink_log(STLL_WARN, __func__, fmt, ##__VA_ARGS__)
#define STLINK_LI(fmt, ...) __stlink_log(STLL_INFO, __func__, fmt, ##__VA_ARGS__)
#define STLINK_TR(lvl, fmt, ...) __stlink_log((STLL_DEBUG + (lvl)), __func__, fmt, ##__VA_ARGS__)

#define DLOG(fmt, ...) STLINK_TR(0, fmt, ##__VA_ARGS__)
#define ILOG(fmt, ...) STLINK_LI(fmt, ##__VA_ARGS__)
#define WLOG(fmt, ...) STLINK_LW(fmt, ##__VA_ARGS__)
#define ELOG(fmt, ...) STLINK_LE(fmt, ##__VA_ARGS__)

#ifdef  __cplusplus
extern "C" {
#endif // __cplusplus

typedef enum {
//  Name       Val   Description                                   | Abbr         | Foreground
	STLL_NONE  = 0, // Default Value - Not Logged (error indication) | -            | -
	STLL_ERR   = 1, // Error (fatal)                                 | [ERR]        | Red
	STLL_WARN  = 2, // Warning (non-fatal)                           | [WRN]        | Dark Yellow
	STLL_INFO  = 3, // Info (general flow) (-v)                      | [INF]        | Blue
	STLL_DEBUG = 4, // Tracing (TR0 - TR9, 10 Levels of tracing      | [TR0]..[TR9] | DarkGrey
} st_loglevel;

/// Enables Logging (if not enabled)
void stlink_log_enable();

/// Disables Logging (if enabled)
void stlink_log_disable();

/// Sets a custom function as the log handler
/// The function's arguments have the following description:
/// 	level :     Log/Trace unified level. It will contain only values from 1 to 13
///                     1:  Error   (STLL_ERR)
///                     2:  Warning (STLL_WARN)
///                     3:  Info    (STLL_INFO)
///                     4:  Trace 0 (STLL_DEBUG, TR0)
///                     5:  Trace 1 (STLL_DEBUG, TR1)
///					    [...]
///                     13: Trace 9 (STLL_DEBUG, TR9)
///     sender:     The function name that issued this log
///     message:    The log's content
void stlink_loghandler_custom(void(*func)(uint8_t level, const char* sender, const char* message));

/// Sets the default STDERR log handler as the log handler
void stlink_loghandler_default();

/// Sets the color-enabled STDERR log handler as the log handler
void stlink_loghandler_color();

/// Sets the logging level to either Error, Warning, Info or Debug. Value "STLL_NONE" is ignored.
void stlink_set_loglevel(st_loglevel level);

/// Sets the tracing level to a value of [0-9]. Values over that are clipped to 9.
/// Taken into account only if log level is STLL_DEBUG.
void stlink_set_tracelevel(uint8_t level);

#ifdef  __cplusplus
}
#endif // __cplusplus

/// Adds a new log.
/// IMPORTANT: NOT TO BE USED DIRECTLY,
///            USE STLINK_LE(fmt, ...), STLINK_LW(fmt, ...),
///            STLINK_LI(fmt, ...) AND STLINK_TR(lvl, fmt, ...)
///            MACRO-FUNCTIONS
void __stlink_log(uint8_t level, const char* sender, const char *fmt, ...);


int32_t ugly_libusb_log_level(st_loglevel v);

#endif // LOGGING_NEW_H
