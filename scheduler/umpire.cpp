#include "umpire.h"
#include <iostream>
#include <pthread.h>

using namespace std;

extern pthread_mutex_t print_mutex;

int current_bowler = 0;
int balls_in_over  = 0;

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
        current_bowler = (current_bowler + 1) % NUM_BOWLERS;

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
