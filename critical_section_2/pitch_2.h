#pragma once
#include <pthread.h>

// ============================================================
//  ENUMERATIONS
// ============================================================

enum BallType {
    NORMAL,
    WIDE,
    NO_BALL,
    FREE_HIT
};

enum WicketType {
    NONE,
    BOWLED,
    CAUGHT,
    RUN_OUT,
    LBW,
    STUMPED
};

// ============================================================
//  BALL EVENT STRUCTURE
// ============================================================

struct BallEvent {
    BallType   type;
    int        runs;
    WicketType wicket;
    bool       ball_in_air;
    bool       boundary;
};

// ============================================================
//  MUTEXES
// ============================================================

extern pthread_mutex_t pitch_mutex;
extern pthread_mutex_t print_mutex;
extern pthread_mutex_t score_mutex;
extern pthread_mutex_t fielder_mutex;

// ============================================================
//  CONDITION VARIABLES
// ============================================================

extern pthread_cond_t ball_delivered;
extern pthread_cond_t stroke_finished;
extern pthread_cond_t fielder_wake_cond;

// ============================================================
//  SHARED FLAGS  (guarded by pitch_mutex unless noted)
// ============================================================

extern bool ball_ready;
extern bool stroke_done;

extern bool ball_in_air;      // guarded by fielder_mutex
extern bool boundary;
extern bool is_running;
extern bool wicket_attempt;

// ============================================================
//  FIELDING COORDINATION  (guarded by fielder_mutex)
// ============================================================

extern int  ball_owner;       // 0-based index: primary fielder for this ball
extern int  backup_fielder;   // 0-based index: backup fielder for this ball
extern bool ball_stopped;     // true once primary fielder has handled the ball

// ============================================================
//  BALL LIFECYCLE CONTROL  (guarded by fielder_mutex)
// ============================================================

extern bool ball_active;      // true = ball in progress; false = ball finished
extern bool keeper_done;      // true once keeper has acted this ball

// ============================================================
//  MATCH STATE
// ============================================================

extern int  global_score;
extern int  striker;
extern int  non_striker;
extern bool next_is_free_hit;
extern int  balls_bowled;
extern int  wickets_fallen;
extern bool match_running;

// ============================================================
//  FUNCTION DECLARATIONS
// ============================================================

void      init_pitch();
void      destroy_pitch();
void      update_score(int runs);
void      stop_match();
BallEvent generate_event();