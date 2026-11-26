/* A host stand-in for libctru's <3ds/thread.h> (threadCreate, threadJoin, threadFree), on POSIX threads, so that tests/map_worker_test.c can run the
 * console's own worker code on a computer. Test use only. gShimStartDelayMs makes a thread sleep before it starts its function, to put a
 * stop request in front of a worker that has not begun. */
#ifndef CTR_SHIM_THREAD_H
#define CTR_SHIM_THREAD_H
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

#define U64_MAX UINT64_MAX
typedef void (*ThreadFunc)(void *arg);
typedef struct ShimThread {
    pthread_t id;
    ThreadFunc entry;
    void *arg;
} *Thread;

/* Shared between the test's thread and the threads it makes: every access below is atomic, so that a thread sanitizer sees only what the
 * code under test does and not the stand-in's own bookkeeping. */
static int gShimStartDelayMs;
static int gShimThreadsCreated, gShimThreadsFreed;
#define SHIM_GET(v) __atomic_load_n(&(v), __ATOMIC_ACQUIRE)
#define SHIM_SET(v, x) __atomic_store_n(&(v), (x), __ATOMIC_RELEASE)
#define SHIM_INC(v) __atomic_add_fetch(&(v), 1, __ATOMIC_ACQ_REL)

static void *ShimMain(void *p)
{
    struct ShimThread *t = p;

    int delay = SHIM_GET(gShimStartDelayMs);

    if (delay > 0) {
        struct timespec d = {delay / 1000, (long)(delay % 1000) * 1000000L};

        nanosleep(&d, NULL);
    }
    t->entry(t->arg);
    return NULL;
}

static Thread threadCreate(ThreadFunc entry, void *arg, size_t stackSize, int prio, int affinity, bool detached)
{
    struct ShimThread *t = calloc(1, sizeof(*t));

    (void)stackSize; (void)prio; (void)affinity; (void)detached;
    t->entry = entry;
    t->arg = arg;
    if (pthread_create(&t->id, NULL, ShimMain, t) != 0) {
        free(t);
        return NULL;
    }
    SHIM_INC(gShimThreadsCreated);
    return t;
}

static int threadJoin(Thread t, uint64_t timeout)
{
    (void)timeout;
    return pthread_join(t->id, NULL);
}

static void threadFree(Thread t)
{
    free(t);
    SHIM_INC(gShimThreadsFreed);
}
#endif
