#include "thread_manager.h"
#include "player_threads.h"
#include <iostream>

using namespace std;

#define NUM_FIELDERS 10

pthread_t bowler;
pthread_t batsmen[2];
pthread_t fielders[NUM_FIELDERS];

int batsman_ids[2] = {1, 2};
int fielder_ids[NUM_FIELDERS];

void create_all_threads() {

    pthread_create(&bowler, NULL, bowler_thread, NULL);

    for (int i = 0; i < 2; i++) {
        pthread_create(&batsmen[i], NULL, batsman_thread, &batsman_ids[i]);
    }

    for (int i = 0; i < NUM_FIELDERS; i++) {
        fielder_ids[i] = i + 1;
        pthread_create(&fielders[i], NULL, fielder_thread, &fielder_ids[i]);
    }
    cout << "Threads initialized."<<endl;
}

void join_all_threads() {
    pthread_join(bowler, NULL);

    for (int i = 0; i < 2; i++) {
        pthread_join(batsmen[i], NULL);
    }

    for (int i = 0; i < NUM_FIELDERS; i++) {
        pthread_join(fielders[i], NULL);
    }
}