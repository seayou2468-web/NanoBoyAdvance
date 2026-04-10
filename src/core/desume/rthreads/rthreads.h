#pragma once

#include <pthread.h>

struct slock {
    pthread_mutex_t mutex;
};
typedef slock slock_t;

static inline slock_t* slock_new(void) {
    slock_t* lock = new slock_t;
    pthread_mutex_init(&lock->mutex, nullptr);
    return lock;
}

static inline void slock_free(slock_t* lock) {
    if (!lock) return;
    pthread_mutex_destroy(&lock->mutex);
    delete lock;
}

static inline void slock_lock(slock_t* lock) {
    pthread_mutex_lock(&lock->mutex);
}

static inline void slock_unlock(slock_t* lock) {
    pthread_mutex_unlock(&lock->mutex);
}
