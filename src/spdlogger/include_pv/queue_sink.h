/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-13.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#ifndef QUEUESINK_H
#define QUEUESINK_H


#include <spdlog/sinks/sink.h>
#include "spdlogger/include/stlink/logging.h"

class queue_sink : public spdlog::sinks::sink {
public:
    void set_pattern(const std::string &pattern) override;

    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override;

    explicit queue_sink(logger_callback cb);
    void log(const spdlog::details::log_msg& msg) override;
    void flush() override;

private:
    logger_callback callback_;
};

#endif //QUEUESINK_H
