#include "pitch_2.h"
#include "log.h"
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <queue>
#include <vector>
#include <functional>
#include "../thread_pool/player_threads_2.h"
#include "../scheduler/umpire.h"

using namespace std;

pthread_mutex_t pitch_mutex;
pthread_mutex_t print_mutex;
pthread_mutex_t score_mutex;
pthread_mutex_t fielder_mutex;

pthread_cond_t ball_delivered;
pthread_cond_t stroke_finished;
pthread_cond_t fielder_wake_cond;

sem_t crease_sem;

bool ball_ready     = false;
bool stroke_done    = true;
bool ball_in_air    = false;
bool boundary       = false;
bool is_running     = false;
bool wicket_attempt = false;

int  ball_owner     = -1;
int  backup_fielder = -1;
bool ball_stopped   = false;

bool ball_active  = false;
bool keeper_done  = false;

int  global_score     = 0;
int  striker          = 1;
int  non_striker      = 2;
bool next_is_free_hit = false;
bool free_hit_pending = false;
int  balls_bowled     = 0;
int  wickets_fallen   = 0;
bool match_running    = true;

int innings = 1;
int target_score = 0;
bool innings_break = false;

float required_run_rate = 0;
float current_run_rate = 0;
int extras_total = 0;
int extras_wides = 0;
int extras_no_balls = 0;
int extras_byes = 0;
int extras_leg_byes = 0;

pthread_mutex_t end1_mutex;   // Striker's crease
pthread_mutex_t end2_mutex;   // Non-striker's crease

int striker_dist_run = 0;
int nonstriker_dist_run = 0;

bool striker_mid_pitch = false;
bool nonstriker_mid_pitch = false;

int expected_balls[12];

// ===== Scheduling Mode =====
bool use_sjf = false;   // Set to true for SJF, false for FCFS

// ===== Batting Orders =====
queue<int> batting_order_fcfs;

priority_queue< pair<int,int>, vector<pair<int,int>>, greater<pair<int,int>>> batting_order_sjf;

// ===== Waiting Time Tracking =====
int arrival_time[20];
int start_time[20];
int waiting_time[20];
bool has_started[20];
int balls_faced[20];
int runs_scored[20];
int completion_time[20];
int turnaround_time[20];


void init_pitch() {
    srand(time(NULL) ^ getpid());

    // ===== Initialize waiting time arrays =====
    for (int i = 0; i < 20; i++) {
    arrival_time[i] = -1;
    start_time[i] = -1;
    waiting_time[i] = 0;
    has_started[i] = false;
    balls_faced[i] = 0;
    runs_scored[i] = 0;
    completion_time[i] = 0;
    turnaround_time[i] = 0;
    }

    arrival_time[1] = 0;
    arrival_time[2] = 0;
    extras_total = 0;
    extras_wides = 0;
    extras_no_balls = 0;
    extras_byes = 0;
    extras_leg_byes = 0;

    pthread_mutex_init(&pitch_mutex,   NULL);
    pthread_mutex_init(&print_mutex,   NULL);
    pthread_mutex_init(&score_mutex,   NULL);
    pthread_mutex_init(&fielder_mutex, NULL);

    pthread_mutex_init(&end1_mutex, NULL);
    pthread_mutex_init(&end2_mutex, NULL);

    pthread_cond_init(&ball_delivered,    NULL);
    pthread_cond_init(&stroke_finished,   NULL);
    pthread_cond_init(&fielder_wake_cond, NULL);

    sem_init(&crease_sem, 0, 2);   // ONLY 2 batsmen allowed

}

void destroy_pitch() {
    pthread_mutex_destroy(&pitch_mutex);
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&score_mutex);
    pthread_mutex_destroy(&fielder_mutex);

    pthread_mutex_destroy(&end1_mutex);
    pthread_mutex_destroy(&end2_mutex);
    
    pthread_cond_destroy(&ball_delivered);
    pthread_cond_destroy(&stroke_finished);
    pthread_cond_destroy(&fielder_wake_cond);

    sem_destroy(&crease_sem);
}

void update_score(int runs) {
    int new_score;

    pthread_mutex_lock(&score_mutex);
    global_score += runs;
    new_score = global_score;
    pthread_mutex_unlock(&score_mutex);

    Logger::log(
        "[Score] +" + to_string(runs) +
        " run(s)  Total: " + to_string(new_score),
        "SCORE"
    );
}

void stop_match() {
    pthread_mutex_lock(&pitch_mutex);
    ball_ready  = true;
    stroke_done = true;
    pthread_cond_broadcast(&ball_delivered);
    pthread_cond_broadcast(&stroke_finished);
    pthread_mutex_unlock(&pitch_mutex);

    pthread_mutex_lock(&fielder_mutex);
    ball_in_air = false;
    ball_active = false;
    pthread_cond_broadcast(&fielder_wake_cond);
    pthread_mutex_unlock(&fielder_mutex);
}

