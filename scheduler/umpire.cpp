#include "umpire.h"
#include <iostream>
#include "log.h"
#include <sstream>
#include <pthread.h>

using namespace std;

int current_bowler = 0;
int balls_in_over  = 0;
int death_bowler_1 = 3;
int death_bowler_2 = 4;
extern int balls_bowled;

// ===== ROUND ROBIN: Bowler PCBs =====
BowlerPCB bowlers[NUM_BOWLERS];

void init_scheduler() {
    current_bowler = 0;
    balls_in_over  = 0;

    // ===== Initialize Bowler PCBs =====
for (int i = 0; i < NUM_BOWLERS; i++) {
    bowlers[i].runs_conceded = 0;
    bowlers[i].balls_bowled = 0;
    bowlers[i].wickets = 0;
}
}

int get_current_bowler() {
    return current_bowler;
}

// =============================================================
// decide_lbw() — declared here so umpire.h can expose it.
//
// Implementation lives in player_threads_2.cpp where it is
// called from batsman_thread().  This forward-declaration in
// umpire.cpp keeps the scheduling module aware of the LBW
// adjudication function without duplicating logic.
//
// OS analogy: The umpire is the kernel scheduler.  decide_lbw()
// is a privileged system call — only the "kernel" (umpire) may
// authoritatively resolve an LBW appeal, and batsman_thread()
// invokes it like a syscall, blocking until a verdict is returned.
// =============================================================
// (decide_lbw body is in player_threads_2.cpp; see umpire.h
//  for the extern declaration used across translation units.)

void on_ball_completed() {

    balls_in_over++;

    // ===== PRIORITY: LAST 4 OVERS =====
    if (balls_bowled >= 96) {

        if (balls_in_over >= OVER_BALLS) {

            balls_in_over = 0;

            static int toggle = 0;

            if (toggle == 0)
                current_bowler = death_bowler_1;
            else
                current_bowler = death_bowler_2;

            toggle = 1 - toggle;

            Logger::log(
                "[PRIORITY] Death Over! Bowler "+to_string(current_bowler+1)+" is bowling",
                "UMPIRE"
            );
                    }
        return;
    }
    
    