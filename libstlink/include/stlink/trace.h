/**
 * @file    trace.h
 * @brief   Trace output arriving from the target.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Reached through IDevice::trace(), and owned by that device. Ask
 * IDevice::has_trace() first: both the programmer and the chip have to carry
 * trace, and when either does not, everything here refuses.
 *
 * This is a byte stream and nothing more. What those bytes mean, ITM packets,
 * timestamps, which stimulus port a byte came from, is a decoding problem and
 * belongs above the library rather than in it.
 */

#ifndef STLINK_TRACE_H
#define STLINK_TRACE_H

#include <cstddef>
#include <cstdint>

#include <stlink/export.h>
#include <stlink/result.h>

namespace stlink
{
    /**
     * @brief The trace channel of one device.
     *
     * One instance belongs to one thread at a time, and to the device that
     * owns it: it is valid for exactly as long as that device is.
     */
    class STLINK_API ITrace
    {
    public:
        virtual ~ITrace() = default;

        ITrace(const ITrace &) = delete;
        ITrace &operator=(const ITrace &) = delete;

        /**
         * @brief Start delivering trace at @p hz.
         *
         * @p hz must not exceed max_frequency(). Asking for more than the
         * programmer can carry is refused rather than clamped, because trace
         * that overruns is lost silently and a caller would have no way to
         * tell a gap from a quiet target.
         *
         * The target must also be told to emit it. That means writing its
         * debug configuration, which is the chip's business rather than the
         * probe's, and is not done here.
         */
        [[nodiscard]] virtual VoidResult start(std::uint32_t hz) = 0;

        /** @brief Stop delivering it. */
        [[nodiscard]] virtual VoidResult stop() = 0;

        /** @brief Whether it is currently being delivered. */
        [[nodiscard]] virtual bool running() const noexcept = 0;

        /**
         * @brief Take whatever trace has arrived, up to @p size bytes.
         *
         * Returns zero when nothing is waiting, which is not an error: a
         * quiet target produces no trace.
         *
         * Today this polls the programmer, so it must be called from the same
         * thread as everything else on the device. It is shaped as a read
         * from a buffer rather than a poll so that it can become one without
         * the signature changing, once something inside the library is
         * draining the channel on its own.
         *
         * @return how many bytes were taken
         */
        [[nodiscard]] virtual Result<std::size_t> read(std::uint8_t *data, std::size_t size) = 0;

        /**
         * @brief The fastest rate this programmer will carry, in hertz.
         *
         * Zero when it carries no trace at all.
         */
        [[nodiscard]] virtual std::uint32_t max_frequency() const noexcept = 0;

    protected:
        ITrace() = default;
    };
} // namespace stlink

#endif // STLINK_TRACE_H
