/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-13.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#include "callback_sink.h"
#include <chrono>

callback_sink::callback_sink(LogCallback callback) : callback_(callback) {

}

logger_level_t from_spdlog_level(spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::off:
        case spdlog::level::critical:
        case spdlog::level::err:
            return LOGGER_LEVEL_ERR;
        case spdlog::level::warn:
            return LOGGER_LEVEL_WRN;
        case spdlog::level::info:
            return LOGGER_LEVEL_INF;
        default:
            return LOGGER_LEVEL_DBG;
    }
}

void callback_sink::sink_it_(const spdlog::details::log_msg &msg) {
    const auto ts = msg.time.time_since_epoch().count();
    const auto level = msg.level;
    const auto file = msg.source.filename;
    const auto func = msg.source.funcname;
    const auto line = msg.source.line;
    const auto pld = msg.payload.data();
    callback_(ts, level, file, func, line, pld);
}

void callback_sink::flush_() {
    // Do nothing. No flush needed.
}
