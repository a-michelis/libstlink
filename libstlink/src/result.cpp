/**
 * @file    result.cpp
 * @brief   Either a value or an error.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/result.h>

namespace stlink
{
    BadResultAccess::BadResultAccess(const char *what) : std::logic_error(what)
    {
    }

    VoidResult::VoidResult(Error error) noexcept : error_(std::move(error))
    {
    }

    bool VoidResult::ok() const noexcept
    {
        return !error_.has_value();
    }

    const Error &VoidResult::error() const
    {
        if (!error_.has_value())
        {
            throw BadResultAccess("this result holds no error, the operation succeeded");
        }

        return *error_;
    }
} // namespace stlink
