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

// Match state currently uses the SJF-style order seeded in pitch_2.cpp.
// Initial batsmen (FCFS starts with openers 1 and 2)
static int batsman_ids[2] = {1, 2};
static int fielder_ids[NUM_FIELDERS];

void create_all_threads() {
    // ===== Initialize FCFS Batting Order =====
    // (1 and 2 are already playing)
    while (!batting_order_fcfs.empty()) batting_order_fcfs.pop();
    for (int i = 3; i <= 11; i++) {
        batting_order_fcfs.push(i);
    }

    // ===== Initialize SJF Batting Order =====
    while (!batting_order_sjf.empty()) batting_order_sjf.pop();
    // (expected_balls, player_id)
    batting_order_sjf.push({5, 11});
    expected_balls[11]=5;
    
    batting_order_sjf.push({7, 10});
    expected_balls[10]=7;

    batting_order_sjf.push({15, 9});
    expected_balls[9]=15;

    batting_order_sjf.push({20, 8});
    expected_balls[8]=20;

    batting_order_sjf.push({25, 7});
    expected_balls[7]=25;

    batting_order_sjf.push({30, 6});
    expected_balls[6]=30;

    batting_order_sjf.push({35, 5});
    expected_balls[5]=35;

    batting_order_sjf.push({40, 4});
    expected_balls[4]=40;

    batting_order_sjf.push({50, 3});
    expected_balls[3]=50;

    Logger::log(
        "[ThreadManager] Creating " + to_string(1 + 2 + NUM_FIELDERS + 1) + " threads...",
        "SYSTEM"
    );

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
    
    death_bowler_1 = 2;
    death_bowler_2 = 3;
    
    init_scheduler();
}

void join_all_threads() {
    pthread_join(bowler, NULL);

    for (int i = 0; i < 2; i++)
        pthread_join(batsmen[i], NULL);

    for (int i = 0; i < NUM_FIELDERS; i++)
        pthread_join(fielders[i], NULL);

    pthread_join(wicket_keeper, NULL);
}
