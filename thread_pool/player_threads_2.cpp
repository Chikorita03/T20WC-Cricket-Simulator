#include "player_threads_2.h"
#include "../critical_section_2/pitch_2.h"
#include <iostream>
#include <unistd.h>

using namespace std;

#define NUM_FIELDERS 9

// ============================================================
//  HELPER: Human-readable labels
// ============================================================

const char* wicket_name(int wt) {
    switch (wt) {
        case BOWLED:  return "Bowled";
        case CAUGHT:  return "Caught";
        case RUN_OUT: return "Run Out";
        case LBW:     return "LBW";
        case STUMPED: return "Stumped";
        default:      return "None";
    }
}

const char* ball_type_name(int bt) {
    switch (bt) {
        case WIDE:     return "WIDE";
        case NO_BALL:  return "NO BALL";
        case FREE_HIT: return "FREE HIT";
        default:       return "normal";
    }
}

// ============================================================
//  PENDING EVENT
// ============================================================

static BallEvent pending_event;

// ============================================================
//  HELPER: check ball_active under fielder_mutex
// ============================================================

static bool is_ball_active() {
    pthread_mutex_lock(&fielder_mutex);
    bool active = ball_active;
    pthread_mutex_unlock(&fielder_mutex);
    return active;
}

// ============================================================
//  BOWLER THREAD
// ============================================================

void* bowler_thread(void* arg) {

    while (match_running) {

        pthread_mutex_lock(&pitch_mutex);

        if (!match_running) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        // ── Start of ball: activate lifecycle ───────────────
        pthread_mutex_lock(&fielder_mutex);
        ball_active  = true;
        ball_stopped = false;
        keeper_done  = false;
        ball_owner   = -1;
        backup_fielder = -1;
        pthread_mutex_unlock(&fielder_mutex);

        BallEvent ev = generate_event();
        pending_event = ev;

        pthread_mutex_lock(&print_mutex);
        cout << "\n======================================" << endl;
        if (ev.type != NORMAL)
            cout << "[Bowler] Delivering  [" << ball_type_name(ev.type) << "]" << endl;
        else
            cout << "[Bowler] Delivering  [Ball " << (balls_bowled + 1) << "]" << endl;
        pthread_mutex_unlock(&print_mutex);

        sleep(1);

        ball_ready  = true;
        stroke_done = false;
        pthread_cond_broadcast(&ball_delivered);

        while (match_running && !stroke_done) {
            pthread_cond_wait(&stroke_finished, &pitch_mutex);
        }

        if (!match_running) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        pthread_mutex_lock(&print_mutex);
        cout << "[Bowler] Ball settled.  Score: " << global_score
             << "  |  Wickets: " << wickets_fallen
             << "  |  Balls: "   << balls_bowled << endl;
        cout << "======================================" << endl;
        pthread_mutex_unlock(&print_mutex);

        pthread_mutex_unlock(&pitch_mutex);

        sleep(1);
    }

    return NULL;
}

// ============================================================
//  BATSMAN THREAD
// ============================================================

