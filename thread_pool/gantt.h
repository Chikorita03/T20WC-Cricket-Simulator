#pragma once
#include <vector>
#include <pthread.h>

using namespace std;

struct GanttEvent {
    int ball; 
    int bowler;
    int striker;
    int non_striker;
};

extern vector<GanttEvent> gantt_log;
extern pthread_mutex_t gantt_mutex;

void log_gantt(int ball, int bowler, int striker, int non_striker);
void print_gantt_chart();
void clear_gantt_chart();
