#include "player_threads_2.h"
#include "../critical_section_2/pitch_2.h"
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <pthread.h>
#include "../scheduler/umpire.h"

using namespace std;

static BallEvent current_event;

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

// =============================================================
// decide_lbw()
//   Simulates umpire deliberation for an LBW appeal.
//
//   OS analogy: This is a deferred interrupt handler — the
//   initial signal (LBW) is raised, but the kernel (umpire)
//   evaluates it asynchronously before committing to a state
//   change (dismissal).  ~60% probability of OUT mirrors a
//   real umpire's bias toward upholding clear-cut appeals.
//
//   Returns true  → batsman is OUT
//           false → NOT OUT, wicket cancelled
// =============================================================
bool decide_lbw() {
    pthread_mutex_lock(&print_mutex);
    cout << "[Umpire] Considering LBW appeal..." << endl;
    pthread_mutex_unlock(&print_mutex);

    bool out = (rand() % 100) < 60;   // ~60% chance OUT

    pthread_mutex_lock(&print_mutex);
    if (out)
        cout << "[Umpire] LBW - OUT! Finger raised." << endl;
    else
        cout << "[Umpire] LBW - NOT OUT! Appeal rejected." << endl;
    pthread_mutex_unlock(&print_mutex);

    return out;
}

// =============================================================
// bowler_thread()
//   Producer thread: generates a BallEvent each iteration,
//   validates dismissal legality via validate_wicket(), then
//   signals batsman threads (consumers) via condition variable.
// =============================================================
void* bowler_thread(void* arg) {
    while (match_running) {
        pthread_mutex_lock(&pitch_mutex);

        if (!match_running) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        // ---- Generate raw ball event ----
        current_event = generate_event();

        // ---- Enforce wicket rules (NO BALL / FREE HIT guard) ----
        validate_wicket(current_event);

        pthread_mutex_lock(&fielder_mutex);
        ball_active  = false;
        ball_stopped = !current_event.ball_in_air;
        keeper_done  = !current_event.ball_in_air;
        ball_owner   = -1;
        pthread_mutex_unlock(&fielder_mutex);

        pthread_mutex_lock(&print_mutex);
        if (current_event.is_free_hit) {
            cout << "[Bowler] FREE HIT - Ball " << (balls_bowled + 1) << endl;
        } else if (current_event.is_wide) {
            cout << "[Bowler] Wide delivery" << endl;
        } else if (current_event.is_no_ball) {
            cout << "[Bowler] NO BALL - next ball is a free hit" << endl;
        } else {
            cout << "[Bowler " << get_current_bowler()
            << "] Delivering ball " << (balls_bowled + 1) << endl;
        }
        pthread_mutex_unlock(&print_mutex);

        ball_ready  = true;
        stroke_done = false;
        pthread_cond_broadcast(&ball_delivered);

        while (!stroke_done && match_running) {
            pthread_cond_wait(&stroke_finished, &pitch_mutex);
        }

        if (match_running) {
            if (!current_event.is_wide && !current_event.is_no_ball) {

                // ===== RR SCHEDULING: Update Bowler PCB =====
                int b = get_current_bowler();

                bowlers[b].balls_bowled++;

                int runs = current_event.base_runs + current_event.extra_runs;
                bowlers[b].runs_conceded += runs;

                if (current_event.wicket != NONE) {
                    bowlers[b].wickets++;
                }

                balls_bowled++;

                // ===== RR Scheduler: Context switch check =====
                on_ball_completed();
            }
            pthread_mutex_lock(&print_mutex);
            cout << "[Pitch] Ball completed | Balls: " << balls_bowled
                 << " | Wickets: " << wickets_fallen << "\n" << endl;
            pthread_mutex_unlock(&print_mutex);
        }

        pthread_mutex_unlock(&pitch_mutex);

        if (match_running && (wickets_fallen >= 10 || balls_bowled >= 120)) {
            match_running = false;
            stop_match();
        }

        if (match_running) sleep(1);
    }
    return NULL;
}

