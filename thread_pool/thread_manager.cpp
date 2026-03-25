#include "thread_manager.h"
#include "player_threads_2.h"
#include "../critical_section_2/pitch_2.h"
#include <iostream>

using namespace std;

#define NUM_FIELDERS 9

static pthread_t bowler;
static pthread_t batsmen[2];
static pthread_t fielders[NUM_FIELDERS];
static pthread_t wicket_keeper;

static int batsman_ids[2] = {1, 2};
static int fielder_ids[NUM_FIELDERS];

void create_all_threads() {
    pthread_mutex_lock(&print_mutex);
    cout << "[ThreadManager] Creating " << (1 + 2 + NUM_FIELDERS + 1)
         << " threads..." << endl;
    pthread_mutex_unlock(&print_mutex);

    // Bowler — owns the pitch for one full ball lifecycle
    pthread_create(&bowler, NULL, bowler_thread, NULL);

    // Two batsmen — only the striker (id == striker) plays each ball
    for (int i = 0; i < 2; i++)
        pthread_create(&batsmen[i], NULL, batsman_thread, &batsman_ids[i]);

    // Nine fielders — sleep until ball_in_air, then react
    for (int i = 0; i < NUM_FIELDERS; i++) {
        fielder_ids[i] = i + 1;
        pthread_create(&fielders[i], NULL, fielder_thread, &fielder_ids[i]);
    }

    // Wicket keeper — like a fielder + handles stumping
    pthread_create(&wicket_keeper, NULL, wicket_keeper_thread, NULL);
}

void join_all_threads() {
    pthread_join(bowler, NULL);

    for (int i = 0; i < 2; i++)
        pthread_join(batsmen[i], NULL);

    for (int i = 0; i < NUM_FIELDERS; i++)
        pthread_join(fielders[i], NULL);

    pthread_join(wicket_keeper, NULL);
}
