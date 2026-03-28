#pragma once
#include <pthread.h>

void* bowler_thread(void* arg);
void* batsman_thread(void* arg);
void* fielder_thread(void* arg);
void* wicket_keeper_thread(void* arg);
void reset_batting_progress();

const char* wicket_name(int wt);
