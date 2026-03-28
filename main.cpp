#include <iostream>
#include "log.h"
#include <unistd.h>
#include "thread_pool/thread_manager.h"
#include "thread_pool/player_threads_2.h"
#include "critical_section_2/pitch_2.h"
#include "scheduler/umpire.h"

using namespace std;

int main() {
    Logger::init("log.txt");
    Logger::section("Match Start");

    cout<<"Select Scheduling Algorithm\n";
    cout<<"1. FCFS\n2. SJF\nEnter Choice: ";

    int choice;
    cin>>choice;

    if(choice==2){
        use_sjf=true;
        Logger::log("[SYSTEM] Using SJF Scheduling", "SCHED");
    }else{
        use_sjf=false;
        Logger::log("[SYSTEM] Using FCFS Scheduling", "SCHED");
    }

    init_pitch();

    create_all_threads();

    while (match_running) {
        usleep(10000);  // small delay to avoid busy waiting
    }

    // Signal all threads to stop
    match_running = false;
    stop_match();          // Broadcasts to unblock any waiting threads

    join_all_threads();

    destroy_pitch();

    Logger::section("Match End");

    Logger::log("Final Score: " + to_string(global_score), "SCORE");
    Logger::log("Wickets: " + to_string(wickets_fallen), "SCORE");
    Logger::log("Balls: " + to_string(balls_bowled), "SCORE");

    Logger::section("Final Bowler Stats");

    for(int i=0;i<NUM_BOWLERS;i++)
    {
        Logger::log(
            "Bowler "+to_string(i+1)+
            " | Balls: "+to_string(bowlers[i].balls_bowled)+
            " | Runs: "+to_string(bowlers[i].runs_conceded)+
            " | Wickets: "+to_string(bowlers[i].wickets),
            "BOWLER"
        );
    }

    Logger::section("Waiting Time Analysis");

    for(int i=1;i<=11;i++)
    {
        if(arrival_time[i]!=-1)
        {
            Logger::log(
                "Batsman "+to_string(i)+
                " | Waiting Time: "+to_string(waiting_time[i]),
                "BATSMAN"
            );
        }
    }

    Logger::close();

    return 0;
}