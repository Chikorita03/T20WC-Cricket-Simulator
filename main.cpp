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

    for (int i = 1; i <= 11; i++) {
        if (completion_time[i] == 0 && arrival_time[i] != -1) {
            completion_time[i] = balls_bowled;
            turnaround_time[i] = completion_time[i] - arrival_time[i];
        }
    }

    Logger::section("MIDDLE ORDER STRATEGY ANALYSIS (4-8)");

    float total_wt = 0;
    float total_tat = 0;
    int total_runs = 0;
    int count = 0;

    for(int i = 4; i <= 8; i++)
    {
        if(arrival_time[i] != -1)
        {
            float strike_rate = (balls_faced[i] > 0)
                ? (runs_scored[i] * 100.0f / balls_faced[i])
                : 0;

            Logger::log(
                "Batsman " + to_string(i) +
                " | WT: " + to_string(waiting_time[i]) +
                " | TAT: " + to_string(turnaround_time[i]) +
                " | Runs: " + to_string(runs_scored[i]) +
                " | SR: " + to_string(strike_rate),
                "ANALYSIS"
            );

            total_wt += waiting_time[i];
            total_tat += turnaround_time[i];
            total_runs += runs_scored[i];
            count++;
        }
    }

    Logger::log("Average Waiting Time: " + to_string(total_wt / count), "ANALYSIS");
    Logger::log("Average Turnaround Time: " + to_string(total_tat / count), "ANALYSIS");
    Logger::log("Total Runs by Middle Order: " + to_string(total_runs), "ANALYSIS");

    Logger::close();

    return 0;
}