// =============================================================
// batsman_thread()
//   Consumer thread: waits for a ball, plays the stroke, then
//   handles run scoring, wicket outcomes, and striker rotation.
//
//   LBW special path:
//     1. Print appeal message (already printed by print block).
//     2. Call decide_lbw() — deferred umpire decision.
//     3. If NOT OUT → cancel wicket so normal run logic runs.
// =============================================================
void* batsman_thread(void* arg) {
    int id = *(int*)arg;
    int my_id = id;   // dynamic id (IMPORTANT)

    sem_wait(&crease_sem);   // batsman enters crease

    while (match_running) {
        pthread_mutex_lock(&pitch_mutex);

        while ((!ball_ready || my_id != striker) && match_running) {
            pthread_cond_wait(&ball_delivered, &pitch_mutex);
        }

        if (!match_running) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        ball_ready = false;

        pthread_mutex_lock(&print_mutex);
        if (current_event.is_wide) {
            cout << "[Batsman " << my_id << "] Wide - cannot play" << endl;
        } else if (current_event.wicket == BOWLED) {
            cout << "[Batsman " << my_id << "] BOWLED!" << endl;
        } else if (current_event.wicket == LBW) {
            cout << "[Batsman " << my_id << "] LBW appeal! Struck on the pads." << endl;
        } else if (current_event.is_boundary && current_event.base_runs == 6) {
            cout << "[Batsman " << my_id << "] SIX!" << endl;
        } else if (current_event.is_boundary && current_event.base_runs == 4) {
            cout << "[Batsman " << my_id << "] FOUR!" << endl;
        } else if (current_event.is_leg_bye) {
            cout << "[Batsman " << my_id << "] Leg bye" << endl;
        } else if (current_event.base_runs == 0) {
            cout << "[Batsman " << my_id << "] Defended - dot ball" << endl;
        } else {
            cout << "[Batsman " << my_id << "] Playing for " << current_event.base_runs << " run(s)" << endl;
        }
        pthread_mutex_unlock(&print_mutex);

        // ===== LBW: deferred umpire decision =====
        // Must happen before we release pitch_mutex so that
        // wicket state is resolved before fielder/keeper wake.
        if (current_event.wicket == LBW) {
            bool lbw_out = decide_lbw();
            if (!lbw_out) {
                // NOT OUT — treat as dot ball; cancel the wicket
                current_event.wicket    = NONE;
                current_event.base_runs = 0;
            }
            // If OUT, current_event.wicket stays LBW and the
            // normal wicket-processing block below handles it.
        }

        // ===== WAITING TIME: First execution =====
        if (!has_started[my_id]) {
            start_time[my_id] = balls_bowled;
            waiting_time[my_id] = start_time[my_id] - arrival_time[my_id];
            has_started[my_id] = true;
        }

        pthread_mutex_unlock(&pitch_mutex);

        // ===== STUMPED: wait for keeper confirmation =====
        if (current_event.wicket == STUMPED) {
            pthread_mutex_lock(&fielder_mutex);
            while (!keeper_done && match_running) {
                pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
            }
            pthread_mutex_unlock(&fielder_mutex);
        }

        if (current_event.ball_in_air) {
            pthread_mutex_lock(&fielder_mutex);
            ball_active = true;
            pthread_cond_broadcast(&fielder_wake_cond);
            while ((!ball_stopped || !keeper_done) && match_running) {
                pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
            }
            pthread_mutex_unlock(&fielder_mutex);
        }

        if (!match_running) {
            pthread_mutex_lock(&pitch_mutex);
            stroke_done = true;
            pthread_cond_signal(&stroke_finished);
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        int total_runs   = current_event.base_runs + current_event.extra_runs;
        int running_runs = current_event.is_wide ? 0 : current_event.base_runs;

        if (current_event.wicket != NONE) {
            sem_post(&crease_sem);
            pthread_mutex_lock(&score_mutex);
            wickets_fallen++;
            // ===== STOP MATCH IF ALL OUT =====
            if (wickets_fallen >= 10) {
                match_running = false;
            }
            pthread_mutex_unlock(&score_mutex);
            // Wake all threads safely
            if (!match_running) {
                stop_match();
            }
            
            // ===== IF MATCH ENDED → DO NOT CREATE NEW BATSMAN =====
            if (!match_running) {

                pthread_mutex_lock(&print_mutex);
                cout << "[MATCH] All out! Innings ends." << endl;
                pthread_mutex_unlock(&print_mutex);

                stop_match();

                // Complete current ball safely
                pthread_mutex_lock(&pitch_mutex);
                stroke_done = true;
                pthread_cond_signal(&stroke_finished);
                pthread_mutex_unlock(&pitch_mutex);

                continue;   // IMPORTANT: skip batsman creation
            }

            // ===== ONLY IF MATCH CONTINUES → CREATE NEW BATSMAN =====
            
            int new_id;

            // ===== SCHEDULER: FCFS vs SJF =====
        if (use_sjf) {
            if (!batting_order_sjf.empty()) {
                new_id = batting_order_sjf.top().second;
                batting_order_sjf.pop();
            } else {
                new_id = 3;
            }

            pthread_mutex_lock(&print_mutex);
            cout << "[SJF] Batsman " << new_id << " comes in" << endl;
            pthread_mutex_unlock(&print_mutex);

        } else {
            if (!batting_order_fcfs.empty()) {
                new_id = batting_order_fcfs.front();
                batting_order_fcfs.pop();
            } else {
            new_id = 3;
            }

            pthread_mutex_lock(&print_mutex);
            cout << "[FCFS] Batsman " << new_id << " comes in" << endl;
            pthread_mutex_unlock(&print_mutex);
        }

            // ===== WAITING TIME: Arrival =====
            arrival_time[new_id] = balls_bowled;

            pthread_mutex_lock(&print_mutex);
            cout << "[Outcome] WICKET - " << wicket_name(current_event.wicket) << "!" << endl;
            pthread_mutex_unlock(&print_mutex);

            sem_wait(&crease_sem);   // new batsman occupies crease

            my_id = new_id;
            int old_non_striker = non_striker;
            striker = my_id;
            non_striker = old_non_striker;

            pthread_cond_broadcast(&ball_delivered);

            if (current_event.ball_in_air) {
                pthread_mutex_lock(&fielder_mutex);
                ball_active = false;
                pthread_cond_broadcast(&fielder_wake_cond);
                pthread_mutex_unlock(&fielder_mutex);
            }

            pthread_mutex_lock(&pitch_mutex);
            stroke_done = true;
            pthread_cond_signal(&stroke_finished);
            pthread_mutex_unlock(&pitch_mutex);

            continue;
        }

        pthread_mutex_lock(&print_mutex);
        if (current_event.is_overthrow) {
            cout << "[Outcome] OVERTHROW! " << current_event.base_runs << " run(s) total" << endl;
        } else if (current_event.is_boundary) {
            cout << "[Outcome] BOUNDARY - " << current_event.base_runs << " runs" << endl;
        } else if (current_event.is_wide) {
            cout << "[Outcome] Wide - " << current_event.extra_runs << " extra(s)" << endl;
        } else if (current_event.is_no_ball) {
            cout << "[Outcome] No Ball - 1 extra" << endl;
        } else if (current_event.is_leg_bye) {
            cout << "[Outcome] Leg Bye - " << current_event.base_runs << " run(s)" << endl;
        } else if (total_runs == 0) {
            cout << "[Outcome] Dot ball" << endl;
        } else {
            cout << "[Outcome] " << total_runs << " run(s)" << endl;
        }
        pthread_mutex_unlock(&print_mutex);

        if (total_runs > 0) {
            update_score(total_runs);
        }

        if (current_event.wicket == NONE && running_runs % 2 == 1) {
            pthread_mutex_lock(&score_mutex);
            int tmp     = striker;
            striker     = non_striker;
            non_striker = tmp;
            pthread_mutex_unlock(&score_mutex);
        }

        if (current_event.ball_in_air) {
            pthread_mutex_lock(&fielder_mutex);
            ball_active = false;
            pthread_cond_broadcast(&fielder_wake_cond);
            pthread_mutex_unlock(&fielder_mutex);
        }

        pthread_mutex_lock(&pitch_mutex);
        stroke_done = true;
        pthread_cond_signal(&stroke_finished);
        pthread_mutex_unlock(&pitch_mutex);
    }
    return NULL;
}

// =============================================================
// fielder_thread()
//   Condition-variable sleeping thread (consumer-side helper).
//   Wakes on ball_active, races to claim the ball.
//
//   CAUGHT handling:
//     A fielder who grabs a CAUGHT ball sets ball_stopped = true,
//     which is the signal batsman_thread waits on.  The wicket
//     itself was already set in generate_event(); the fielder
//     only needs to confirm ball possession.
//
//   STUMPED is explicitly excluded (wicket_keeper handles it).
// =============================================================
void* fielder_thread(void* arg) {
    int id = *(int*)arg;
    unsigned int rseed = (unsigned)(time(NULL) + id * 7919u)
                       ^ (unsigned)((unsigned long)pthread_self() & 0xFFFFFFFFUL)
                       ^ ((unsigned)id * 1234567891u)
                       ^ ((unsigned)(id * id) * 6700417u);

    while (match_running) {
        pthread_mutex_lock(&fielder_mutex);

        while (!ball_active && match_running) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }

        if (!match_running) {
            pthread_mutex_unlock(&fielder_mutex);
            break;
        }

        // Race to claim ball — exclude STUMPED (keeper's job)
        if (!ball_stopped && ball_owner == -1 && current_event.wicket != STUMPED) {
            ball_owner = id;
            int dist = 5 + (int)(rand_r(&rseed) % 55);

            pthread_mutex_lock(&print_mutex);
            if (current_event.wicket == CAUGHT) {
                // Fielder caught it — wicket was set by generator;
                // we confirm possession so batsman_thread can proceed.
                cout << "[Fielder " << id << "] CAUGHT! Ball safely held." << endl;
            } else {
                cout << "[Fielder " << id << "] Stopped ball at " << dist << "m" << endl;
            }
            pthread_mutex_unlock(&print_mutex);

            ball_stopped = true;
            pthread_cond_broadcast(&fielder_wake_cond);
        }

        // Wait until this ball cycle ends
        while (ball_active && match_running) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }

        pthread_mutex_unlock(&fielder_mutex);
    }
    return NULL;
}

