/*
 * File: stlink_thread.c
 *
 * POSIX (pthreads) implementation.
 */

#include <stdint.h>
#include <stdlib.h>

#include <pthread.h>

#include <stlink_thread.h>

/*
 * pthreads wants an entry point of void *(*)(void *), which is not the shape
 * the caller supplies, so the caller's function and argument travel in a
 * context. Kept deliberately identical in structure to the Win32 side.
 */
struct stlink_thread_ctx {
    stlink_thread_fn fn;
    void *arg;
};

static void *stlink_thread_entry(void *raw) {
    struct stlink_thread_ctx ctx = *(struct stlink_thread_ctx *)raw;
    free(raw);

    ctx.fn(ctx.arg);

    return (NULL);
}

int32_t stlink_thread_create(stlink_thread_t *thread, stlink_thread_fn fn, void *arg) {
    struct stlink_thread_ctx *ctx = malloc(sizeof(*ctx));

    if(ctx == NULL) { return (-1); }

    ctx->fn = fn;
    ctx->arg = arg;

    int32_t ret = (int32_t)pthread_create(thread, NULL, stlink_thread_entry, ctx);

    if(ret != 0) { free(ctx); }

    return (ret);
}

int32_t stlink_thread_join(stlink_thread_t thread) {
    return ((int32_t)pthread_join(thread, NULL));
}
