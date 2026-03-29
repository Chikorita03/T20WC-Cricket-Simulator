#include "player_threads_2.h"
#include "log.h"
#include <sstream>
#include <iomanip>
#include "../critical_section_2/pitch_2.h"
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <pthread.h>
#include "../scheduler/umpire.h"
#include "../thread_pool/gantt.h"

using namespace std;

static BallEvent current_event;
static bool batsman3_used = false;
static int next_fixed = 9;

static int delivery_extras_for_event(const BallEvent& ev) {
    int wide_runs = 0;
    int no_ball_runs = 0;
    int bye_runs = 0;
    int leg_bye_runs = 0;

    if (ev.is_wide) {
        wide_runs = ev.extra_runs;
    } else if (ev.is_no_ball) {
        no_ball_runs = ev.extra_runs;
    } else if (ev.is_leg_bye) {
        leg_bye_runs = ev.base_runs + ev.extra_runs;
    } else if (ev.extra_runs > 0) {
        bye_runs = ev.extra_runs;
    }

    return wide_runs + no_ball_runs + bye_runs + leg_bye_runs;
}

static int read_total_extras() {
    pthread_mutex_lock(&score_mutex);
    int total = extras_total;
    pthread_mutex_unlock(&score_mutex);
    return total;
}

static int read_global_score() {
    pthread_mutex_lock(&score_mutex);
    int score = global_score;
    pthread_mutex_unlock(&score_mutex);
    return score;
}

static void log_pitch_ball_completed(int balls_after, int wickets_after) {
    int innings_extras = read_total_extras();

    Logger::log(
        "[Pitch] Ball completed | Balls: " + to_string(balls_after) +
        " | Wickets: " + to_string(wickets_after) +
        " | Extras: " + to_string(innings_extras),
        "UMPIRE"
    );
}

static void log_chase_requirement(int balls_after, int score_after) {
    if (innings != 2) return;
    int balls_left = 120 - balls_after;
    int runs_needed = target_score - score_after;
    if (balls_left < 0) balls_left = 0;
    if (runs_needed < 0) runs_needed = 0;

    Logger::log(
        "[CHASE] Need " + to_string(runs_needed) + " in " + to_string(balls_left) + " balls",
        "STATS"
    );
}

static void record_extras(const BallEvent& ev) {
    int delivery_extras = delivery_extras_for_event(ev);
    if (delivery_extras == 0) return;

    pthread_mutex_lock(&score_mutex);
    if (ev.is_wide) {
        extras_wides += ev.extra_runs;
    } else if (ev.is_no_ball) {
        extras_no_balls += ev.extra_runs;
    } else if (ev.is_leg_bye) {
        extras_leg_byes += ev.base_runs + ev.extra_runs;
    } else if (ev.extra_runs > 0) {
        extras_byes += ev.extra_runs;
    }
    extras_total += delivery_extras;
    pthread_mutex_unlock(&score_mutex);
}

void reset_batting_progress() {
    batsman3_used = false;
    next_fixed = 9;
}

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

// decide_lbw()
//   simulates umpire deliberation for an LBW appeal.
//   This is a deferred interrupt handler — the
//   initial signal (LBW) is raised, but the kernel (umpire)
//   evaluates it asynchronously before committing to a state
//   change (dismissal).  ~60% probability of OUT mirrors a
//   real umpire's bias toward upholding clear-cut appeals.
//   returns true  → batsman is OUT
//           false → NOT OUT, wicket cancelled

bool decide_lbw() {
    Logger::log("[Umpire] Considering LBW appeal...", "UMPIRE");

    bool out = (rand() % 100) < 60;   // ~60% chance OUT

    if(out)
        Logger::log("[Umpire] LBW - OUT!", "UMPIRE");
    else
        Logger::log("[Umpire] LBW - NOT OUT!", "UMPIRE");

    return out;
}

