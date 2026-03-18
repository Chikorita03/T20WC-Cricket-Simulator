#ifndef PITCH_H
#define PITCH_H

#include <pthread.h>

extern pthread_mutex_t pitch_mutex;

extern pthread_cond_t ball_delivered;
extern pthread_cond_t stroke_finished;

extern bool ball_ready;
extern bool stroke_done;

extern int global_score;
extern pthread_mutex_t score_mutex;

void init_pitch();
void destroy_pitch();
int generate_runs();
void update_score(int runs);

#endif