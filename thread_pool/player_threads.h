#ifndef PLAYER_THREADS_H
#define PLAYER_THREADS_H

#include <pthread.h>

// Thread functions
void* bowler_thread(void* arg);
void* batsman_thread(void* arg);
void* fielder_thread(void* arg);

extern bool match_running;

#endif