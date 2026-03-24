#include "pitch_2.h"
#include <iostream>
#include <cstdlib>
#include <ctime>

using namespace std;

// ============================================================
//  MUTEX / CONDITION VARIABLE DEFINITIONS
// ============================================================

pthread_mutex_t pitch_mutex;
pthread_mutex_t print_mutex;
pthread_mutex_t score_mutex;
pthread_mutex_t fielder_mutex;

pthread_cond_t ball_delivered;
pthread_cond_t stroke_finished;
pthread_cond_t fielder_wake_cond;

// ============================================================
//  SHARED FLAGS
// ============================================================

bool ball_ready     = false;
bool stroke_done    = true;
bool ball_in_air    = false;
bool boundary       = false;
bool is_running     = false;
bool wicket_attempt = false;

// ============================================================
//  FIELDING COORDINATION
// ============================================================

int  ball_owner     = -1;
int  backup_fielder = -1;
bool ball_stopped   = false;

// ============================================================
//  BALL LIFECYCLE CONTROL
// ============================================================

bool ball_active  = false;   // true while a delivery is in progress
bool keeper_done  = false;   // true once keeper has acted this ball

// ============================================================
//  MATCH STATE
// ============================================================

int  global_score     = 0;
int  striker          = 1;
int  non_striker      = 2;
bool next_is_free_hit = false;
int  balls_bowled     = 0;
int  wickets_fallen   = 0;
bool match_running    = true;

// ============================================================
//  INIT / DESTROY
// ============================================================

void init_pitch() {
    srand((unsigned int)time(NULL));

    pthread_mutex_init(&pitch_mutex,   NULL);
    pthread_mutex_init(&print_mutex,   NULL);
    pthread_mutex_init(&score_mutex,   NULL);
    pthread_mutex_init(&fielder_mutex, NULL);

    pthread_cond_init(&ball_delivered,    NULL);
    pthread_cond_init(&stroke_finished,   NULL);
    pthread_cond_init(&fielder_wake_cond, NULL);
}

void destroy_pitch() {
    pthread_mutex_destroy(&pitch_mutex);
    pthread_mutex_destroy(&print_mutex);
    pthread_mutex_destroy(&score_mutex);
    pthread_mutex_destroy(&fielder_mutex);

    pthread_cond_destroy(&ball_delivered);
    pthread_cond_destroy(&stroke_finished);
    pthread_cond_destroy(&fielder_wake_cond);
}

// ============================================================
//  SCORE UPDATE
// ============================================================

void update_score(int runs) {
    int new_score;

    pthread_mutex_lock(&score_mutex);
    global_score += runs;
    new_score = global_score;
    pthread_mutex_unlock(&score_mutex);

    pthread_mutex_lock(&print_mutex);
    cout << "  [Score] +" << runs << " run(s)  ->  Total: " << new_score << endl;
    pthread_mutex_unlock(&print_mutex);
}

// ============================================================
//  STOP MATCH
// ============================================================

void stop_match() {
    pthread_mutex_lock(&pitch_mutex);
    ball_ready  = true;
    stroke_done = true;
    pthread_cond_broadcast(&ball_delivered);
    pthread_cond_broadcast(&stroke_finished);
    pthread_mutex_unlock(&pitch_mutex);

    pthread_mutex_lock(&fielder_mutex);
    ball_in_air  = false;
    ball_active  = false;   // Kill any fielder/keeper still acting
    pthread_cond_broadcast(&fielder_wake_cond);
    pthread_mutex_unlock(&fielder_mutex);
}

// ============================================================
//  BALL EVENT GENERATOR
// ============================================================

BallEvent generate_event() {
    BallEvent ev;
    ev.runs        = 0;
    ev.wicket      = NONE;
    ev.ball_in_air = false;
    ev.boundary    = false;

    if (next_is_free_hit) {
        ev.type          = FREE_HIT;
        next_is_free_hit = false;
    } else {
        int t = rand() % 100;
        if      (t < 70) ev.type = NORMAL;
        else if (t < 83) ev.type = WIDE;
        else             ev.type = NO_BALL;
    }

    switch (ev.type) {

        case WIDE:
            ev.runs = 1;
            return ev;

        case NO_BALL:
            ev.runs          = 1;
            next_is_free_hit = true;
            {
                int r = rand() % 100;
                if      (r < 30) { /* no extra runs */ }
                else if (r < 55) ev.runs += 1;
                else if (r < 70) ev.runs += 2;
                else if (r < 80) ev.runs += 3;
                else if (r < 95) { ev.runs += 4; ev.boundary = true; }
                else             { ev.runs += 6; ev.boundary = true; }
            }
            if (!ev.boundary && ev.runs > 1)
                ev.ball_in_air = true;
            return ev;

        case NORMAL:
        case FREE_HIT:
        default:
            break;
    }

    int r = rand() % 100;
    if      (r < 30) ev.runs = 0;
    else if (r < 55) ev.runs = 1;
    else if (r < 70) ev.runs = 2;
    else if (r < 80) ev.runs = 3;
    else if (r < 95) { ev.runs = 4; ev.boundary = true; }
    else             { ev.runs = 6; ev.boundary = true; }

    if (!ev.boundary)
        ev.ball_in_air = true;

    if (ev.type == FREE_HIT) {
        if (!ev.boundary && (rand() % 100 < 5)) {
            ev.wicket      = RUN_OUT;
            ev.ball_in_air = true;
        }
        return ev;
    }

    if (!ev.boundary) {
        int w = rand() % 100;
        if (w < 18) {
            int wt = rand() % 5;
            switch (wt) {
                case 0: ev.wicket = BOWLED;  ev.ball_in_air = false; break;
                case 3: ev.wicket = LBW;     ev.ball_in_air = false; break;
                case 1: ev.wicket = CAUGHT;  ev.ball_in_air = true;  break;
                case 2: ev.wicket = RUN_OUT; ev.ball_in_air = true;  break;
                case 4: ev.wicket = STUMPED; ev.ball_in_air = true;  break;
            }
            if (ev.wicket != RUN_OUT)
                ev.runs = 0;
        }
    }

    return ev;
}