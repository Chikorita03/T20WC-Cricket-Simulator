#include <iostream>
#include <unistd.h>
#include "thread_pool/thread_manager.h"
#include "thread_pool/player_threads_2.h"
#include "critical_section_2/pitch_2.h"
#include "scheduler/umpire.h"

using namespace std;

int main() {
    cout << "=== T20 Cricket Simulator ===" << endl;

    init_pitch();

    create_all_threads();

    while (balls_bowled < 120) {
        usleep(10000);  // small delay to avoid busy waiting
    }

    // Signal all threads to stop
    match_running = false;
    stop_match();          // Broadcasts to unblock any waiting threads

    join_all_threads();

    destroy_pitch();

    cout << "\n=== Match Ended ===" << endl;
    cout << "Final Score   : " << global_score   << " runs" << endl;
    cout << "Wickets Fallen: " << wickets_fallen << endl;
    cout << "Balls Bowled  : " << balls_bowled   << endl;

    cout << "\n===== FINAL BOWLER STATS (RR Scheduler) =====" << endl;
    for (int i = 0; i < NUM_BOWLERS; i++) {
        cout << "Bowler " << i
         << " | Balls: " << bowlers[i].balls_bowled
         << " | Runs: " << bowlers[i].runs_conceded
         << " | Wickets: " << bowlers[i].wickets
         << endl;
    }

    cout << "\n===== WAITING TIME ANALYSIS =====\n";

    for (int i = 1; i <= 11; i++) {
        if (arrival_time[i] != -1) {
        cout << "Batsman " << i
             << " | Waiting Time: " << waiting_time[i]
             << endl;
        }
    }
    return 0;
}