// =============================================================
// wicket_keeper_thread()
//   Handles STUMPED dismissals exclusively.
//
//   OS analogy: dedicated interrupt handler for a specific
//   interrupt vector (STUMPED).  It only acts if the ball has
//   not already been claimed (ball_stopped == false), preventing
//   a race with fielders.
// =============================================================
void* wicket_keeper_thread(void* arg) {
    while (match_running) {
        pthread_mutex_lock(&fielder_mutex);

        while (!ball_active && match_running) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }

        if (!match_running) {
            pthread_mutex_unlock(&fielder_mutex);
            break;
        }

        if (current_event.wicket == STUMPED) {
            // STUMPED: keeper acts only if ball not already stopped
            // (guards against a fielder accidentally claiming the ball
            //  when wicket == STUMPED, which fielder_thread now prevents)
            if (!ball_stopped) {
                pthread_mutex_lock(&print_mutex);
                cout << "[Keeper] STUMPED! Bails off — batsman out of crease." << endl;
                pthread_mutex_unlock(&print_mutex);

                ball_owner   = 0;   // 0 = keeper owns the ball
                ball_stopped = true;
                pthread_cond_broadcast(&fielder_wake_cond);
            }
        } else {
            // Non-stumped ball: keeper receives safely if no fielder got it
            if (ball_owner == -1 && !ball_stopped) {
                pthread_mutex_lock(&print_mutex);
                cout << "[Keeper] Ball safely in gloves" << endl;
                pthread_mutex_unlock(&print_mutex);
            }
        }

        keeper_done = true;
        pthread_cond_broadcast(&fielder_wake_cond);

        while (ball_active && match_running) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }

        pthread_mutex_unlock(&fielder_mutex);
    }
    return NULL;
}
