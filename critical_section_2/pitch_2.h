#pragma once
#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <vector>
#include <functional>

enum WicketType {
    NONE,
    BOWLED,
    CAUGHT,
    RUN_OUT,
    LBW,
    STUMPED
};

struct BallEvent {
    bool is_wide;
    bool is_no_ball;
    bool is_free_hit;
    bool is_boundary;
    bool is_leg_bye;
    bool is_overthrow;
    bool ball_in_air;

    int base_runs;
    int extra_runs;

    WicketType wicket;
};

extern pthread_mutex_t pitch_mutex;
extern pthread_mutex_t print_mutex;
extern pthread_mutex_t score_mutex;
extern pthread_mutex_t fielder_mutex;

extern pthread_cond_t ball_delivered;
extern pthread_cond_t stroke_finished;
extern pthread_cond_t fielder_wake_cond;

extern sem_t crease_sem;

extern bool ball_ready;
extern bool stroke_done;
extern bool ball_in_air;
extern bool boundary;
extern bool is_running;
extern bool wicket_attempt;

extern int  ball_owner;
extern int  backup_fielder;
extern bool ball_stopped;

extern bool ball_active;
extern bool keeper_done;

extern int  global_score;
extern int  striker;
extern int  non_striker;
extern bool next_is_free_hit;
extern bool free_hit_pending;
extern int  balls_bowled;
extern int  wickets_fallen;
extern bool match_running;

extern pthread_mutex_t end1_mutex;
extern pthread_mutex_t end2_mutex;

extern bool striker_mid_pitch;
extern bool nonstriker_mid_pitch;

extern int striker_dist_run;
extern int nonstriker_dist_run;

extern int expected_balls[12];

// ===== SCHEDULING MODE =====
extern bool use_sjf;

// ===== FCFS Queue =====
extern std::queue<int> batting_order_fcfs;

// ===== SJF Priority Queue =====
extern std::priority_queue<
    std::pair<int,int>,
    std::vector<std::pair<int,int>>,
    std::greater<std::pair<int,int>>
> batting_order_sjf;

// ===== WAITING TIME ANALYSIS =====
extern int arrival_time[20];
extern int start_time[20];
extern int waiting_time[20];
extern bool has_started[20];
extern int balls_faced[20];
extern int runs_scored[20];
extern int completion_time[20];
extern int turnaround_time[20];

void init_pitch();
void destroy_pitch();
void update_score(int runs);
void stop_match();
BallEvent generate_event();

// ===== Wicket Validation =====
// Enforces dismissal legality based on delivery type.
// Must be called by bowler_thread() after generate_event().
void validate_wicket(BallEvent &ev);
