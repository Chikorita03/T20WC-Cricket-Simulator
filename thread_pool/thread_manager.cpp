#include "thread_manager.h"
#include "log.h"
#include "player_threads_2.h"
#include "../critical_section_2/pitch_2.h"
#include <iostream>
#include "../scheduler/umpire.h"

using namespace std;

#define NUM_FIELDERS 9

static pthread_t bowler;
static pthread_t batsmen[2];
static pthread_t fielders[NUM_FIELDERS];
static pthread_t wicket_keeper;

//opening batsmen are 1 and 2
static int batsman_ids[2] = {1, 2};
static int fielder_ids[NUM_FIELDERS];

void create_all_threads() {
    //FCFS: only middle order (4–8)
    while (!batting_order_fcfs.empty()) batting_order_fcfs.pop();
    for (int i = 4; i <= 8; i++) {
        batting_order_fcfs.push(i);
    }

    //SJF: only middle order (4–8)
    while (!batting_order_sjf.empty()) batting_order_sjf.pop();

    //(expected_balls, player_id)
    batting_order_sjf.push({20, 4});
    expected_balls[4] = 20;

    batting_order_sjf.push({15, 5});
    expected_balls[5] = 15;

    batting_order_sjf.push({10, 6});
    expected_balls[6] = 10;

    batting_order_sjf.push({8, 7});
    expected_balls[7] = 8;

    batting_order_sjf.push({5, 8});
    expected_balls[8] = 5;
    
    Logger::log(
        "[ThreadManager] Creating " + to_string(1 + 2 + NUM_FIELDERS + 1) + " threads...",
        "SYSTEM"
    );

    //internal indices are 0-based: 2/3 -> displayed bowlers in the outputs: 3/4
    death_bowler_1 = 2;
    death_bowler_2 = 3;
    init_scheduler();

    //bowler - owns the pitch for one full ball lifecycle
    pthread_create(&bowler, NULL, bowler_thread, NULL);

    //two batsmen — only the striker plays each ball
    for (int i = 0; i < 2; i++)
        pthread_create(&batsmen[i], NULL, batsman_thread, &batsman_ids[i]);
    
    //nine fielders — sleep until ball_in_air, then react
    for (int i = 0; i < NUM_FIELDERS; i++) {
        fielder_ids[i] = i + 1;
        pthread_create(&fielders[i], NULL, fielder_thread, &fielder_ids[i]);
    }

    //wicket keeper - like a fielder and also handles stumping
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
