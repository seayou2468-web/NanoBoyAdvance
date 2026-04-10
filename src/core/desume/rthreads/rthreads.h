#pragma once

#include <pthread.h>

typedef pthread_mutex_t slock_t;

static inline slock_t* slock_new(void) {
    slock_t* lock = new slock_t;
    pthread_mutex_init(lock, nullptr);
    return lock;
}

static inline void slock_free(slock_t* lock) {
    if (!lock) return;
    pthread_mutex_destroy(lock);
    delete lock;
}

static inline void slock_lock(slock_t* lock) {
    pthread_mutex_lock(lock);
}

static inline void slock_unlock(slock_t* lock) {
    pthread_mutex_unlock(lock);
}
