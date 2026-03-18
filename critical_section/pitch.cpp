#include "pitch.h"
#include <iostream>
#include <cstdlib>

using namespace std;

pthread_mutex_t pitch_mutex;
pthread_mutex_t print_mutex;

pthread_cond_t ball_delivered;
pthread_cond_t stroke_finished;
bool ball_ready = false;
bool stroke_done = true;

int global_score = 0;
pthread_mutex_t score_mutex;

void init_pitch() {
    pthread_mutex_init(&pitch_mutex, NULL);
    pthread_mutex_init(&print_mutex, NULL);

    pthread_cond_init(&ball_delivered, NULL);
    pthread_cond_init(&stroke_finished, NULL);

    pthread_mutex_init(&score_mutex, NULL);
}

void destroy_pitch() {
    pthread_mutex_destroy(&pitch_mutex);
    pthread_mutex_destroy(&print_mutex);
    
    pthread_cond_destroy(&ball_delivered);
    pthread_cond_destroy(&stroke_finished);

    pthread_mutex_destroy(&score_mutex);
}

void update_score(int runs) {
    int new_score;

    pthread_mutex_lock(&score_mutex);
    global_score += runs;
    new_score = global_score;
    pthread_mutex_unlock(&score_mutex);

    pthread_mutex_lock(&print_mutex);
    cout << "[Score Updated] Total Score: " << new_score << endl;
    pthread_mutex_unlock(&print_mutex);
}

int generate_runs() {
    int r = rand() % 100;  

    if (r < 30) return 0;      
    else if (r < 55) return 1; 
    else if (r < 70) return 2; 
    else if (r < 80) return 3;
    else if (r < 95) return 4; 
    else return 6;             
}