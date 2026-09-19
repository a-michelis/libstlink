/**
 * @file    log.h
 * @brief   Diagnostics.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This is not the error channel. A failure reaches the caller as a return
 * value; logging is the detail behind it.
 *
 * One sink receives every record. To send records to several places, or to
 * filter them, install a sink that wraps the others.
 */

#ifndef STLINK_LOG_H
#define STLINK_LOG_H

#include <cstdint>
#include <functional>
#include <string>

#include <stlink/export.h>

/*
 * Which printf the arguments are checked against. MinGW is GCC targeting the
 * Microsoft runtime, whose printf has no %z or %ll, so asking for the plain
 * printf archetype there rejects perfectly good format strings. gnu_printf
 * checks against the C99 one, which is what mingw-w64 actually provides once
 * __USE_MINGW_ANSI_STDIO is set, and the build sets it.
 */
#if defined(__MINGW32__)
#define STLINK_LOG_PRINTF(fmt_, args_) __attribute__((format(gnu_printf, fmt_, args_)))
#elif defined(__GNUC__)
#define STLINK_LOG_PRINTF(fmt_, args_) __attribute__((format(printf, fmt_, args_)))
#else
#define STLINK_LOG_PRINTF(fmt_, args_)
#endif

/** @brief The issuing function's full signature, however the compiler spells it. */
#if defined(_MSC_VER)
#define STLINK_LOG_FUNCTION __FUNCSIG__
#else
#define STLINK_LOG_FUNCTION __PRETTY_FUNCTION__
#endif

namespace stlink
{
    /** @brief How bad it is, ordered from worst to least. */
    enum class LogKind : std::uint8_t
    {
        Fatal = 0, /**< The library's own invariants are broken. A bug here. */
        Error,     /**< The operation failed. The device or the target said no. */
        Warning,   /**< It worked, but something was not as expected. */
        Info,      /**< What the library is doing. */
        Debug,     /**< Detail behind it: commands, replies, retries. */
    };

    /** @brief The three letter tag: FAT, ERR, WRN, INF, DBG. */
    STLINK_API const char *to_string(LogKind kind) noexcept;

    /** @brief One record, already formatted. */
    struct STLINK_API LogRecord
    {
        LogKind kind = LogKind::Info;
        /**
         * @brief The issuing function, as the compiler spells it.
         *
         * A string literal, so carrying it costs nothing. It is the whole
         * signature, return type and parameters included; qualified() reduces
         * it to namespace::Class::function.
         */
        const char *function = "";
        int line = 0;
        const char *message = ""; /**< Valid only for the duration of the call. */

        /** @brief The issuer as namespace::Class::function. */
        [[nodiscard]] std::string qualified() const;
    };

    /**
     * @brief Where records go, and how much detail is produced.
     *
     * Process wide, like the streams the default sink writes to.
     */
    class STLINK_API Log
    {
    public:
        using Sink = std::function<void(const LogRecord &)>;

        /** @brief Install the sink. An empty sink silences the library. */
        static void set_sink(Sink sink);

        /** @brief Back to writing Warning and worse to stderr, the rest to stdout. */
        static void reset_sink();

        /** @brief Records less severe than this are discarded. Defaults to Info. */
        static void set_threshold(LogKind kind) noexcept;
        [[nodiscard]] static LogKind threshold() noexcept;

        /**
         * @brief Whether a record of this kind would be kept.
         *
         * Lock free, so the macros can test it before formatting anything.
         */
        [[nodiscard]] static bool enabled(LogKind kind) noexcept;

        /** @brief Format a record and hand it to the sink. Used by the macros. */
        static void write(LogKind kind, const char *function, int line,
                          const char *format, ...) STLINK_LOG_PRINTF(4, 5);
    };
} // namespace stlink

/** @brief Emit a record, formatting its arguments only if it will be kept. */
#define STLINK_LOG(kind_, ...)                                                            \
    do                                                                                    \
    {                                                                                     \
        if (::stlink::Log::enabled(kind_))                                                \
        {                                                                                 \
            ::stlink::Log::write(kind_, STLINK_LOG_FUNCTION, __LINE__, __VA_ARGS__);      \
        }                                                                                 \
    } while (false)

#define STLINK_LOG_FAT(...) STLINK_LOG(::stlink::LogKind::Fatal, __VA_ARGS__)
#define STLINK_LOG_ERR(...) STLINK_LOG(::stlink::LogKind::Error, __VA_ARGS__)
#define STLINK_LOG_WRN(...) STLINK_LOG(::stlink::LogKind::Warning, __VA_ARGS__)
#define STLINK_LOG_INF(...) STLINK_LOG(::stlink::LogKind::Info, __VA_ARGS__)
#define STLINK_LOG_DBG(...) STLINK_LOG(::stlink::LogKind::Debug, __VA_ARGS__)

#endif // STLINK_LOG_H
