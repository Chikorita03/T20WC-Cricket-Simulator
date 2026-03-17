#include "player_threads.h"
#include <iostream>
#include <unistd.h>

using namespace std;

bool match_running = true;

void* bowler_thread(void* arg) {
    while (true) {
        cout << "[Bowler] Delivering ball" << endl;
        sleep(1);
    }
    return NULL;
}

void* batsman_thread(void* arg) {
    int id = *(int*)arg;

    while (true) {
        cout << "[Batsman " << id << "] Playing shot" << endl;
        sleep(1);
    }
    return NULL;
}

void* fielder_thread(void* arg) {
    int id = *(int*)arg;

    while (true) {
        cout << "[Fielder " << id << "] Waiting" << endl;
        sleep(2);
    }
    return NULL;
} 