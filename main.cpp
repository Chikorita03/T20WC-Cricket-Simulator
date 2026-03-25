#include <iostream>
#include <unistd.h>
#include "thread_pool/thread_manager.h"
#include "thread_pool/player_threads_2.h"
#include "critical_section_2/pitch_2.h"

using namespace std;

int main() {
    cout << "=== T20 Cricket Simulator ===" << endl;

    init_pitch();

    create_all_threads();

    sleep(30);   // Run match for 30 seconds (~several overs)

    // Signal all threads to stop
    match_running = false;
    stop_match();          // Broadcasts to unblock any waiting threads

    join_all_threads();

    destroy_pitch();

    cout << "\n=== Match Ended ===" << endl;
    cout << "Final Score   : " << global_score   << " runs" << endl;
    cout << "Wickets Fallen: " << wickets_fallen << endl;
    cout << "Balls Bowled  : " << balls_bowled   << endl;

    return 0;
}
