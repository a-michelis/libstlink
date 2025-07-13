/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-12.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */

#include "logger_internal.h"

#include <cstdarg>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/sinks/basic_file_sink.h>


#include "custom_stderr_color_sink.h"
#include "queue_sink.h"
#include "stlink/logging.h"

static std::shared_ptr<spdlog::logger> global_logger;
static std::mutex logger_mutex;


spdlog::level::level_enum to_spdlog_level(logger_level_t lvl) {
    switch (lvl) {
        case LOGGER_LEVEL_ERR: return spdlog::level::err;
        case LOGGER_LEVEL_WRN: return spdlog::level::warn;
        case LOGGER_LEVEL_INF: return spdlog::level::info;
        case LOGGER_LEVEL_DBG: return spdlog::level::debug;
        default: return spdlog::level::info;
    }
}

std::shared_ptr<spdlog::logger> logger_get() {
    std::lock_guard lock(logger_mutex);
    return global_logger;
}

extern "C" void __declspec(dllexport) logger_init_stderr(logger_level_t max_log_level) {
    std::lock_guard lock(logger_mutex);
    if (global_logger) return;

    auto sink = std::make_shared<custom_stderr_color_sink>();

    // Format: [DateTime] [LogLevel] function:line : message
    sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%L%$] %s:%# : %v");

    global_logger = std::make_shared<spdlog::logger>("stderr_logger", sink);
    global_logger->set_level(to_spdlog_level(max_log_level));
    spdlog::set_default_logger(global_logger);
}

extern "C" void __declspec(dllexport) logger_init_file(const char* filename, logger_level_t max_log_level) {
    std::lock_guard lock(logger_mutex);
    if (global_logger) return;

    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
    sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%L] %s:%# : %v");

    global_logger = std::make_shared<spdlog::logger>("file_logger", sink);
    global_logger->set_level(to_spdlog_level(max_log_level));
    spdlog::set_default_logger(global_logger);
}

extern "C" void __declspec(dllexport) logger_init_callback(logger_callback cb, logger_level_t max_log_level) {
    std::lock_guard lock(logger_mutex);
    if (global_logger) return;

    auto sink = std::make_shared<queue_sink>(cb);

    // Pattern not needed for QueueSink; the callback gets raw message directly.

    global_logger = std::make_shared<spdlog::logger>("queue_logger", sink);
    global_logger->set_level(to_spdlog_level(max_log_level));
    spdlog::set_default_logger(global_logger);
}


extern "C" void __declspec(dllexport) logger_log(logger_level_t level, const char* file, const char* func, int line, const char* fmt, ...) {
    std::lock_guard lock(logger_mutex);
    if (!global_logger) return;

    va_list args;
    va_start(args, fmt);
    std::string msg = fmt::vformat(fmt, fmt::make_format_args(args));
    va_end(args);

    spdlog::source_loc loc{file, line, func};

    global_logger->log(loc, to_spdlog_level(level), msg);
}

extern "C" int32_t __declspec(dllexport) logger_libusb_log_level(logger_level_t v) {
#ifdef __FreeBSD__
    // FreeBSD includes its own reimplementation of libusb.
    // Its libusb_set_debug() function expects a lib_debug_level
    // instead of a lib_log_level and is verbose enough to drown out
    // all other output.
    switch (v) {
        case LOGGER_LEVEL_DBG:
            return (3); // LIBUSB_DEBUG_FUNCTION + LIBUSB_DEBUG_TRANSFER
        case LOGGER_LEVEL_INF:
            return (1); // LIBUSB_DEBUG_FUNCTION only
        case LOGGER_LEVEL_WRN:
            return (0); // LIBUSB_DEBUG_NO
        case LOGGER_LEVEL_ERR:
            return (0); // LIBUSB_DEBUG_NO
    }
    return (0);
#else
    switch (v) {
        case LOGGER_LEVEL_DBG:
            return (4);
        case LOGGER_LEVEL_INF:
            return (3);
        case LOGGER_LEVEL_WRN:
            return (2);
        case LOGGER_LEVEL_ERR:
            return (1);
    }
    return (2);
#endif
}