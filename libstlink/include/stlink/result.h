/**
 * @file    result.h
 * @brief   Either a value or an error.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Every operation that can fail returns one of these. Ask ok() first; anything
 * else throws when it does not apply, because asking for a value that is not
 * there is a mistake in the caller rather than something the device did.
 *
 *     auto result = IDevice::create(info);
 *     if (!result.ok()) { STLINK_LOG_ERR("%s", result.error().describe().c_str()); return; }
 *     auto device = result.value();
 *
 * An operation that produces nothing returns VoidResult. One that produces a
 * value returns Result<T>, which is a VoidResult that also carries the value.
 */

#ifndef STLINK_RESULT_H
#define STLINK_RESULT_H

#include <optional>
#include <stdexcept>
#include <utility>

#include <stlink/error.h>
#include <stlink/export.h>

namespace stlink
{
    /** @brief Thrown when a result is asked for something it does not hold. */
    class STLINK_API BadResultAccess : public std::logic_error
    {
    public:
        explicit BadResultAccess(const char *what);
    };

    template <typename T>
    class Result;

    /**
     * @brief The outcome of an operation that produces nothing.
     *
     * Also the base of Result<T>, so a caller that only wants to know whether
     * something worked can take one of these and ignore the value.
     *
     * Not polymorphic: there is no virtual destructor and nothing derived from
     * it is ever deleted through it.
     */
    class STLINK_API VoidResult
    {
    public:
        /** @brief It worked. */
        VoidResult() noexcept = default;

        /** @brief It did not. */
        VoidResult(Error error) noexcept;

        VoidResult(VoidResult &&) noexcept = default;
        VoidResult &operator=(VoidResult &&) noexcept = default;

        VoidResult(const VoidResult &) = delete;
        VoidResult &operator=(const VoidResult &) = delete;

        /*
         * Slicing a Result<T> down to its base would silently drop the value.
         * These match exactly, so they win against the move constructor above
         * and the mistake is a compile error rather than a lost value.
         */
        template <typename T>
        VoidResult(Result<T> &&) = delete;

        template <typename T>
        VoidResult &operator=(Result<T> &&) = delete;

        /** @brief Whether the operation succeeded. The only question that never throws. */
        [[nodiscard]] bool ok() const noexcept;

        /**
         * @brief Why it failed.
         *
         * Stays where it is, so it can be read as often as needed.
         *
         * @throws BadResultAccess if the operation succeeded.
         */
        [[nodiscard]] const Error &error() const;

    private:
        std::optional<Error> error_;
    };

    /**
     * @brief The outcome of an operation that produces a value.
     *
     * Move only, and the value is moved out, so T may be move only itself.
     */
    template <typename T>
    class Result : public VoidResult
    {
    public:
        Result(T value) : value_(std::move(value)) {}
        Result(Error error) noexcept : VoidResult(std::move(error)) {}

        Result(Result &&) noexcept = default;
        Result &operator=(Result &&) noexcept = default;

        Result(const Result &) = delete;
        Result &operator=(const Result &) = delete;

        /**
         * @brief Take the value.
         *
         * @throws BadResultAccess if the operation failed, or if the value has
         *         already been taken.
         */
        [[nodiscard]] T value()
        {
            if (!ok())
            {
                throw BadResultAccess("this result holds an error, not a value");
            }

            if (!value_.has_value())
            {
                throw BadResultAccess("the value of this result has already been taken");
            }

            T taken = std::move(*value_);
            value_.reset();

            return taken;
        }

    private:
        std::optional<T> value_;
    };
} // namespace stlink

#endif // STLINK_RESULT_H
