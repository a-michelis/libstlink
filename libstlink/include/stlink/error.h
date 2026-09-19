/**
 * @file    error.h
 * @brief   What went wrong, and what was being attempted at each layer.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * An error is immutable. Wrapping one produces a new error whose cause is the
 * old one, so a failure arrives with the whole chain behind it:
 *
 *     erasing the sector <- writing flash <- the probe refused the command
 *
 * Nothing from a third party library reaches a caller. A libusb or Win32
 * failure is translated into a code from this file with a context that says
 * what was being attempted, so the vocabulary here is the whole vocabulary.
 */

#ifndef STLINK_ERROR_H
#define STLINK_ERROR_H

#include <cstdint>
#include <memory>
#include <string>

#include <stlink/export.h>

namespace stlink
{
    /** @brief What kind of failure it is. */
    enum class ErrorCode : std::uint16_t
    {
        Unknown = 0,

        /* Talking to the probe */
        NotFound,      /**< No device matched. */
        AccessDenied,  /**< The operating system refused. */
        Busy,          /**< Something else holds the device. */
        Disconnected,  /**< It was there and is not any more. */
        Timeout,       /**< It did not answer in time. */
        Protocol,      /**< It answered, and the answer made no sense. */

        /* Talking to the target */
        TargetUnknown,   /**< The chip did not identify itself. */
        TargetRefused,   /**< The chip refused the operation. */
        NotSupported,    /**< This probe or this chip cannot do that. */

        /* Asked for the impossible */
        InvalidArgument, /**< The caller passed something that cannot work. */
        OutOfRange,      /**< An address or a length outside the device. */

        /* Us */
        OutOfMemory,
        Internal, /**< A broken invariant. A bug here, worth reporting. */
    };

    /** @brief The tag for a code, for logs and messages. */
    STLINK_API const char *to_string(ErrorCode code) noexcept;

    /**
     * @brief A failure, and optionally what caused it.
     *
     * Immutable. wrap() returns a new error rather than changing this one, so
     * an error can be held, copied and logged without anyone else's wrapping
     * being visible in it.
     */
    class STLINK_API Error
    {
    public:
        /**
         * @param code    what kind of failure it is
         * @param context what was being attempted, as a string literal
         */
        Error(ErrorCode code, const char *context) noexcept;

        /** @brief A new error caused by this one. */
        [[nodiscard]] Error wrap(ErrorCode code, const char *context) const;

        [[nodiscard]] ErrorCode code() const noexcept;
        [[nodiscard]] const char *context() const noexcept;

        /** @brief What caused this, or nullptr at the bottom of the chain. */
        [[nodiscard]] const Error *cause() const noexcept;

        /** @brief The innermost error, the one that actually failed. */
        [[nodiscard]] const Error &root() const noexcept;

        /** @brief The whole chain, outermost first, separated by " <- ". */
        [[nodiscard]] std::string describe() const;

    private:
        ErrorCode code_;
        const char *context_;
        std::shared_ptr<const Error> cause_;
    };
} // namespace stlink

#endif // STLINK_ERROR_H
