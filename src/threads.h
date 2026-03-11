#ifndef THREADS_H
#define THREADS_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

typedef struct {
    int pending;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} Semaphore;

typedef struct {
    int closed;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} Gate;

void my_semaphore_wait(Semaphore *sem);
void my_semaphore_increment(Semaphore *sem);

void gate_wait(Gate *gate);
void gate_open(Gate *gate);
void gate_close(Gate *gate);

#endif // THREADS_H

#ifdef THREADS_IMPL
#ifndef THREADS_IMPL_GUARD
#define THREADS_IMPL_GUARD

void my_semaphore_wait(Semaphore *sem) {
    pthread_mutex_lock(&sem->mutex);
    while (!sem->pending) pthread_cond_wait(&sem->cond, &sem->mutex);
    sem->pending -= 1;
    pthread_mutex_unlock(&sem->mutex);
}

void my_semaphore_increment(Semaphore *sem) {
    pthread_mutex_lock(&sem->mutex);
    sem->pending += 1;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
}

bool gate_is_closed(Gate *gate) {
    bool ret = 0;
    pthread_mutex_lock(&gate->mutex);
    ret = gate->closed;
    pthread_mutex_unlock(&gate->mutex);
    return ret;
}

void gate_wait(Gate *gate) {
    pthread_mutex_lock(&gate->mutex);
    while (gate->closed)
        pthread_cond_wait(&gate->cond, &gate->mutex);
    pthread_mutex_unlock(&gate->mutex);
}

void gate_open(Gate *gate) {
    pthread_mutex_lock(&gate->mutex);
    gate->closed = 0;
    pthread_cond_signal(&gate->cond);
    pthread_mutex_unlock(&gate->mutex);
}

void gate_close(Gate *gate) {
    pthread_mutex_lock(&gate->mutex);
    gate->closed = 1;
    pthread_mutex_unlock(&gate->mutex);
}

#endif // THREADS_IMPL_GUARD
#endif // THREADS_IMPL
