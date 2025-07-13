/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-12.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */

#ifndef LOGGER_H
#define LOGGER_H

#ifdef _WIN32
#ifdef SPDLOGGER_DLL_EXPORTING
#define SPDLOGGER_API __declspec(dllexport)
#else
#define SPDLOGGER_API __declspec(dllimport)
#endif
#else
#define SPDLOGGER_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Logging levels supported by the logger
 */
typedef enum {
    LOGGER_LEVEL_ERR = 0,  /**< Error messages (highest severity) */
    LOGGER_LEVEL_WRN = 1,  /**< Warning messages */
    LOGGER_LEVEL_INF = 2,  /**< Informational messages */
    LOGGER_LEVEL_DBG = 3   /**< Debug messages (lowest severity) */
} logger_level_t;

/**
 * @brief Callback function type for queue-based logging
 * @param unix_timestamp_ns Timestamp of the log message in nanoseconds since Unix epoch
 * @param level Logging level of the message (corresponds to logger_level_t)
 * @param sender Source of the log message (typically filename)
 * @param msg The formatted log message
 */
typedef void (*logger_callback)(long long unix_timestamp_ns, int level, const char* sender, const char* msg);

/**
 * @brief Initializes logging to stderr with colored output
 * @param max_log_level Maximum level of messages to output
 * @note Only the first initialization call will take effect
 */
void SPDLOGGER_API logger_init_stderr(logger_level_t max_log_level);

/**
 * @brief Initializes logging to a file
 * @param filename Path to the log file
 * @param max_log_level Maximum level of messages to output
 * @note Only the first initialization call will take effect
 */
void SPDLOGGER_API logger_init_file(const char* filename, logger_level_t max_log_level);

/**
 * @brief Initializes logging via callback function (to be used by an external logging front-end)
 * @param cb Callback function to receive log messages
 * @param max_log_level Maximum level of messages to process
 * @note Only the first initialization call will take effect
 */
void SPDLOGGER_API logger_init_callback(logger_callback cb, logger_level_t max_log_level);

/**
 * @brief Core logging function used by the logging macros
 * @param level Severity level of the message
 * @param file Source file name (automatically provided by macros)
 * @param func Function name (automatically provided by macros)
 * @param line Line number (automatically provided by macros)
 * @param fmt Printf-style format string
 * @param ... Variable arguments for format string
 */
void SPDLOGGER_API logger_log(logger_level_t level, const char* file, const char* func, int line, const char* fmt, ...);

int32_t SPDLOGGER_API logger_libusb_log_level(logger_level_t v);
/**
 * @brief Logging macros for different severity levels
* These macros automatically include source location information
 */
#define LOG_ERR(FMT, ...) logger_log(LOGGER_LEVEL_ERR, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define LOG_WRN(FMT, ...) logger_log(LOGGER_LEVEL_WRN, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define LOG_INF(FMT, ...) logger_log(LOGGER_LEVEL_INF, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define LOG_DBG(FMT, ...) logger_log(LOGGER_LEVEL_DBG, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)

#define ELOG(FMT, ...) logger_log(LOGGER_LEVEL_ERR, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define WLOG(FMT, ...) logger_log(LOGGER_LEVEL_WRN, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define ILOG(FMT, ...) logger_log(LOGGER_LEVEL_INF, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)
#define DLOG(FMT, ...) logger_log(LOGGER_LEVEL_DBG, __FILE__, __func__, __LINE__, FMT, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif //LOGGER_H