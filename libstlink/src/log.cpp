/**
 * @file    log.cpp
 * @brief   Diagnostics.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stlink/log.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace stlink
{
    namespace
    {
        /*
         * The threshold is atomic so that enabled() takes no lock: it is read
         * once per call site per record, kept or not. The sink is behind a
         * mutex because installing one is rare and calling it must not race
         * with replacing it.
         */
        std::atomic<LogKind> g_threshold{LogKind::Info};

        std::mutex &sink_mutex()
        {
            static std::mutex m;
            return m;
        }

        void default_sink(const LogRecord &record);

        Log::Sink &sink()
        {
            static Log::Sink s = &default_sink;
            return s;
        }

        void default_sink(const LogRecord &record)
        {
            std::FILE *stream = (record.kind <= LogKind::Warning) ? stderr : stdout;

            std::fprintf(stream, "%s %s:%d: %s\n", to_string(record.kind),
                         record.qualified().c_str(), record.line, record.message);
            std::fflush(stream);
        }

        /* Whether c can appear in an identifier or a qualification. */
        bool is_name_char(char c)
        {
            return (c == '_') || (c == ':') || (c == '~') ||
                   ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z')) ||
                   ((c >= '0') && (c <= '9'));
        }
    } // namespace

    const char *to_string(LogKind kind) noexcept
    {
        switch (kind)
        {
        case LogKind::Fatal:
            return "FAT";
        case LogKind::Error:
            return "ERR";
        case LogKind::Warning:
            return "WRN";
        case LogKind::Info:
            return "INF";
        case LogKind::Debug:
            return "DBG";
        }

        return "???";
    }

    /*
     * Both spellings are a whole signature, and differ in what surrounds the
     * name:
     *
     *   gcc   void stlink::Transport::open(int)
     *   msvc  void __cdecl stlink::Transport::open(int)
     *
     * The name ends where the parameter list begins, and starts after whatever
     * precedes it. Template arguments and a parenthesised return type are
     * skipped by scanning from the right, so the first '(' found belongs to the
     * parameters.
     */
    std::string LogRecord::qualified() const
    {
        const std::string signature(function);

        if (signature.empty())
        {
            return signature;
        }

        std::size_t depth = 0;
        std::size_t end = std::string::npos;

        for (std::size_t i = signature.size(); i-- > 0;)
        {
            const char c = signature[i];

            if ((c == ')') || (c == '>'))
            {
                ++depth;
            }
            else if ((c == '(') || (c == '<'))
            {
                if (depth > 0)
                {
                    --depth;
                }

                if ((c == '(') && (depth == 0))
                {
                    end = i;
                    break;
                }
            }
        }

        if (end == std::string::npos)
        {
            end = signature.size();
        }

        std::size_t begin = end;

        while ((begin > 0) && is_name_char(signature[begin - 1]))
        {
            --begin;
        }

        if (begin >= end)
        {
            return signature;
        }

        return signature.substr(begin, end - begin);
    }

    void Log::set_sink(Sink s)
    {
        const std::lock_guard<std::mutex> lock(sink_mutex());
        sink() = std::move(s);
    }

    void Log::reset_sink()
    {
        const std::lock_guard<std::mutex> lock(sink_mutex());
        sink() = &default_sink;
    }

    void Log::set_threshold(LogKind kind) noexcept
    {
        g_threshold.store(kind, std::memory_order_relaxed);
    }

    LogKind Log::threshold() noexcept
    {
        return g_threshold.load(std::memory_order_relaxed);
    }

    bool Log::enabled(LogKind kind) noexcept
    {
        return kind <= g_threshold.load(std::memory_order_relaxed);
    }

    void Log::write(LogKind kind, const char *function, int line, const char *format, ...)
    {
        /* Long enough for any line this library produces; the rest is cut. */
        char message[1024];

        std::va_list args;
        va_start(args, format);
        const int written = std::vsnprintf(message, sizeof(message), format, args);
        va_end(args);

        if (written < 0)
        {
            message[0] = '\0';
        }

        LogRecord record;
        record.kind = kind;
        record.function = function;
        record.line = line;
        record.message = message;

        const std::lock_guard<std::mutex> lock(sink_mutex());

        if (sink())
        {
            sink()(record);
        }
    }
} // namespace stlink