static int random_running_runs() {
    int r = rand() % 100;
    if      (r < 35) return 1;
    else if (r < 65) return 2;
    else             return 3;
}

// =============================================================
// validate_wicket()
//   Enforces dismissal rules based on delivery type.
//
//   OS analogy: This is a policy gate — like an interrupt
//   handler deciding whether a signal is valid for the current
//   CPU state.  The "NO BALL / FREE HIT" check maps to a
//   protected-mode restriction: only a specific operation
//   (RUN_OUT) is permitted in those special states.
//
//   Rules:
//     - NO BALL  → only RUN_OUT is valid; cancel everything else
//     - FREE HIT → only RUN_OUT is valid; cancel everything else
//     - Normal   → BOWLED, CAUGHT, LBW, STUMPED are valid;
//                  also cancel any stray RUN_OUT (run-out is
//                  handled separately via deadlock logic)
// =============================================================
void validate_wicket(BallEvent &ev) {
    if (ev.is_no_ball || ev.is_free_hit) {
        // Only RUN_OUT may dismiss on no-ball / free-hit.
        // All other wickets are invalid — cancel them.
        if (ev.wicket != RUN_OUT) {
            ev.wicket = NONE;
        }
        return;
    }

    // Normal delivery: RUN_OUT is handled by deadlock module —
    // remove it here so we never double-process it.
    if (ev.wicket == RUN_OUT) {
        ev.wicket = NONE;
    }

    // BOWLED, CAUGHT, LBW, STUMPED are accepted as-is.
    // (LBW will still pass through; the final out/not-out
    //  decision is deferred to decide_lbw() in batsman_thread.)
}

int get_batsman_type(int id) {
    if (id <= 3) return 0;        // OPENERS
    else if (id <= 8) return 1;   // MIDDLE
    else return 2;                // TAIL
}

