#ifndef PITCH_H
#define PITCH_H

#include <pthread.h>

extern pthread_mutex_t pitch_mutex;

void init_pitch();
void destroy_pitch();

#endif