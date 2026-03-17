#include "player_threads.h"
#include <iostream>
#include <unistd.h>
#include "../critical_section/pitch.h"

using namespace std;

bool match_running = true;

void* bowler_thread(void* arg) {
    while (match_running) {

        pthread_mutex_lock(&pitch_mutex);//entering the pitch

        cout << "[Bowler] Delivering ball" << endl;
        sleep(1);

        cout << "[Batsman] Playing shot" << endl;
        sleep(1);

        cout << "[Pitch] Ball completed\n" << endl;

        pthread_mutex_unlock(&pitch_mutex); //exiting the pitch

        sleep(1);
    }
    return NULL;
}

void* batsman_thread(void* arg) {
    int id = *(int*)arg;

    while (match_running) {
        cout << "[Batsman " << id << "] Playing shot" << endl;
        sleep(1);
    }
    return NULL;
}

void* fielder_thread(void* arg) {
    int id = *(int*)arg;

    while (match_running) {
        cout << "[Fielder " << id << "] Waiting" << endl;
        sleep(2);
    }
    return NULL;
} 

void* wicket_keeper_thread(void* arg) {
    while (match_running) {
        cout << "[Wicket Keeper] Ready behind stumps" << endl;
        sleep(1);
    }
    return NULL;
}