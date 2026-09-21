/* tdx_thread.c - Win32 CRT / pthreads thread and mutex wrapper. */
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
    section = (CRITICAL_SECTION *)malloc(sizeof(*section));
    if (!section) {
        tdx_error_set(err, "out of memory for a mutex");
        return TDX_ERR;
    }
    InitializeCriticalSection(section);
    mutex->native = section;
    return TDX_OK;
}

void tdx_mutex_destroy(tdx_mutex *mutex) {
    if (!mutex || !mutex->native)
        return;
    DeleteCriticalSection((CRITICAL_SECTION *)mutex->native);
    free(mutex->native);
    mutex->native = NULL;
}

void tdx_mutex_lock(tdx_mutex *mutex) {
    if (mutex && mutex->native)
        EnterCriticalSection((CRITICAL_SECTION *)mutex->native);
}

void tdx_mutex_unlock(tdx_mutex *mutex) {
    if (mutex && mutex->native)
        LeaveCriticalSection((CRITICAL_SECTION *)mutex->native);
}

void tdx_sleep_ms(int milliseconds) {
    if (milliseconds > 0)
        Sleep((DWORD)milliseconds);
}

int64_t tdx_monotonic_ms(void) {
    static LARGE_INTEGER frequency;
    LARGE_INTEGER counter;
    if (frequency.QuadPart == 0)
        QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (int64_t)((double)counter.QuadPart * 1000.0 / (double)frequency.QuadPart);
}


int tdx_cond_init(tdx_cond *cond, tdx_error *err) {
    CONDITION_VARIABLE *variable;
    if (!cond) {
        tdx_error_set(err, "condition variable is null");
        return TDX_ERR;
    }
    variable = (CONDITION_VARIABLE *)malloc(sizeof(*variable));
    if (!variable) {
        tdx_error_set(err, "out of memory for a condition variable");
        return TDX_ERR;
    }
    InitializeConditionVariable(variable);
    cond->native = variable;
    return TDX_OK;
}

void tdx_cond_destroy(tdx_cond *cond) {
    if (!cond || !cond->native)
        return;
    free(cond->native);
    cond->native = NULL;
}

void tdx_cond_wait(tdx_cond *cond, tdx_mutex *mutex, int timeout_ms) {
    if (!cond || !cond->native || !mutex || !mutex->native)
        return;
    if (timeout_ms < 0)
        timeout_ms = INFINITE;
    SleepConditionVariableCS((CONDITION_VARIABLE *)cond->native,
                             (CRITICAL_SECTION *)mutex->native, (DWORD)timeout_ms);
}

void tdx_cond_signal(tdx_cond *cond) {
    if (cond && cond->native)
        WakeConditionVariable((CONDITION_VARIABLE *)cond->native);
}

void tdx_cond_broadcast(tdx_cond *cond) {
    if (cond && cond->native)
        WakeAllConditionVariable((CONDITION_VARIABLE *)cond->native);
}


void tdx_thread_detach(tdx_thread *thread) {
    if (!thread || !thread->handle)
        return;
    CloseHandle((HANDLE)thread->handle);
    thread->handle = NULL;
}

#else /* POSIX */

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
    if (pthread_mutex_init(&mutex->native, NULL) != 0) {
        tdx_error_set(err, "cannot create a mutex");
        return TDX_ERR;
    }
    return TDX_OK;
}

void tdx_mutex_destroy(tdx_mutex *mutex) {
    if (mutex)
        pthread_mutex_destroy(&mutex->native);
}

void tdx_mutex_lock(tdx_mutex *mutex) {
    if (mutex)
        pthread_mutex_lock(&mutex->native);
}

void tdx_mutex_unlock(tdx_mutex *mutex) {
    if (mutex)
        pthread_mutex_unlock(&mutex->native);
}

void tdx_sleep_ms(int milliseconds) {
    if (milliseconds > 0)
        usleep((useconds_t)milliseconds * 1000);
}

int64_t tdx_monotonic_ms(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}


int tdx_cond_init(tdx_cond *cond, tdx_error *err) {
    if (!cond) {
        tdx_error_set(err, "condition variable is null");
        return TDX_ERR;
    }
    if (pthread_cond_init(&cond->native, NULL) != 0) {
        tdx_error_set(err, "cannot create a condition variable");
        return TDX_ERR;
    }
    return TDX_OK;
}

void tdx_cond_destroy(tdx_cond *cond) {
    if (cond)
        pthread_cond_destroy(&cond->native);
}

void tdx_cond_wait(tdx_cond *cond, tdx_mutex *mutex, int timeout_ms) {
    struct timespec deadline;
    if (!cond || !mutex)
        return;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeout_ms / 1000;
    deadline.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }
    pthread_cond_timedwait(&cond->native, &mutex->native, &deadline);
}

void tdx_cond_signal(tdx_cond *cond) {
    if (cond)
        pthread_cond_signal(&cond->native);
}

void tdx_cond_broadcast(tdx_cond *cond) {
    if (cond)
        pthread_cond_broadcast(&cond->native);
}


void tdx_thread_detach(tdx_thread *thread) {
    if (!thread || !thread->started)
        return;
    pthread_detach(thread->handle);
    thread->started = 0;
}

#endif
