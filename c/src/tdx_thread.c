/* tdx_thread.c - Win32 CRT / pthreads thread and mutex wrapper. */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "tdx_thread.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#include <process.h>

typedef struct thread_trampoline {
    tdx_thread_fn entry;
    void *context;
} thread_trampoline;

static unsigned __stdcall tdx_thread_entry(void *raw) {
    thread_trampoline trampoline = *(thread_trampoline *)raw;
    free(raw);
    trampoline.entry(trampoline.context);
    return 0;
}

int tdx_thread_start(tdx_thread *thread, tdx_thread_fn entry, void *context,
                     tdx_error *err) {
    thread_trampoline *trampoline;
    uintptr_t handle;

    if (!thread || !entry) {
        tdx_error_set(err, "thread start needs a handle and an entry point");
        return TDX_ERR;
    }
    trampoline = (thread_trampoline *)malloc(sizeof(*trampoline));
    if (!trampoline) {
        tdx_error_set(err, "out of memory for a thread trampoline");
        return TDX_ERR;
    }
    trampoline->entry = entry;
    trampoline->context = context;
    handle = _beginthreadex(NULL, 0, tdx_thread_entry, trampoline, 0, &thread->id);
    if (handle == 0) {
        free(trampoline);
        tdx_error_set(err, "cannot start a worker thread");
        return TDX_ERR;
    }
    thread->handle = (void *)handle;
    return TDX_OK;
}

void tdx_thread_join(tdx_thread *thread) {
    if (!thread || !thread->handle)
        return;
    WaitForSingleObject((HANDLE)thread->handle, INFINITE);
    CloseHandle((HANDLE)thread->handle);
    thread->handle = NULL;
}

int tdx_mutex_init(tdx_mutex *mutex, tdx_error *err) {
    CRITICAL_SECTION *section;
    if (!mutex) {
        tdx_error_set(err, "mutex is null");
        return TDX_ERR;
    }
    mutex->initialized = 0;
    mutex->native = NULL;
    section = (CRITICAL_SECTION *)malloc(sizeof(*section));
    if (!section) {
        tdx_error_set(err, "out of memory for a mutex");
        return TDX_ERR;
    }
    InitializeCriticalSection(section);
    mutex->native = section;
    mutex->initialized = 1;
    return TDX_OK;
}

void tdx_mutex_destroy(tdx_mutex *mutex) {
    if (!tdx_mutex_is_initialized(mutex))
        return;
    DeleteCriticalSection((CRITICAL_SECTION *)mutex->native);
    free(mutex->native);
    mutex->native = NULL;
    mutex->initialized = 0;
}

void tdx_mutex_lock(tdx_mutex *mutex) {
    if (tdx_mutex_is_initialized(mutex))
        EnterCriticalSection((CRITICAL_SECTION *)mutex->native);
}

void tdx_mutex_unlock(tdx_mutex *mutex) {
    if (tdx_mutex_is_initialized(mutex))
        LeaveCriticalSection((CRITICAL_SECTION *)mutex->native);
}

void tdx_sleep_ms(int milliseconds) {
    if (milliseconds > 0)
        Sleep((DWORD)milliseconds);
}

int64_t tdx_monotonic_ms(void) {
    return (int64_t)GetTickCount64();
}


int tdx_cond_init(tdx_cond *cond, tdx_error *err) {
    CONDITION_VARIABLE *variable;
    if (!cond) {
        tdx_error_set(err, "condition variable is null");
        return TDX_ERR;
    }
    cond->initialized = 0;
    cond->native = NULL;
    variable = (CONDITION_VARIABLE *)malloc(sizeof(*variable));
    if (!variable) {
        tdx_error_set(err, "out of memory for a condition variable");
        return TDX_ERR;
    }
    InitializeConditionVariable(variable);
    cond->native = variable;
    cond->initialized = 1;
    return TDX_OK;
}

void tdx_cond_destroy(tdx_cond *cond) {
    if (!tdx_cond_is_initialized(cond))
        return;
    free(cond->native);
    cond->native = NULL;
    cond->initialized = 0;
}

void tdx_cond_wait(tdx_cond *cond, tdx_mutex *mutex, int timeout_ms) {
    if (!tdx_cond_is_initialized(cond) || !tdx_mutex_is_initialized(mutex))
        return;
    if (timeout_ms < 0)
        timeout_ms = INFINITE;
    SleepConditionVariableCS((CONDITION_VARIABLE *)cond->native,
                             (CRITICAL_SECTION *)mutex->native, (DWORD)timeout_ms);
}

void tdx_cond_signal(tdx_cond *cond) {
    if (tdx_cond_is_initialized(cond))
        WakeConditionVariable((CONDITION_VARIABLE *)cond->native);
}

void tdx_cond_broadcast(tdx_cond *cond) {
    if (tdx_cond_is_initialized(cond))
        WakeAllConditionVariable((CONDITION_VARIABLE *)cond->native);
}


void tdx_thread_detach(tdx_thread *thread) {
    if (!thread || !thread->handle)
        return;
    CloseHandle((HANDLE)thread->handle);
    thread->handle = NULL;
}

#else /* POSIX */

#include <errno.h>
#include <time.h>
#include <unistd.h>

