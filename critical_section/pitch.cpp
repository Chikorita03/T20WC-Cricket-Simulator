#include "pitch.h"

pthread_mutex_t pitch_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t ball_delivered = PTHREAD_COND_INITIALIZER;
pthread_cond_t stroke_finished  = PTHREAD_COND_INITIALIZER;

bool ball_ready = false;
bool stroke_done  = true;

void init_pitch() {
    pthread_mutex_init(&pitch_mutex, NULL);
}

void destroy_pitch() {
    pthread_mutex_destroy(&pitch_mutex);
}