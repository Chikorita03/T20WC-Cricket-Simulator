#include <iostream>
#include <unistd.h>
#include "thread_pool/thread_manager.h"
#include "critical_section/pitch.h"
#include "thread_pool/player_threads.h"

using namespace std;

int main() {

    init_pitch();

    create_all_threads();

    sleep(15);   // run match for 15 seconds

    extern bool match_running;
    match_running = false;

    join_all_threads();

    destroy_pitch();

    cout << "\nMatch Ended!" << endl;
    cout << "Final Score: " << global_score << endl;

    return 0;
}