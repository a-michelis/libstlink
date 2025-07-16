/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-14.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */

#include <stlink/logging.h>

#include <cstdarg>
#include "callback_sink.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/stdout_sinks.h"

std::shared_ptr<spdlog::sinks::sink> _globalSink{nullptr};
std::shared_ptr<spdlog::logger> _globalLogger{nullptr};
std::mutex _globalLogMutex{};

spdlog::level::level_enum to_spdlog_level(logger_level_t level) {
    switch (level) {
        case LOGGER_LEVEL_ERR:
            return spdlog::level::err;
        case LOGGER_LEVEL_WRN:
            return spdlog::level::warn;
        case LOGGER_LEVEL_INF:
            return spdlog::level::info;
        case LOGGER_LEVEL_DBG:
            return spdlog::level::debug;
        default:
            return spdlog::level::warn;
    }
}

void logger_init_stderr(bool color_support)
{
    std::lock_guard lock(_globalLogMutex);
    if (_globalSink) return;
    if (color_support) {
        _globalSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
    }
    else {
        _globalSink = std::make_shared<spdlog::sinks::stderr_sink_mt>();
    }
    _globalSink->set_pattern("[%L] [%D %H:%M:%S.%e] %! (%s:%#) : %v");
    _globalLogger = std::make_shared<spdlog::logger>("global", _globalSink);
    spdlog::register_logger(_globalLogger);
    spdlog::set_default_logger(_globalLogger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_every(std::chrono::milliseconds(100));
}

void logger_init_file(const char *filename)
{
    std::lock_guard lock(_globalLogMutex);
    if (_globalSink) return;
    _globalSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
    _globalSink->set_pattern("[%L] [%D %H:%M:%S.%e] %! (%s:%#) : %v");
    _globalLogger = std::make_shared<spdlog::logger>("global", _globalSink);
    spdlog::register_logger(_globalLogger);
    spdlog::set_default_logger(_globalLogger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_every(std::chrono::milliseconds(100));
}

void logger_init_callback(LogCallback cb)
{
    std::lock_guard lock(_globalLogMutex);
    if (_globalSink) return;
    _globalSink = std::make_shared<callback_sink>(cb);
    _globalLogger = std::make_shared<spdlog::logger>("global", _globalSink);
    spdlog::register_logger(_globalLogger);
    spdlog::set_default_logger(_globalLogger);
    spdlog::set_level(spdlog::level::debug);
}

enum ugly_loglevel {
    UDEBUG = 90,
    UINFO  = 50,
    UWARN  = 30,
    UERROR = 20
};

logger_level_t logger_convert_from_libusb(int loglevel) {
    #ifdef __FreeBSD__
        // FreeBSD includes its own reimplementation of libusb.
        // Its libusb_set_debug() function expects a lib_debug_level
        // instead of a lib_log_level and is verbose enough to drown out
        // all other output.
        switch (loglevel) {
            case 3:
                return LOGGER_LEVEL_DBG; // LIBUSB_DEBUG_FUNCTION + LIBUSB_DEBUG_TRANSFER
            case 1:
                return LOGGER_LEVEL_INF; // LIBUSB_DEBUG_FUNCTION only
            case 0:
                return LOGGER_LEVEL_ERR; // LIBUSB_DEBUG_NO
        }

        // Default: Warning
        return LOGGER_LEVEL_WRN;
    #else
        switch (loglevel) {
            case 4:
                return LOGGER_LEVEL_DBG;
            case 3:
                return LOGGER_LEVEL_INF;
            case 2:
                return LOGGER_LEVEL_WRN;
            case 1:
                return LOGGER_LEVEL_ERR;
        }
        // Default: Warning
        return LOGGER_LEVEL_WRN;
    #endif
}

int logger_convert_to_libusb(logger_level_t loglevel)
{
#ifdef __FreeBSD__
    // FreeBSD includes its own reimplementation of libusb.
    // Its libusb_set_debug() function expects a lib_debug_level
    // instead of a lib_log_level and is verbose enough to drown out
    // all other output.
    switch (loglevel) {
        case LOGGER_LEVEL_DBG:
            return (3); // LIBUSB_DEBUG_FUNCTION + LIBUSB_DEBUG_TRANSFER
        case LOGGER_LEVEL_INF:
            return (1); // LIBUSB_DEBUG_FUNCTION only
        case LOGGER_LEVEL_WRN:
            return (0); // LIBUSB_DEBUG_NO
        case LOGGER_LEVEL_ERR:
            return (0); // LIBUSB_DEBUG_NO
    }

    // Default: Warning/Error
    return (0);
#else
    switch (loglevel) {
        case LOGGER_LEVEL_DBG:
            return (4);
        case LOGGER_LEVEL_INF:
            return (3);
        case LOGGER_LEVEL_WRN:
            return (2);
        case LOGGER_LEVEL_ERR:
            return (1);
    }
    // Default: Warning
    return (2);
#endif
}

logger_level_t logger_convert_from_ugly(int logLevel) {
    switch (logLevel) {
        case UDEBUG:
            return LOGGER_LEVEL_DBG;
        case UINFO:
            return LOGGER_LEVEL_INF;
        case UWARN:
            return LOGGER_LEVEL_WRN;
        case UERROR:
            return LOGGER_LEVEL_ERR;
        default:
            return LOGGER_LEVEL_WRN;
    }
}

int logger_convert_to_ugly(logger_level_t logLevel) {
    switch (logLevel) {
        case LOGGER_LEVEL_DBG:
            return UDEBUG;
        case LOGGER_LEVEL_INF:
            return UINFO;
        case LOGGER_LEVEL_WRN:
            return UWARN;
        case LOGGER_LEVEL_ERR:
            return UERROR;
        default:
            return UWARN;
    }
}

void logger_log_global(logger_level_t level, const char *file, const char *func, int line, const char *fmt, ...) {
    std::lock_guard lock(_globalLogMutex);
    if (!_globalSink || !_globalLogger) return;
    const auto loc = spdlog::source_loc{file, line, func};

    va_list args;
    va_start(args, fmt);
    static char tmp[10240];
    const auto len = vsnprintf(tmp, sizeof(tmp), fmt, args);
    va_end(args);
    if (len <= 0) {
        return;
    }

    const spdlog::string_view_t str(tmp, len);

    _globalLogger->log(loc, to_spdlog_level(level), str);
}