BallEvent generate_event() {
    BallEvent ev;
    ev.is_wide      = false;
    ev.is_no_ball   = false;
    ev.is_free_hit  = false;
    ev.is_boundary  = false;
    ev.is_leg_bye   = false;
    ev.is_overthrow = false;
    ev.ball_in_air  = false;
    ev.base_runs    = 0;
    ev.extra_runs   = 0;
    ev.wicket       = NONE;

    int t = rand() % 100;
    bool force_no_ball = false;
    bool force_wide    = false;

    if (!ev.is_free_hit) {
        if      (t < 95) { }
        else if (t < 98) { force_wide   = true; }
        else             { force_no_ball = true; }
    }

    if (force_no_ball && force_wide) {
        force_wide       = false;
        ev.is_no_ball    = true;
        ev.extra_runs    = 1;
        free_hit_pending = true;

        int r = rand() % 100;
        if      (r < 30) { }
        else if (r < 55) ev.base_runs = 1;
        else if (r < 70) ev.base_runs = 2;
        else if (r < 80) ev.base_runs = 3;
        else if (r < 95) { ev.base_runs = 4; ev.is_boundary = true; }
        else             { ev.base_runs = 6; ev.is_boundary = true; }

        if (!ev.is_boundary && ev.base_runs > 0)
            ev.ball_in_air = true;
        // No wicket can occur on no-ball except RUN_OUT —
        // validate_wicket() will enforce this, so we leave
        // ev.wicket = NONE here (no RUN_OUT generation).
        
        ev.is_free_hit = free_hit_pending;

        return ev;
    }

    if (force_wide) {
        ev.is_wide    = true;
        ev.extra_runs = 1;

        if (ev.is_free_hit) {
            free_hit_pending = true;
        }

        int r = rand() % 100;
        if      (r < 60) { }
        else if (r < 75) ev.extra_runs += 1;
        else if (r < 85) ev.extra_runs += 2;
        else if (r < 92) ev.extra_runs += 3;
        else             { ev.extra_runs = 5; ev.is_boundary = true; }

        if (!ev.is_boundary && ev.extra_runs > 1)
            ev.ball_in_air = true;
        return ev;
    }

    if (force_no_ball) {
        ev.is_no_ball    = true;
        ev.extra_runs    = 1;
        free_hit_pending = true;

        int r = rand() % 100;
        if      (r < 25) { }
        else if (r < 50) ev.base_runs = 1;
        else if (r < 65) ev.base_runs = 2;
        else if (r < 75) ev.base_runs = 3;
        else if (r < 90) { ev.base_runs = 4; ev.is_boundary = true; }
        else             { ev.base_runs = 6; ev.is_boundary = true; }

        if (!ev.is_boundary && ev.base_runs > 0)
            ev.ball_in_air = true;

        // RUN_OUT on no-ball is handled by deadlock module — not generated here.
        return ev;
    }

    free_hit_pending = false;

    // ===== PLAYER-AWARE DISTRIBUTION =====
    int batsman_type;

    // classify batsman using striker
    if (striker <= 3) batsman_type = 0;        // OPENERS
    else if (striker <= 8) batsman_type = 1;   // MIDDLE
    else batsman_type = 2;                     // TAIL

    int x = rand() % 100;

    // OPENERS
    if (batsman_type == 0) 
    {
        if (x < 30) ev.base_runs = 0;
        else if (x < 62) ev.base_runs = 1;
        else if (x < 72) ev.base_runs = 2;
        else if (x < 74) ev.base_runs = 3;
        else if (x < 88) { ev.base_runs = 4; ev.is_boundary = true; }
        else if (x < 95) { ev.base_runs = 6; ev.is_boundary = true; }
        else {
            int wt = rand() % 4;
            ev.wicket = (WicketType)(wt + 1);
            ev.base_runs = 0;
        }
    }

    // MIDDLE ORDER
    else if (batsman_type == 1) 
    {
        if (x < 34) ev.base_runs = 0;
        else if (x < 68) ev.base_runs = 1;
        else if (x < 78) ev.base_runs = 2;
        else if (x < 80) ev.base_runs = 3;
        else if (x < 92) { ev.base_runs = 4; ev.is_boundary = true; }
        else if (x < 97) { ev.base_runs = 6; ev.is_boundary = true; }
        else {
            int wt = rand() % 4;
            ev.wicket = (WicketType)(wt + 1);
            ev.base_runs = 0;
        }
    }

    // TAILENDERS
    else 
    {
        if (x < 45) ev.base_runs = 0;
        else if (x < 73) ev.base_runs = 1;
        else if (x < 81) ev.base_runs = 2;
        else if (x < 82) ev.base_runs = 3;
        else if (x < 92) { ev.base_runs = 4; ev.is_boundary = true; }
        else if (x < 96) { ev.base_runs = 6; ev.is_boundary = true; }
        else {
            int wt = rand() % 4;
            ev.wicket = (WicketType)(wt + 1);
            ev.base_runs = 0;
        }
    }

    // ball in air logic
    if (!ev.is_boundary && ev.wicket == NONE && ev.base_runs > 0) {
        ev.ball_in_air = true;
    }

    if (ev.is_free_hit) {
        // On free hit only RUN_OUT is valid — deadlock module handles it;
        // do not generate any wicket here.
        return ev;
    }

    

    if (!ev.is_boundary && ev.wicket == NONE && ev.base_runs > 0 && !ev.is_leg_bye) {
        if (rand() % 100 < 3) {
            ev.is_overthrow = true;
            int ot = rand() % 100;
            if      (ot < 50) ev.base_runs += 1;
            else if (ot < 85) ev.base_runs += 2;
            else              { ev.base_runs = 4; ev.is_boundary = true; }
        }
    }

    return ev;
}

void reset_for_second_innings() {

    Logger::section("=== INNINGS BREAK ===");

    target_score = global_score + 1;

    Logger::log(
        "[TARGET] Team 2 needs " + to_string(target_score) + " runs",
        "SYSTEM"
    );

    // Reset match state
    global_score = 0;
    balls_bowled = 0;
    wickets_fallen = 0;
    ball_ready = false;
    stroke_done = true;
    ball_active = false;
    ball_stopped = false;
    keeper_done = false;
    ball_owner = -1;
    backup_fielder = -1;
    ball_in_air = false;
    boundary = false;
    is_running = false;
    wicket_attempt = false;

    striker = 1;
    non_striker = 2;
    extras_total = 0;
    extras_wides = 0;
    extras_no_balls = 0;
    extras_byes = 0;
    extras_leg_byes = 0;

    free_hit_pending = false;
    next_is_free_hit = false;

    match_running = true;
    current_run_rate = 0;
    required_run_rate = 0;
    arrival_time[1] = 0;
    arrival_time[2] = 0;

    // Reset stats
    for (int i = 0; i < 20; i++) {
        arrival_time[i] = -1;
        start_time[i] = -1;
        waiting_time[i] = 0;
        balls_faced[i] = 0;
        runs_scored[i] = 0;
        has_started[i] = false;
        completion_time[i] = 0;
        turnaround_time[i] = 0;
    }
    arrival_time[1] = 0;
    arrival_time[2] = 0;
    reset_batting_progress();
    sem_destroy(&crease_sem);
    sem_init(&crease_sem, 0, 2);

    // ===== RESET BOWLER STATS FOR 2ND INNINGS =====
    for (int i = 0; i < NUM_BOWLERS; i++) {
        bowlers[i].runs_conceded = 0;
        bowlers[i].balls_bowled  = 0;
        bowlers[i].wickets       = 0;
    }   
    innings = 2;
}
