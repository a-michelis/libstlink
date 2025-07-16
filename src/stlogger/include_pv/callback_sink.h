/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-13.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */


#ifndef CALLBACK_SINK_H
#define CALLBACK_SINK_H

#include "spdlog/sinks/base_sink.h"
#include "mutex"
#include "stlink/logging.h"

class callback_sink final : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit callback_sink(LogCallback callback);
protected:
    LogCallback callback_;

    void sink_it_(const spdlog::details::log_msg &msg) override;

    void flush_() override;
};

#endif //CALLBACK_SINK_H
