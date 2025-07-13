/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-13.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#ifndef CUSTOMSTDERRCOLORSINK_H
#define CUSTOMSTDERRCOLORSINK_H


#include <spdlog/sinks/sink.h>
#include <spdlog/details/log_msg.h>

class custom_stderr_color_sink : public spdlog::sinks::sink
{
public:
    void set_pattern(const std::string &pattern) override;

    void set_formatter(std::unique_ptr<spdlog::formatter> sink_formatter) override;

protected:
    void log(const spdlog::details::log_msg& msg) override;
    void flush() override;
};

#endif //CUSTOMSTDERRCOLORSINK_H
