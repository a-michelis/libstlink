/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-13.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */
 

#include "custom_stderr_color_sink.h"
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <spdlog/details/os.h>
#include <chrono>

void custom_stderr_color_sink::set_pattern(const std::string &pattern) {
}

void custom_stderr_color_sink::set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) {
}

void custom_stderr_color_sink::log(const spdlog::details::log_msg& msg)
{
    // pick ANSI color code by level
    const char* color = "\033[0m";
    switch (msg.level)
    {
        case spdlog::level::err:   color = "\033[31m"; break;
        case spdlog::level::warn:  color = "\033[33m"; break;
        case spdlog::level::info:  color = "\033[37m"; break;
        case spdlog::level::debug: color = "\033[90m"; break;
        default:                   break;
    }

    // format timestamp
    auto t = std::chrono::system_clock::to_time_t(msg.time);
    std::tm tm = spdlog::details::os::localtime(t);
    char timestamp[32];
    std::snprintf(timestamp, sizeof(timestamp),
                  "%04d-%02d-%02d %02d:%02d:%02d",
                  tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
                  tm.tm_hour, tm.tm_min, tm.tm_sec);

    // pull out function name (if source enabled)
    const char* func = msg.source.funcname ? msg.source.funcname : "unknown";

    // assemble the final line
    fmt::memory_buffer buf;
    constexpr size_t estimated_size = 256;  // Should be enough for typical log messages
    buf.reserve(estimated_size);

    fmt::format_to_n(buf.data(), estimated_size,
        "{}[{}] [{:<5}] {}:{} : {}\033[0m\n",
        color,
        timestamp,
        spdlog::level::to_string_view(msg.level),
        func,
        msg.source.line,
        msg.payload
    );
    
    // write to stderr
    std::fwrite(buf.data(), 1, buf.size(), stderr);
}

void custom_stderr_color_sink::flush()
{
    std::fflush(stderr);
}