#include "player_threads_2.h"
#include "../critical_section_2/pitch_2.h"
#include <iostream>
#include <unistd.h>
#include <cstdlib>
#include <ctime>
#include <pthread.h>

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

void* bowler_thread(void* arg) {
    while (match_running) {
        pthread_mutex_lock(&pitch_mutex);

        if (!match_running) {
            pthread_mutex_unlock(&pitch_mutex);
            break;
        }

        current_event = generate_event();

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
            cout << "[Bowler] Delivering ball " << (balls_bowled + 1) << endl;
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
                balls_bowled++;
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
            cout << "[Batsman " << my_id << "] LBW appeal!" << endl;
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

        pthread_mutex_unlock(&pitch_mutex);

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
            pthread_mutex_unlock(&score_mutex);

            static int next_batsman_id = 3;
            int new_id = next_batsman_id++;

            pthread_mutex_lock(&print_mutex);
            cout << "[Outcome] WICKET - " << wicket_name(current_event.wicket) << "!" << endl;
            cout << "[Innings] Batsman " << new_id << " comes to crease" << endl;
            pthread_mutex_unlock(&print_mutex);

            sem_wait(&crease_sem);   // ✅ new batsman occupies crease

            my_id = new_id; 
            int old_non_striker = non_striker;
            striker = my_id;
            non_striker = old_non_striker;

            pthread_cond_broadcast(&ball_delivered);

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

        if (!ball_stopped && ball_owner == -1 && current_event.wicket != STUMPED) {
            ball_owner = id;
            int dist = 5 + (int)(rand_r(&rseed) % 55);

            pthread_mutex_lock(&print_mutex);
            if (current_event.wicket == CAUGHT) {
                cout << "[Fielder " << id << "] CAUGHT!" << endl;
            } else if (current_event.wicket == RUN_OUT) {
                cout << "[Fielder " << id << "] Direct hit - RUN OUT attempt at " << dist << "m" << endl;
            } else {
                cout << "[Fielder " << id << "] Stopped ball at " << dist << "m" << endl;
            }
            pthread_mutex_unlock(&print_mutex);

            ball_stopped = true;
            pthread_cond_broadcast(&fielder_wake_cond);
        }

        while (ball_active && match_running) {
            pthread_cond_wait(&fielder_wake_cond, &fielder_mutex);
        }

        pthread_mutex_unlock(&fielder_mutex);
    }
    return NULL;
}

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

        if (current_event.wicket == STUMPED || ball_owner == -1) {
            pthread_mutex_lock(&print_mutex);
            if (current_event.wicket == STUMPED) {
                cout << "[Keeper] STUMPED!" << endl;
            } else {
                cout << "[Keeper] Ball safely in gloves" << endl;
            }
            pthread_mutex_unlock(&print_mutex);

            if (current_event.wicket == STUMPED && !ball_stopped) {
                ball_owner   = 0;
                ball_stopped = true;
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