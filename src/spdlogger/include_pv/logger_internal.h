/* -----------------------------------------------
 * Created by Andreas Michelis on 2025-07-12.
 * Copyright (c) 2025, Andreas Michelis.
 * All Rights Reserved
 * ----------------------------------------------- */
 

#ifndef LOGGER_INTERNAL_H
#define LOGGER_INTERNAL_H

#include <memory>
#include <spdlog/logger.h>


std::shared_ptr<spdlog::logger> logger_get();



#endif //LOGGER_INTERNAL_H
