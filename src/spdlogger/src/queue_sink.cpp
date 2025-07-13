/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-13.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#include "queue_sink.h"
#include <chrono>
#include <string_view>

void queue_sink::set_pattern(const std::string &pattern) {
}

void queue_sink::set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) {
}

queue_sink::queue_sink(logger_callback cb) : callback_(cb) {}

void queue_sink::log(const spdlog::details::log_msg& msg)
{
    if (!callback_) return;

    auto tp = std::chrono::time_point_cast<std::chrono::nanoseconds>(msg.time);
    long long unix_ns = tp.time_since_epoch().count();

    int level = static_cast<int>(msg.level);

    const char* sender = msg.source.filename ? msg.source.filename : "";

    std::string_view sv(msg.payload.data(), msg.payload.size());

    callback_(unix_ns, level, sender, sv.data());
}

void queue_sink::flush() {}