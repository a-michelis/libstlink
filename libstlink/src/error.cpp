/**
 * @file    error.cpp
 * @brief   What went wrong, and what was being attempted at each layer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/error.h>

namespace stlink
{
    const char *to_string(ErrorCode code) noexcept
    {
        switch (code)
        {
        case ErrorCode::Unknown:
            return "unknown";
        case ErrorCode::IO:
            return "input or output";
        case ErrorCode::NotFound:
            return "not found";
        case ErrorCode::AccessDenied:
            return "access denied";
        case ErrorCode::Busy:
            return "busy";
        case ErrorCode::Disconnected:
            return "disconnected";
        case ErrorCode::Timeout:
            return "timeout";
        case ErrorCode::Protocol:
            return "protocol";
        case ErrorCode::TargetUnknown:
            return "target unknown";
        case ErrorCode::TargetRefused:
            return "target refused";
        case ErrorCode::NotSupported:
            return "not supported";
        case ErrorCode::InvalidArgument:
            return "invalid argument";
        case ErrorCode::OutOfRange:
            return "out of range";
        case ErrorCode::OutOfMemory:
            return "out of memory";
        case ErrorCode::Internal:
            return "internal";
        }

        return "unknown";
    }

    Error::Error(ErrorCode code, const char *context) noexcept
        : code_(code), context_((context != nullptr) ? context : "")
    {
    }

    Error Error::wrap(ErrorCode code, const char *context) const
    {
        Error wrapper(code, context);
        wrapper.cause_ = std::make_shared<const Error>(*this);

        return wrapper;
    }

    ErrorCode Error::code() const noexcept
    {
        return code_;
    }

    const char *Error::context() const noexcept
    {
        return context_;
    }

    const Error *Error::cause() const noexcept
    {
        return cause_.get();
    }

    const Error &Error::root() const noexcept
    {
        const Error *innermost = this;

        while (innermost->cause_ != nullptr)
        {
            innermost = innermost->cause_.get();
        }

        return *innermost;
    }

    std::string Error::describe() const
    {
        std::string text;

        for (const Error *e = this; e != nullptr; e = e->cause_.get())
        {
            if (!text.empty())
            {
                text += " <- ";
            }

            text += e->context_;
            text += " (";
            text += to_string(e->code_);
            text += ")";
        }

        return text;
    }
} // namespace stlink
