/* tdx_thread.h - the smallest threading surface the streamer needs.
 *
 * A worker pool and an SSE server both need to run functions on their own
 * stacks.  Rather than pull in a framework we expose exactly one thread
 * primitive and one recursive-free mutex, implemented with the Win32 CRT on
 * Windows and pthreads elsewhere. */
#ifndef TDX_THREAD_H
#define TDX_THREAD_H

#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tdx_thread_fn)(void *context);

#if defined(_WIN32)
typedef struct tdx_thread {
    void *handle;
    unsigned id;
} tdx_thread;
typedef struct tdx_mutex {
    void *native; /* CRITICAL_SECTION* */
} tdx_mutex;
typedef struct tdx_cond {
    void *native; /* CONDITION_VARIABLE* */
} tdx_cond;
#else
#include <pthread.h>
typedef struct tdx_thread {
    pthread_t handle;
    int started;
} tdx_thread;
typedef struct tdx_mutex {
    pthread_mutex_t native;
} tdx_mutex;
typedef struct tdx_cond {
    pthread_cond_t native;
} tdx_cond;
#endif

int tdx_thread_start(tdx_thread *thread, tdx_thread_fn entry, void *context,
                     tdx_error *err);
/* Blocks until the thread returns.  Safe to call once per started thread. */
void tdx_thread_join(tdx_thread *thread);
/* Releases the handle so the thread runs to completion on its own. */
void tdx_thread_detach(tdx_thread *thread);

int tdx_mutex_init(tdx_mutex *mutex, tdx_error *err);
void tdx_mutex_destroy(tdx_mutex *mutex);
void tdx_mutex_lock(tdx_mutex *mutex);
void tdx_mutex_unlock(tdx_mutex *mutex);

int tdx_cond_init(tdx_cond *cond, tdx_error *err);
void tdx_cond_destroy(tdx_cond *cond);
/* Waits until signalled or timeout_ms elapses.  The mutex must be held. */
void tdx_cond_wait(tdx_cond *cond, tdx_mutex *mutex, int timeout_ms);
void tdx_cond_signal(tdx_cond *cond);
void tdx_cond_broadcast(tdx_cond *cond);

/* Cooperative sleep, used for reconnect backoff. */
void tdx_sleep_ms(int milliseconds);

/* Monotonic milliseconds since an arbitrary origin. */
int64_t tdx_monotonic_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* TDX_THREAD_H */