// bowler_thread()
//   producer thread: generates a BallEvent each iteration,
//   validates dismissal legality via validate_wicket(), then signals batsman threads via condition variable.

void* bowler_thread(void* arg) {
    while (match_running) {
        bool innings_finished = false;
        pthread_mutex_lock(&pitch_mutex);

        if (!match_running) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        // print header once per ball
        int over = balls_bowled / 6;
        int ball = balls_bowled % 6 + 1;

        Logger::section(
            "Over " + to_string(over) + "." + to_string(ball) + " | Ball " + to_string(balls_bowled+1)
        );

        int retry_count = 0;

        while(true){
            if(!match_running){
                break;
            }
            retry_count++;

            // generate raw ball event 
            current_event = generate_event();

            // enforce wicket rules (NO BALL / FREE HIT guard)
            validate_wicket(current_event);

            // safety: prevent infinite loop 
            if (retry_count > 8) {
                current_event.is_wide = false;
                current_event.is_no_ball = false;
            }

            pthread_mutex_lock(&fielder_mutex);
            ball_active  = false;
            ball_stopped = !current_event.ball_in_air;
            keeper_done  = !current_event.ball_in_air;
            ball_owner   = -1;
            pthread_mutex_unlock(&fielder_mutex);

            string msg;

            if(current_event.is_free_hit)
                msg="[Bowler] FREE HIT - Ball "+to_string(balls_bowled+1);
            else if(current_event.is_wide)
                msg="[Bowler] Wide delivery";
            else if(current_event.is_no_ball)
                msg="[Bowler] NO BALL - next ball is a free hit";
            else
                msg="[Bowler "+to_string(get_current_bowler() + 1)+
                    "] Delivering ball "+to_string(balls_bowled+1);

            Logger::log(msg,"BOWLER");

            if (!current_event.is_wide && !current_event.is_no_ball) {
                record_delivery_start_context(
                    balls_bowled + 1,
                    get_current_bowler() + 1,
                    striker,
                    non_striker
                );
            }

            ball_ready  = true;
            stroke_done = false;
            pthread_cond_broadcast(&ball_delivered);

            while (!stroke_done && match_running) {
                pthread_cond_wait(&stroke_finished, &pitch_mutex);

            }

            if(!match_running){
                break;
            }

            if (match_running) {
                int runs = current_event.base_runs + current_event.extra_runs;

                if (!current_event.is_wide && !current_event.is_no_ball) {

                    // RR scheduling: update bowler PCB
                    int b = get_current_bowler();

                    bowlers[b].balls_bowled++;
                    bowlers[b].runs_conceded += runs;

                    if (current_event.wicket != NONE) {
                        bowlers[b].wickets++;
                    }
                    balls_bowled++; 
                }
                
                log_pitch_ball_completed(balls_bowled, wickets_fallen);

                float overs = balls_bowled / 6.0f;
                current_run_rate = (overs > 0) ? (global_score / overs) : 0;

                stringstream rr_msg;
                rr_msg << fixed << setprecision(2);
                rr_msg << "[STATS] CRR: " << current_run_rate;

                if (innings == 2) {
                    int balls_left = 120 - balls_bowled;
                    int runs_needed = target_score - global_score;
                    if (balls_left < 0) balls_left = 0;
                    if (runs_needed < 0) runs_needed = 0;

                    if (balls_left > 0)
                        required_run_rate = (runs_needed * 6.0f) / balls_left;
                    else
                        required_run_rate = 0;

                    if (required_run_rate > 36.0f) {
                        rr_msg << " | RRR: >36";
                    } else {
                        rr_msg << " | RRR: " << required_run_rate;
                    }
                }

                Logger::log(rr_msg.str(), "STATS");
                log_chase_requirement(balls_bowled, read_global_score());

                if (!current_event.is_wide && !current_event.is_no_ball){
                    on_ball_completed();

                    if (balls_bowled >= 120) {
                        if (innings == 1)
                            Logger::log("[MATCH] Innings 1 complete! 20 overs finished.", "SYSTEM");
                        else
                            Logger::log("[MATCH] Innings 2 complete! 20 overs finished.", "SYSTEM");

                        match_running = false;
                        innings_finished = true;
                    }

                    Logger::log(" ", "");
                    break;   // exit retry loop on valid ball
                }

                Logger::log(" ", "");
            }
        }

        pthread_mutex_unlock(&pitch_mutex);

        if (innings_finished) {
            stop_match();
        }

        if (match_running) sleep(1);
    }
    return NULL;
}

