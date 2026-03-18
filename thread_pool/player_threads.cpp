#include "player_threads.h"
#include <iostream>
#include <unistd.h>
#include "../critical_section/pitch.h"

using namespace std;

bool match_running = true;

void* bowler_thread(void* arg) {
    while (match_running) {

        pthread_mutex_lock(&pitch_mutex); // entering pitch

        cout << "[Bowler] Delivering ball" << endl;
        sleep(1);

        ball_ready = true;
        stroke_done = false;

        pthread_cond_signal(&ball_delivered);

        while (!stroke_done) {
            pthread_cond_wait(&stroke_finished, &pitch_mutex);
        }

        cout << "[Pitch] Ball completed\n" << endl;

        pthread_mutex_unlock(&pitch_mutex); // exiting pitch

        sleep(1);
    }
    return NULL;
}

void* batsman_thread(void* arg) {
    int id = *(int*)arg;

    while (match_running) {

        pthread_mutex_lock(&pitch_mutex);

        while (!ball_ready) {
            pthread_cond_wait(&ball_delivered, &pitch_mutex);
        }

        cout << "[Batsman " << id << "] Playing shot" << endl;
        sleep(1);

        int runs = generate_runs();
        update_score(runs);   
        
        stroke_done = true;
        ball_ready = false;

        pthread_cond_signal(&stroke_finished);
        pthread_mutex_unlock(&pitch_mutex);
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