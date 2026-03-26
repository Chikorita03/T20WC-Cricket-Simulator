#include "umpire.h"
#include <iostream>
#include <pthread.h>

using namespace std;

extern pthread_mutex_t print_mutex;

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

            pthread_mutex_lock(&print_mutex);
            cout << "\n[PRIORITY] Death Over! Bowler " << current_bowler << " is bowling" << endl;
            pthread_mutex_unlock(&print_mutex);
        }
        return;
    }
    
    // ===== NORMAL RR =====
    if (balls_in_over >= OVER_BALLS) {

        pthread_mutex_lock(&print_mutex);
        cout << "\n[UMPIRE] Over completed." << endl;

        cout << "[Context Switch] Saving Bowler " << current_bowler << " state:" << endl;
        cout << "   Balls: " << bowlers[current_bowler].balls_bowled
             << " | Runs: " << bowlers[current_bowler].runs_conceded
             << " | Wickets: " << bowlers[current_bowler].wickets
             << endl;

        pthread_mutex_unlock(&print_mutex);

        balls_in_over = 0;
        int next = (current_bowler + 1) % NUM_BOWLERS;

        for (int i = 0; i < NUM_BOWLERS; i++) {
            int candidate = (next + i) % NUM_BOWLERS;
            // 🚫 Max 4 overs for ALL bowlers
            if (bowlers[candidate].balls_bowled >= 24)
            continue;

            // 🚫 Death bowlers: only 2 overs BEFORE death overs
            if ((candidate == death_bowler_1 || candidate == death_bowler_2) && balls_bowled < 96 && bowlers[candidate].balls_bowled >= 12) continue;
            current_bowler = candidate;
            break;
        }
        // 🔒 SAFETY FALLBACK
        if (bowlers[current_bowler].balls_bowled >= 24) {
            for (int i = 0; i < NUM_BOWLERS; i++) {
                if (bowlers[i].balls_bowled < 24) {
                    current_bowler = i;
                    break;
                }
         }
        }

        pthread_mutex_lock(&print_mutex);
        cout << "[UMPIRE] New bowler: Bowler " << current_bowler << endl;
        cout << "[Context Switch] Loading Bowler " << current_bowler << " state:" << endl;
        cout << "   Balls: " << bowlers[current_bowler].balls_bowled
             << " | Runs: " << bowlers[current_bowler].runs_conceded
             << " | Wickets: " << bowlers[current_bowler].wickets
             << endl;

        pthread_mutex_unlock(&print_mutex);
    }
}
