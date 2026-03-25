#pragma once

#define NUM_BOWLERS 4
#define OVER_BALLS 6

// Scheduler state
extern int current_bowler;
extern int balls_in_over;

// API
void init_scheduler();
void on_ball_completed();   // called after valid ball
int get_current_bowler();