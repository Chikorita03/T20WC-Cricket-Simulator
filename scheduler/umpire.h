#pragma once

#define NUM_BOWLERS 4
#define OVER_BALLS 6

// Scheduler state
extern int current_bowler;
extern int balls_in_over;

// ===== ROUND ROBIN SCHEDULING =====
// Each bowler is treated as a process.
// BowlerPCB represents Process Control Block (PCB)
struct BowlerPCB {
    int runs_conceded;
    int balls_bowled;
    int wickets;
};

// Array of PCBs (one per bowler)
extern BowlerPCB bowlers[NUM_BOWLERS];

// API
void init_scheduler();
void on_ball_completed();   // called after valid ball
int get_current_bowler();