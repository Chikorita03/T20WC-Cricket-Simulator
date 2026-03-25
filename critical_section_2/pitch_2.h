#pragma once
#include <pthread.h>
#include <semaphore.h>

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

void      init_pitch();
void      destroy_pitch();
void      update_score(int runs);
void      stop_match();
BallEvent generate_event();