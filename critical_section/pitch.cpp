#include "pitch.h"

pthread_mutex_t pitch_mutex;

void init_pitch() {
    pthread_mutex_init(&pitch_mutex, NULL);
}

void destroy_pitch() {
    pthread_mutex_destroy(&pitch_mutex);
}