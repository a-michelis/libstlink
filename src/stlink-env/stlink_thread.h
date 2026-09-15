/*
 * File: stlink_thread.h
 *
 *
 */

#ifndef STLINK_THREAD_H
#define STLINK_THREAD_H

#include <stdint.h>

/*
 * Minimal thread wrapper.
 *
 * The library only ever starts a handful of workers and waits for them, which
 * Win32 and pthreads both provide directly. Wrapping those two calls removes
 * the last reason for a Windows build to carry a pthreads implementation:
 * PThreads4W on MSVC, or winpthread on MinGW, the latter being a DLL that the
 * produced binaries would otherwise have to ship beside them.
 */

#if defined(_WIN32)
/* A HANDLE, held as void* so that windows.h stays out of this header. */
typedef void *stlink_thread_t;
#else
#include <pthread.h>
typedef pthread_t stlink_thread_t;
#endif

/*
 * A thread entry point. It deliberately returns nothing: Win32 and pthreads
 * disagree on both the return type and the calling convention, and nothing
 * here reads a thread's result.
 */
typedef void (*stlink_thread_fn)(void *arg);

/**
 * @brief Start a thread running fn(arg).
 * @return 0 on success, otherwise the platform error code
 */
int32_t stlink_thread_create(stlink_thread_t *thread, stlink_thread_fn fn, void *arg);

/**
 * @brief Wait for a thread to finish, then release it.
 * @return 0 on success, otherwise the platform error code
 */
int32_t stlink_thread_join(stlink_thread_t thread);

#endif // STLINK_THREAD_H