void* batsman_thread(void* arg) {
    int id = *(int*)arg;

    while (match_running) {

        pthread_mutex_lock(&pitch_mutex);

        while (match_running && !ball_ready) {
            pthread_cond_wait(&ball_delivered, &pitch_mutex);
        }

        if (!match_running) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        if (id != striker) {
            pthread_mutex_unlock(&pitch_mutex);
            usleep(500000);
            continue;
        }

        // ─────────────────────────────────────────────────────
        //  STRIKER: full ball lifecycle
        // ─────────────────────────────────────────────────────
        BallEvent ev = pending_event;

        pthread_mutex_lock(&print_mutex);
        cout << "[Batsman " << id << " (striker)] Facing delivery" << endl;
        pthread_mutex_unlock(&print_mutex);

        sleep(1);

        // ── WIDE ─────────────────────────────────────────────
        if (ev.type == WIDE) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Umpire] WIDE - +1 run (does not count as a ball)" << endl;
            pthread_mutex_unlock(&print_mutex);

            update_score(1);
            goto finish_ball;
        }

        // ── NO_BALL announcement ──────────────────────────────
        if (ev.type == NO_BALL) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Umpire] NO BALL - +1 penalty, next ball is FREE HIT" << endl;
            pthread_mutex_unlock(&print_mutex);
        }

        // ── FREE_HIT announcement ─────────────────────────────
        if (ev.type == FREE_HIT) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Umpire] FREE HIT - batsman safe from most dismissals" << endl;
            pthread_mutex_unlock(&print_mutex);
        }

        // ── Wake fielders when ball is in the air ────────────
        if (ev.ball_in_air) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Batsman " << id << "] Ball in the air! Fielders alert!" << endl;
            pthread_mutex_unlock(&print_mutex);

            pthread_mutex_lock(&fielder_mutex);
            ball_in_air    = true;
            ball_stopped   = false;                          // Reset for fresh assignment
            ball_owner     = rand() % NUM_FIELDERS;          // 0-based
            backup_fielder = (ball_owner + 1) % NUM_FIELDERS;
            pthread_cond_broadcast(&fielder_wake_cond);
            pthread_mutex_unlock(&fielder_mutex);

            // ── Wait for primary fielder to complete before proceeding ──
            // This enforces the ordering:
            //   Batsman wakes fielders → Primary fields → Keeper acts → Ball ends
            int waited = 0;
            while (waited < 50) {          // cap at 5 s to avoid infinite hang
                pthread_mutex_lock(&fielder_mutex);
                bool done = ball_stopped;
                pthread_mutex_unlock(&fielder_mutex);
                if (done) break;
                usleep(100000);            // poll every 100 ms
                waited++;
            }
        }

        // ── Boundary ─────────────────────────────────────────
        if (ev.boundary) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Batsman " << id << "] ";
            cout << (ev.runs == 6 ? "SIX! Over the ropes!" : "FOUR! Racing to the boundary!") << endl;
            pthread_mutex_unlock(&print_mutex);

            update_score(ev.runs);
            if (ev.type == NORMAL || ev.type == FREE_HIT)
                balls_bowled++;
            goto finish_ball;
        }

        // ── Wicket ────────────────────────────────────────────
        if (ev.wicket != NONE) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [WICKET] Batsman " << id << " is OUT - "
                 << wicket_name(ev.wicket) << "!" << endl;
            pthread_mutex_unlock(&print_mutex);

            wickets_fallen++;

            int new_id = striker + 2;
            pthread_mutex_lock(&print_mutex);
            cout << "  [Innings] Batsman " << new_id << " comes to the crease." << endl;
            pthread_mutex_unlock(&print_mutex);

            striker = new_id;

            if (ev.wicket == RUN_OUT && ev.runs > 0)
                update_score(ev.runs);

            if (ev.type == NORMAL || ev.type == FREE_HIT)
                balls_bowled++;
            goto finish_ball;
        }

        // ── Runs ──────────────────────────────────────────────
        if (ev.runs > 0) {
            update_score(ev.runs);

            is_running = true;
            pthread_mutex_lock(&print_mutex);
            cout << "  [Running] Batsmen " << striker << " & " << non_striker
                 << " running - " << ev.runs << " run(s)" << endl;
            pthread_mutex_unlock(&print_mutex);

            for (int i = 0; i < ev.runs; i++) {
                usleep(400000);
                int tmp     = striker;
                striker     = non_striker;
                non_striker = tmp;
                pthread_mutex_lock(&print_mutex);
                cout << "    Run " << (i + 1) << " completed - "
                     << "striker now: Batsman " << striker << endl;
                pthread_mutex_unlock(&print_mutex);
            }
            is_running = false;

        } else {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Batsman " << id << "] Dot ball - no run" << endl;
            pthread_mutex_unlock(&print_mutex);
        }

        if (ev.type == NORMAL || ev.type == FREE_HIT)
            balls_bowled++;

    finish_ball:
        // ── End of ball: deactivate lifecycle FIRST ──────────
        // Setting ball_active = false here (still inside pitch_mutex)
        // stops all fielder/keeper threads from starting new actions.
        // ball_stopped / keeper_done are also reset for next ball.
        pthread_mutex_lock(&fielder_mutex);
        ball_in_air      = false;
        ball_active    = false;
        ball_stopped   = false;
        keeper_done    = false;
        ball_owner     = -1;
        backup_fielder = -1;
        pthread_mutex_unlock(&fielder_mutex);

        // ── Reset per-ball pitch flags ────────────────────────
        boundary       = false;
        is_running     = false;
        wicket_attempt = false;
        ball_ready     = false;
        stroke_done    = true;

        pthread_cond_signal(&stroke_finished);
        pthread_mutex_unlock(&pitch_mutex);
    }

    return NULL;
}

// ============================================================
//  FIELDER THREAD
// ============================================================

