/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-12.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */

#ifndef LOGGER_H
#define LOGGER_H

#ifdef _WIN32
#ifdef     STLOGGER_DLL_EXPORTING
#define        STLOGGER_API __declspec(dllexport)
#else      //STLOGGER_DLL_EXPORTING
#define        STLOGGER_API __declspec(dllimport)
#endif     //STLOGGER_DLL_EXPORTING
#else // _WIN32
#define    STLOGGER_API
#endif // _WIN32

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

    typedef struct STLOGGER_API logger_t logger_t;

    typedef void (*LogCallback)(long long timestamp,
                                int level,
                                const char* file,
                                const char* func,
                                int line,
                                const char* msg);

    /**
     * @brief Initializes the logger to output to standard error (stderr).
     *
     * This function configures the logging system to output logs to stderr.
     * It can optionally support colored output based on the `color_support`
     * parameter. This setting is useful for differentiating log levels
     * or improving readability in development and debugging scenarios.
     *
     * @param color_support If set to `true`, colored output for logs
     *                      is enabled, provided the terminal or
     *                      environment supports it. If `false`,
     *                      no color enhancement will be applied.
     */
    void STLOGGER_API logger_init_stderr(bool color_support);


    /**
     * Initializes the logger to write log messages to the specified file.
     *
     * @param filename The name of the file where log messages will be written.
     *                 This should be a valid file path.
     */
    void STLOGGER_API logger_init_file(const char* filename);

    /**
     * Initializes the logging system with the provided callback function.
     * The callback function will be invoked for each log message with the specified parameters.
     *
     * @param cb The callback function to handle log messages. It receives the log timestamp,
     *           log level, sender identifier, and the log message as arguments.
     */
    void STLOGGER_API logger_init_callback(LogCallback cb);

    /**
     * Checks if the logger has been successfully initialized.
     *
     * This function determines whether the logging system has been set up
     * and is ready for use.
     *
     * @return true if the logger is initialized, false otherwise.
     */
    bool STLOGGER_API logger_is_initialized();


    /**
     * @brief Creates and initializes a logger instance with the specified log level.
     *
     * This function initializes a logging mechanism by allocating memory for the
     * logger instance and setting the log level to the specified value.
     *
     * @param logger A pointer to a pointer that will be initialized to point to
     *               the created logger instance. If the input value is not `nullptr`,
     *               it will be set to `nullptr` by the function.
     * @param logLevel The logging level to be used by the logger. Acceptable values are:
     *                 - LOGGER_LEVEL_ERR: Error messages (highest severity).
     *                 - LOGGER_LEVEL_WRN: Warning messages.
     *                 - LOGGER_LEVEL_INF: Informational messages.
     *                 - LOGGER_LEVEL_DBG: Debug messages (lowest severity).
     *
     * @return void
     */
    bool STLOGGER_API logger_create(logger_t **logger, logger_level_t logLevel);

    /**
     * Destroys the logger object and releases any resources associated with it.
     *
     * @param logger A pointer to the logger_t structure to be destroyed. It must not be NULL.
     */
    void STLOGGER_API logger_destroy(logger_t *logger);


    /**
     * Logs a message with the specified log level, along with file, function, and line metadata.
     *
     * @param level The logging level (e.g., LOGGER_LEVEL_ERR, LOGGER_LEVEL_WRN, LOGGER_LEVEL_INF, or LOGGER_LEVEL_DBG).
     * @param file The name of the file from which the log message originates.
     * @param func The name of the function from which the log message originates.
     * @param line The line number in the source file from which the log message originates.
     * @param fmt The message format string (similar to printf format).
     * @param ... Additional arguments corresponding to the format specifiers in the format string.
     */
    void STLOGGER_API logger_log_global(logger_level_t level,
                                        const char* file,
                                        const char* func,
                                        int line,
                                        const char* fmt,
                                        ...);


    /**
     * Converts a given libusb log level to the corresponding logger_level_t value.
     *
     * @param loglevel The log level from libusb, typically represented as an integer.
     *                 This value is mapped to the appropriate logger_level_t enum.
     * @return Returns the corresponding logger_level_t enum value based on the input.
     *         If the input does not match a valid log level, the behavior is "info".
     */
    logger_level_t STLOGGER_API logger_convert_from_libusb(int loglevel);


    /**
     * Converts the given logger level to its corresponding libusb-compatible level.
     *
     * @param loglevel The logger level to be converted. This should be one of the
     *                 predefined levels in the logger_level_t enum, such as
     *                 LOGGER_LEVEL_ERR, LOGGER_LEVEL_WRN, LOGGER_LEVEL_INF,
     *                 or LOGGER_LEVEL_DBG.
     * @return The corresponding libusb-compatible logging level as an integer.
     *         Returns -1 if the provided logger level is invalid.
     */
    int STLOGGER_API logger_convert_to_libusb(logger_level_t loglevel);


    /**
     * Converts from legacy ugly-logging log level to the corresponding logger_level_t value.
     *
     * @param logLevel The integer representation of the log level to be converted.
     * @return The corresponding logger_level_t enum value. If the input does not match a valid log level,
     *         the behavior is "info".
     */
    logger_level_t STLOGGER_API logger_convert_from_ugly(int logLevel);


    /**
     * Converts the given logger level to a corresponding legacy ugly-logging representation.
     *
     * @param logLevel The logger level to be converted.
     * @return The legacy ugly-logging representation of the given logger level.
     */
    int STLOGGER_API logger_convert_to_ugly(logger_level_t logLevel);

    #define DLOG(...) logger_log_global(LOGGER_LEVEL_DBG, __FILE__, __func__, __LINE__, __VA_ARGS__)
    #define ILOG(...) logger_log_global(LOGGER_LEVEL_INF, __FILE__, __func__, __LINE__, __VA_ARGS__)
    #define WLOG(...) logger_log_global(LOGGER_LEVEL_WRN, __FILE__, __func__, __LINE__, __VA_ARGS__)
    #define ELOG(...) logger_log_global(LOGGER_LEVEL_ERR, __FILE__, __func__, __LINE__, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif // LOGGER_H