// batsman_thread()
//   consumer thread: waits for a ball, plays the stroke, then
//   handles run scoring, wicket outcomes, and striker rotation.
//
//   LBW special path:
//     1. print appeal message (already printed by print block).
//     2. call decide_lbw() — deferred umpire decision.
//     3. if NOT OUT - cancel wicket so normal run logic runs.

void* batsman_thread(void* arg) {
    int id = *(int*)arg;
    int my_id = id;   // dynamic id 

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

        string msg1;

        if(current_event.is_wide)
            msg1="[Batsman "+to_string(my_id)+"] Wide - cannot play";
        else if(current_event.wicket==BOWLED)
            msg1="[Batsman "+to_string(my_id)+"] BOWLED!";
        else if(current_event.wicket==LBW)
            msg1="[Batsman "+to_string(my_id)+"] LBW appeal!";
        else if(current_event.is_boundary && current_event.base_runs==6)
            msg1="[Batsman "+to_string(my_id)+"] SIX!";
        else if(current_event.is_boundary && current_event.base_runs==4)
            msg1="[Batsman "+to_string(my_id)+"] FOUR!";
        else if(current_event.is_leg_bye)
            msg1="[Batsman "+to_string(my_id)+"] Leg bye";
        else if(current_event.base_runs==0)
            msg1="[Batsman "+to_string(my_id)+"] Dot ball";
        else
            msg1="[Batsman "+to_string(my_id)+"] Playing for "+to_string(current_event.base_runs);

        Logger::log(msg1,"BATSMAN");

        // LBW: deferred umpire decision
        // must happen before we release pitch_mutex so that
        //  state is resolved before fielder/keeper wake.
        if (current_event.wicket == LBW) {
            bool lbw_out = decide_lbw();
            if (!lbw_out) {
                // NOT OUT — treat as dot ball; cancel the wicket
                current_event.wicket    = NONE;
                current_event.base_runs = 0;
            }
            // If OUT, current_event.wicket stays LBW and the normal wicket-processing block below handles it.
        }

        // waiting time: first execution 
        if (!has_started[my_id]) {
            start_time[my_id] = balls_bowled;
            waiting_time[my_id] = start_time[my_id] - arrival_time[my_id];
            has_started[my_id] = true;
        }

        pthread_mutex_unlock(&pitch_mutex);

        // stumped: wait for keeper confirmation
        if (current_event.wicket == STUMPED) {
            pthread_mutex_lock(&fielder_mutex);
            while (!keeper_done && match_running) {
                pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
            }
            pthread_mutex_unlock(&fielder_mutex);
        }

        if (current_event.ball_in_air) {
            // zone system: compute angle, sector, weighted owner
            pthread_mutex_lock(&fielder_mutex);

            shot_angle  = rand() % 360;
            shot_sector = shot_angle / 45;   // 0–7

            // apply handedness flip for left-handers
            // batsman_hand[id] == 1 means left-handed
            if (batsman_hand[id] == 1) {
                shot_sector = (shot_sector + 4) % 8;
            }

            // weighted selection: primary=70, adj-left=15, adj-right=15
            int primary   = shot_sector;
            int adj_left  = (shot_sector + 7) % 8;
            int adj_right = (shot_sector + 1) % 8;

            int weights[3] = {70, 15, 15};
            int candidates[3] = {primary, adj_left, adj_right};

            // map sector → fielder id (fielders 1–9, keeper=separate)
            // sector 0–7 maps to fielder ids 1–8; sector maps 1:1 here
            // adjust if your fielder numbering differs
            int r = rand() % 100;
            int chosen_sector;
            if (r < 70)       chosen_sector = candidates[0];
            else if (r < 85)  chosen_sector = candidates[1];
            else              chosen_sector = candidates[2];

            vector<int>& options = sector_to_fielders[chosen_sector];
            ball_owner = options[rand() % options.size()];
            ball_stopped=false;
            ball_active        = true;
            

            // commentary
            Logger::log(
                "[Batsman " + to_string(my_id) + "] hits to " +
                string(sector_name[chosen_sector]) +
                " for " + to_string(current_event.base_runs) + " run(s)",
                "BATTING"
            );

            pthread_cond_broadcast(&fielder_wake_cond);
            pthread_mutex_unlock(&fielder_mutex);

            pthread_mutex_lock(&fielder_mutex);
            while (!ball_stopped && match_running) {
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
        if (!current_event.is_wide) {
        balls_faced[my_id]++;
        runs_scored[my_id] += total_runs;
    }
        int running_runs = current_event.is_wide ? 0 : current_event.base_runs;

        bool attempted_run = (!current_event.is_boundary && !current_event.is_wide && current_event.wicket == NONE);

        if(attempted_run && ball_stopped && keeper_done){
            if(rand()%100<5){
                Logger::log("[RAG] Striker holds End1 → requests End2", "RAG");
                Logger::log("[RAG] Non-striker holds End2 → requests End1", "RAG");

                striker_mid_pitch = true;
                nonstriker_mid_pitch = true;

                striker_dist_run = rand()%22;
                nonstriker_dist_run = rand()%22;

                int striker_try = pthread_mutex_trylock(&end2_mutex);
                int nonstriker_try = pthread_mutex_trylock(&end1_mutex);

                bool deadlock = striker_mid_pitch && nonstriker_mid_pitch;

                if (deadlock){
                    Logger::log("[DEADLOCK] Circular wait detected!", "RAG");

                    float striker_score =(22.0f - striker_dist_run) * (1.0f / (1 + expected_balls[striker]));

                    float nonstriker_score =(22.0f - nonstriker_dist_run) * (1.0f / (1 + expected_balls[non_striker]));

                    int victim =(striker_score > nonstriker_score) ? striker : non_striker;

                    Logger::log( "[UMPIRE] RUN OUT! Batsman " + to_string(victim), "UMPIRE"
                    );

                    current_event.wicket = RUN_OUT;
                    current_event.base_runs = 0;
                }

                if (striker_try == 0)
                    pthread_mutex_unlock(&end2_mutex);

                if (nonstriker_try == 0)
                    pthread_mutex_unlock(&end1_mutex);

                striker_mid_pitch = false;
                nonstriker_mid_pitch = false;
            }
        }

        if (current_event.wicket != NONE) {
            completion_time[my_id] = balls_bowled + 1;
            turnaround_time[my_id] = completion_time[my_id] - arrival_time[my_id];
            sem_post(&crease_sem);
            pthread_mutex_lock(&score_mutex);
            wickets_fallen++;
            pthread_mutex_unlock(&score_mutex);
            // stop match if all out
                if (wickets_fallen == 10){
                    if (innings == 1)
                        Logger::log("[MATCH] Innings 1 complete!", "SYSTEM");
                    else
                        Logger::log("[MATCH] Team 2 all out!", "SYSTEM");

                    if (!current_event.is_wide && !current_event.is_no_ball) {

                        int b = get_current_bowler();
                        int runs = current_event.base_runs + current_event.extra_runs;

                        bowlers[b].balls_bowled++;
                        bowlers[b].runs_conceded += runs;
                        balls_bowled++;

                        if (current_event.wicket != NONE) {
                            bowlers[b].wickets++;
                        }
                    }
                record_extras(current_event);
                update_score(total_runs);

                // 2nd innings: traget chase check
                if (innings == 2 && global_score >= target_score) {
                    log_pitch_ball_completed(balls_bowled, wickets_fallen);
                    log_chase_requirement(balls_bowled, read_global_score());

                    Logger::log("[MATCH] Target chased successfully!", "SYSTEM");

                    match_running = false;

                    pthread_mutex_lock(&pitch_mutex);
                    stroke_done = true;
                    pthread_cond_signal(&stroke_finished);
                    pthread_mutex_unlock(&pitch_mutex);

                    continue;
                }

                log_pitch_ball_completed(balls_bowled, wickets_fallen);
                log_chase_requirement(balls_bowled, read_global_score());

                match_running = false;
                pthread_mutex_lock(&pitch_mutex);
                stroke_done = true;
                pthread_cond_signal(&stroke_finished);
                pthread_mutex_unlock(&pitch_mutex);
                break;
            }

            // wake all threads safely
            if (!match_running) {
                stop_match();

                pthread_mutex_lock(&pitch_mutex);
                stroke_done = true;
                pthread_cond_signal(&stroke_finished);
                pthread_mutex_unlock(&pitch_mutex);
            }
            
            if (!match_running) {
                stop_match();
                pthread_mutex_lock(&pitch_mutex);
                stroke_done = true;
                pthread_cond_signal(&stroke_finished);
                pthread_mutex_unlock(&pitch_mutex);

                continue;  
            }

            // only if match continues - create new batsman
            
            int new_id=0;

            // top order(batsman 3) 
            if (!batsman3_used) {
                new_id = 3;
                batsman3_used = true;

                arrival_time[3] = balls_bowled;

                Logger::log("[FIXED] Batsman 3 comes in (Top order)", "SCHED");
            } else {

            // hybrid order: fixed+scheduler
            if (use_sjf) {

                if (!batting_order_sjf.empty()) {
                    new_id = batting_order_sjf.top().second;
                    batting_order_sjf.pop();

                    Logger::log("[SJF] Middle-order batsman " + to_string(new_id) + " comes in", "SCHED");

                } else if (next_fixed <= 11) {

                    new_id = next_fixed;
                    next_fixed++;

                    Logger::log("[TAIL] Batsman " + to_string(new_id) + " comes in", "SCHED");

                } else {
                    new_id = 11;
                }

            } else {

                if (!batting_order_fcfs.empty()) {
                    new_id = batting_order_fcfs.front();
                    batting_order_fcfs.pop();

                    Logger::log("[FCFS] Middle-order batsman " + to_string(new_id) + " comes in", "SCHED");

                } else if (next_fixed <= 11) {

                    new_id = next_fixed;
                    next_fixed++;

                    Logger::log("[TAIL] Batsman " + to_string(new_id) + " comes in", "SCHED");

                } else {
                    new_id = 11;
                }
            }
        }
        // waiting time: arrival
            if (arrival_time[new_id] == -1){
                arrival_time[new_id] = balls_bowled;
            }

            Logger::log(
                "[Outcome] WICKET - "+string(wicket_name(current_event.wicket)),
                "WICKET"
            );
            
            record_extras(current_event);
            update_score(total_runs);

            sem_wait(&crease_sem);   // new batsman occupies crease

            my_id = new_id;
            striker = my_id;

            pthread_cond_broadcast(&ball_delivered);


            pthread_mutex_lock(&pitch_mutex);
            stroke_done = true;
            pthread_cond_signal(&stroke_finished);
            pthread_mutex_unlock(&pitch_mutex);

            continue;
        }

        string msg2;

        if(current_event.is_overthrow)
            msg2="[Outcome] OVERTHROW!";
        else if(current_event.is_boundary)
            msg2="[Outcome] BOUNDARY - "+to_string(current_event.base_runs);
        else if(current_event.is_wide)
            msg2="[Outcome] Wide";
        else if(current_event.is_no_ball)
            msg2="[Outcome] No Ball";
        else if(current_event.is_leg_bye)
            msg2="[Outcome] Leg Bye";
        else if(total_runs==0)
            msg2="[Outcome] Dot ball";
        else
            msg2="[Outcome] "+to_string(total_runs)+" run(s)";

        Logger::log(msg2,"OUTCOME");

        record_extras(current_event);
        update_score(total_runs);

        if (innings == 2 && global_score >= target_score) {
            int projected_balls = balls_bowled;
            if (!current_event.is_wide && !current_event.is_no_ball) {
                projected_balls++;
            }

            log_pitch_ball_completed(projected_balls, wickets_fallen);
            log_chase_requirement(projected_balls, read_global_score());
            Logger::log("[MATCH] Target chased successfully!", "SYSTEM");
            match_running = false;

            pthread_mutex_lock(&pitch_mutex);
            stroke_done = true;
            pthread_cond_signal(&stroke_finished);
            pthread_mutex_unlock(&pitch_mutex);
            continue;
        }

        if (current_event.wicket == NONE && running_runs % 2 == 1) {
            pthread_mutex_lock(&score_mutex);
            int tmp     = striker;
            striker     = non_striker;
            non_striker = tmp;
            pthread_mutex_unlock(&score_mutex);
        }

        pthread_mutex_lock(&pitch_mutex);
        stroke_done = true;
        pthread_cond_signal(&stroke_finished);
        pthread_mutex_unlock(&pitch_mutex);
    }
    return NULL;
}

// fielder_thread()
//   Wakes on ball_active, races to claim the ball.
//
//   CAUGHT handling:
//     a fielder who grabs a CAUGHT ball sets ball_stopped = true, which is the signal batsman_thread waits on. The wicket
//     itself was already set in generate_event(); so the fielder only needs to confirm ball possession.

void* fielder_thread(void* arg) {
    int id = *(int*)arg;
    unsigned int rseed = (unsigned)(time(NULL) + id * 7919u)
                       ^ (unsigned)((unsigned long)pthread_self() & 0xFFFFFFFFUL)
                       ^ ((unsigned)id * 1234567891u)
                       ^ ((unsigned)(id * id) * 6700417u);

    while (match_running) {
        pthread_mutex_lock(&fielder_mutex);

        while (match_running && !(ball_active && ball_owner == id)) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }

        if (!match_running) {
            pthread_mutex_unlock(&fielder_mutex);
            break;
        }

        ball_active = false;
        pthread_mutex_unlock(&fielder_mutex);

        // simulate fielding
        Logger::log(
            "[Fielder " + to_string(id) + "] collects the ball",
            "FIELDER"
        );

        // mark ball stopped
        pthread_mutex_lock(&fielder_mutex);
        ball_stopped = true;
        pthread_cond_broadcast(&fielder_wake_cond);
        pthread_mutex_unlock(&fielder_mutex);
    }
    return NULL;
}

// wicket_keeper_thread()
//   handles stumped dismissals exclusively.
//   dedicated interrupt handler for a specific interrupt vector (STUMPED).  It only acts if the ball has
//   not already been claimed (ball_stopped == false), preventing a race with fielders.

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
            // stumped: keeper acts only if ball not already stopped (guards against a fielder accidentally claiming the ball when wicket == STUMPED, which fielder_thread now prevents)
            if (!ball_stopped) {
                Logger::log("[Keeper] STUMPED!","KEEPER");

                ball_owner   = 0;   // 0 = keeper owns the ball
                ball_stopped = true;
                pthread_cond_broadcast(&fielder_wake_cond);
            }
        } else {
            // non-stumped ball: keeper receives safely if no fielder got it
            if (ball_owner == -1 && !ball_stopped) {
                Logger::log("[Keeper] Ball safely in gloves","KEEPER");
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