void* fielder_thread(void* arg) {
    int id      = *(int*)arg;     // 1-based
    int zero_id = id - 1;         // 0-based — matches ball_owner / backup_fielder

    while (match_running) {

        // ── Wait until ball is in the air ────────────────────
        pthread_mutex_lock(&fielder_mutex);
        while (match_running && !ball_in_air) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }
        if (!match_running) {
            pthread_mutex_unlock(&fielder_mutex);
            break;
        }

        // ── Determine role; also check lifecycle immediately ─
        bool is_primary = (zero_id == ball_owner);
        bool is_backup  = (zero_id == backup_fielder);
        bool active     = ball_active;
        pthread_mutex_unlock(&fielder_mutex);

        // Non-involved or ball already dead on wakeup → do nothing
        if (!active || (!is_primary && !is_backup)) {
            pthread_mutex_lock(&fielder_mutex);
            while (match_running && ball_in_air && ball_active) {
                pthread_mutex_unlock(&fielder_mutex);
                usleep(1000);
                pthread_mutex_lock(&fielder_mutex);
            }
            pthread_mutex_unlock(&fielder_mutex);
            continue;
        }

        // ── BACKUP: one support line only ────────────────────
        if (is_backup) {
            pthread_mutex_lock(&fielder_mutex);
            bool already_done = ball_stopped || !ball_active;
            pthread_mutex_unlock(&fielder_mutex);

            if (!already_done) {
                pthread_mutex_lock(&print_mutex);
                cout << "  [Fielder " << id << "] Backing up play" << endl;
                pthread_mutex_unlock(&print_mutex);
            }
            pthread_mutex_lock(&fielder_mutex);
            while (match_running && ball_in_air && ball_active) {
                pthread_mutex_unlock(&fielder_mutex);
                usleep(1000);
                pthread_mutex_lock(&fielder_mutex);
            }
            pthread_mutex_unlock(&fielder_mutex);
            continue;
        }

        // ── PRIMARY: full handling sequence ──────────────────

        // Guard: atomically claim the ball — prevents duplicate execution
        pthread_mutex_lock(&fielder_mutex);
        if (ball_stopped || !ball_active) {
            pthread_mutex_unlock(&fielder_mutex);
            continue;
        }
        // Do NOT set ball_stopped yet; set it after actions complete
        pthread_mutex_unlock(&fielder_mutex);

        usleep(300000);   // Sprint to ball

        // Check lifecycle again after sleep
        if (!is_ball_active()) continue;

        pthread_mutex_lock(&print_mutex);
        cout << "  [Fielder " << id << "] Moving to ball" << endl;
        pthread_mutex_unlock(&print_mutex);

        usleep(200000);

        if (!is_ball_active()) continue;

        int outcome = rand() % 100;
        if (outcome < 10) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Fielder " << id << "] Catch - taken cleanly!" << endl;
            pthread_mutex_unlock(&print_mutex);
        } else {
            int dist = 10 + rand() % 50;
            pthread_mutex_lock(&print_mutex);
            cout << "  [Fielder " << id << "] Stops the ball at " << dist << " metres" << endl;
            pthread_mutex_unlock(&print_mutex);
        }

        if (!is_ball_active()) continue;

        pthread_mutex_lock(&print_mutex);
        cout << "  [Fielder " << id << "] Throws to keeper" << endl;
        pthread_mutex_unlock(&print_mutex);

        // ── Mark ball handled (enables keeper to act) ────────
        pthread_mutex_lock(&fielder_mutex);
        ball_stopped = true;
        pthread_mutex_unlock(&fielder_mutex);
    }

    return NULL;
}

// ============================================================
//  WICKET KEEPER THREAD
// ============================================================

void* wicket_keeper_thread(void* arg) {

    while (match_running) {

        // ── Wait until ball is in the air ────────────────────
        pthread_mutex_lock(&fielder_mutex);
        while (match_running && !ball_in_air) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }
        if (!match_running) {
            pthread_mutex_unlock(&fielder_mutex);
            break;
        }
        pthread_mutex_unlock(&fielder_mutex);

        // ── Poll until primary fielder has thrown (or ball ends) ─
        // Single-pass poll with small sleep steps; no unbounded retry loop.
        bool thrown = false;
        for (int i = 0; i < 20; i++) {          // max ~2 s wait
            pthread_mutex_lock(&fielder_mutex);
            thrown = ball_stopped && ball_active;
            pthread_mutex_unlock(&fielder_mutex);
            if (thrown) break;
            usleep(100000);
        }

        // ── Claim keeper action atomically (act only once per ball) ─
        pthread_mutex_lock(&fielder_mutex);
        if (!thrown || keeper_done || !ball_active) {
            pthread_mutex_unlock(&fielder_mutex);
            continue;
        }
        keeper_done = true;
        pthread_mutex_unlock(&fielder_mutex);

        // ── Keeper acts ───────────────────────────────────────
        pthread_mutex_lock(&print_mutex);
        cout << "  [Keeper] Receives throw from Fielder "
             << (ball_owner + 1) << " - gloves ready!" << endl;
        pthread_mutex_unlock(&print_mutex);

        usleep(150000);

        // Final lifecycle check after sleep
        if (!is_ball_active()) continue;

        int action = rand() % 100;
        if (action < 15) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Keeper] STUMPING attempt - bails off!" << endl;
            pthread_mutex_unlock(&print_mutex);
        } else if (action < 30) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Keeper] Catch behind the wicket - "
                 << ((action < 22) ? "taken!" : "grassed!") << endl;
            pthread_mutex_unlock(&print_mutex);
        } else if (action < 45 && is_running) {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Keeper] RUN-OUT attempt at the stumps!" << endl;
            pthread_mutex_unlock(&print_mutex);
        } else {
            pthread_mutex_lock(&print_mutex);
            cout << "  [Keeper] Ball collected - all safe" << endl;
            pthread_mutex_unlock(&print_mutex);
        }
    }

    return NULL;
}