typedef struct thread_trampoline {
    tdx_thread_fn entry;
    void *context;
} thread_trampoline;

static void *tdx_thread_entry(void *raw) {
    thread_trampoline trampoline = *(thread_trampoline *)raw;
    free(raw);
    trampoline.entry(trampoline.context);
    return NULL;
}

int tdx_thread_start(tdx_thread *thread, tdx_thread_fn entry, void *context,
                     tdx_error *err) {
    thread_trampoline *trampoline;
    if (!thread || !entry) {
        tdx_error_set(err, "thread start needs a handle and an entry point");
        return TDX_ERR;
    }
    trampoline = (thread_trampoline *)malloc(sizeof(*trampoline));
    if (!trampoline) {
        tdx_error_set(err, "out of memory for a thread trampoline");
        return TDX_ERR;
    }
    trampoline->entry = entry;
    trampoline->context = context;
    if (pthread_create(&thread->handle, NULL, tdx_thread_entry, trampoline) != 0) {
        free(trampoline);
        tdx_error_set(err, "cannot start a worker thread");
        return TDX_ERR;
    }
    thread->started = 1;
    return TDX_OK;
}

void tdx_thread_join(tdx_thread *thread) {
    if (!thread || !thread->started)
        return;
    pthread_join(thread->handle, NULL);
    thread->started = 0;
}

int tdx_mutex_init(tdx_mutex *mutex, tdx_error *err) {
    if (!mutex) {
        tdx_error_set(err, "mutex is null");
        return TDX_ERR;
    }
    mutex->initialized = 0;
    if (pthread_mutex_init(&mutex->native, NULL) != 0) {
        tdx_error_set(err, "cannot create a mutex");
        return TDX_ERR;
    }
    mutex->initialized = 1;
    return TDX_OK;
}

void tdx_mutex_destroy(tdx_mutex *mutex) {
    if (tdx_mutex_is_initialized(mutex)) {
        pthread_mutex_destroy(&mutex->native);
        mutex->initialized = 0;
    }
}

void tdx_mutex_lock(tdx_mutex *mutex) {
    if (tdx_mutex_is_initialized(mutex))
        pthread_mutex_lock(&mutex->native);
}

void tdx_mutex_unlock(tdx_mutex *mutex) {
    if (tdx_mutex_is_initialized(mutex))
        pthread_mutex_unlock(&mutex->native);
}

void tdx_sleep_ms(int milliseconds) {
    struct timespec remaining;
    if (milliseconds <= 0)
        return;
    remaining.tv_sec = milliseconds / 1000;
    remaining.tv_nsec = (long)(milliseconds % 1000) * 1000000L;
    while (nanosleep(&remaining, &remaining) != 0 && errno == EINTR) {}
}

int64_t tdx_monotonic_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}


int tdx_cond_init(tdx_cond *cond, tdx_error *err) {
    pthread_condattr_t attributes;
    int result;
    if (!cond) {
        tdx_error_set(err, "condition variable is null");
        return TDX_ERR;
    }
    cond->initialized = 0;
    cond->monotonic = 0;
    if (pthread_condattr_init(&attributes) != 0) {
        tdx_error_set(err, "cannot create condition attributes");
        return TDX_ERR;
    }
#if defined(_POSIX_CLOCK_SELECTION) && _POSIX_CLOCK_SELECTION >= 0 && !defined(__APPLE__)
    if (pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC) == 0)
        cond->monotonic = 1;
#endif
    result = pthread_cond_init(&cond->native, &attributes);
    pthread_condattr_destroy(&attributes);
    if (result != 0) {
        tdx_error_set(err, "cannot create a condition variable");
        return TDX_ERR;
    }
    cond->initialized = 1;
    return TDX_OK;
}

void tdx_cond_destroy(tdx_cond *cond) {
    if (tdx_cond_is_initialized(cond)) {
        pthread_cond_destroy(&cond->native);
        cond->initialized = 0;
    }
}

void tdx_cond_wait(tdx_cond *cond, tdx_mutex *mutex, int timeout_ms) {
    struct timespec deadline;
    if (!tdx_cond_is_initialized(cond) || !tdx_mutex_is_initialized(mutex))
        return;
    if (timeout_ms < 0) {
        pthread_cond_wait(&cond->native, &mutex->native);
        return;
    }
    clock_gettime(cond->monotonic ? CLOCK_MONOTONIC : CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    pthread_cond_timedwait(&cond->native, &mutex->native, &deadline);
}

void tdx_cond_signal(tdx_cond *cond) {
    if (tdx_cond_is_initialized(cond))
        pthread_cond_signal(&cond->native);
}

void tdx_cond_broadcast(tdx_cond *cond) {
    if (tdx_cond_is_initialized(cond))
        pthread_cond_broadcast(&cond->native);
}


void tdx_thread_detach(tdx_thread *thread) {
    if (!thread || !thread->started)
        return;
    pthread_detach(thread->handle);
    thread->started = 0;
}

#endif

int tdx_mutex_is_initialized(const tdx_mutex *mutex) {
    return mutex && mutex->initialized;
}

int tdx_cond_is_initialized(const tdx_cond *cond) {
    return cond && cond->initialized;
}
