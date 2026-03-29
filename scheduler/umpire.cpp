#include "umpire.h"
#include <iostream>
#include "log.h"
#include <sstream>
#include <algorithm>
#include <pthread.h>
#include "../thread_pool/gantt.h"
#include "../critical_section_2/pitch_2.h"

using namespace std;

int current_bowler = 0;
int balls_in_over = 0;
int death_bowler_1 = 2;
int death_bowler_2 = 3;
float match_intensity = 0.0f;
extern int balls_bowled;

struct DeliveryStartContext {
    int ball = 0;
    int bowler = 0;
    int striker = 0;
    int non_striker = 0;
    bool valid = false;
};

static DeliveryStartContext delivery_start_ctx;

static constexpr int TOTAL_OVERS = 20;
static constexpr int TOTAL_BALLS = TOTAL_OVERS * OVER_BALLS;
static constexpr int MAX_BALLS_PER_BOWLER = 4 * OVER_BALLS;
static constexpr int PRE_DEATH_CAP_BALLS = 2 * OVER_BALLS;
static constexpr float DEATH_OVERS_INTENSITY_THRESHOLD = 0.80f; //last 20% of the innings is death overs

static void refresh_match_intensity() {
    float raw = static_cast<float>(balls_bowled) / static_cast<float>(TOTAL_BALLS);
    match_intensity = std::clamp(raw, 0.0f, 1.0f);
}

static bool in_death_phase() {
    return match_intensity >= DEATH_OVERS_INTENSITY_THRESHOLD;
}

//to store bowler stats
BowlerPCB bowlers[NUM_BOWLERS];

void init_scheduler() {
    current_bowler = 0;
    balls_in_over = 0;
    match_intensity = 0.0f;

for (int i = 0; i < NUM_BOWLERS; i++) {
    bowlers[i].runs_conceded = 0;
    bowlers[i].balls_bowled = 0;
    bowlers[i].wickets = 0;
}
}

int get_current_bowler() {
    return current_bowler;
}

void record_delivery_start_context(int ball, int bowler, int striker_id, int non_striker_id) {
    delivery_start_ctx.ball = ball;
    delivery_start_ctx.bowler = bowler;
    delivery_start_ctx.striker = striker_id;
    delivery_start_ctx.non_striker = non_striker_id;
    delivery_start_ctx.valid = true;
}

void on_ball_completed() {
    //gantt should reflect who was on strike at the start of the ball.
    const int ball_number = delivery_start_ctx.valid ? delivery_start_ctx.ball : balls_bowled;
    const int bowler_for_ball = delivery_start_ctx.valid ? delivery_start_ctx.bowler : (current_bowler + 1);
    const int striker_for_ball = delivery_start_ctx.valid ? delivery_start_ctx.striker : striker;
    const int non_striker_for_ball = delivery_start_ctx.valid ? delivery_start_ctx.non_striker : non_striker;
    delivery_start_ctx.valid = false;

    balls_in_over++;
    refresh_match_intensity();

    //death over priority scheduling
    if (in_death_phase()) {
        if (balls_in_over >= OVER_BALLS) {
            balls_in_over = 0;
            static int toggle = 0;
            int preferred = (toggle == 0) ? death_bowler_1 : death_bowler_2;
            int alternate = (toggle == 0) ? death_bowler_2 : death_bowler_1;

            if (bowlers[preferred].balls_bowled < MAX_BALLS_PER_BOWLER) {
                current_bowler = preferred;
            } else if (bowlers[alternate].balls_bowled < MAX_BALLS_PER_BOWLER) {
                current_bowler = alternate;
            } else {
                //safety fallback: choose any bowler with balls remaining
                for (int i = 0; i < NUM_BOWLERS; i++) {
                    if (bowlers[i].balls_bowled < MAX_BALLS_PER_BOWLER) {
                        current_bowler = i;
                        break;
                    }
                }
            }

            toggle = 1 - toggle;

            Logger::log(
                "[PRIORITY] High intensity phase! Bowler " + to_string(current_bowler + 1) +
                " is bowling (intensity: " + to_string(match_intensity) + ")",
                "UMPIRE"
            );
        }

        log_gantt(ball_number, bowler_for_ball, striker_for_ball, non_striker_for_ball);
        return;
    }
    
    //round robin scheduling
    if (balls_in_over >= OVER_BALLS) {

        int over_number = balls_bowled / 6;
        Logger::section("Over " + to_string(over_number) +" Completed");

        {
            string msg =
                "[UMPIRE] Over completed. Saving Bowler " + to_string(current_bowler+1) +
                " | Balls: " + to_string(bowlers[current_bowler].balls_bowled) +
                " | Runs: " + to_string(bowlers[current_bowler].runs_conceded) +
                " | Wickets: " + to_string(bowlers[current_bowler].wickets);

            Logger::log(msg, "UMPIRE");
        }

        balls_in_over = 0;
        int next = (current_bowler + 1) % NUM_BOWLERS;

        for (int i = 0; i < NUM_BOWLERS; i++) {
            int candidate = (next + i) % NUM_BOWLERS;
            //max 4 overs for each bowlers
            if (bowlers[candidate].balls_bowled >= MAX_BALLS_PER_BOWLER)
            continue;

            //before death phase, death bowlers can bowl at most 2 overs (12 balls).
            if ((candidate == death_bowler_1 || candidate == death_bowler_2) &&
                !in_death_phase() &&
                bowlers[candidate].balls_bowled >= PRE_DEATH_CAP_BALLS) continue;
            current_bowler = candidate;
            break;
        }
        //safety fallback if all have bowled max balls
        if (bowlers[current_bowler].balls_bowled >= MAX_BALLS_PER_BOWLER) {
            for (int i = 0; i < NUM_BOWLERS; i++) {
                if (bowlers[i].balls_bowled < MAX_BALLS_PER_BOWLER) {
                    current_bowler = i;
                    break;
                }
         }
        }

        {
            string msg =
                "[UMPIRE] New bowler: " + to_string(current_bowler+1) +
                " | Balls: " + to_string(bowlers[current_bowler].balls_bowled) +
                " | Runs: " + to_string(bowlers[current_bowler].runs_conceded) +
                " | Wickets: " + to_string(bowlers[current_bowler].wickets);

            Logger::log(msg, "UMPIRE");
        }
    }
    log_gantt(ball_number, bowler_for_ball, striker_for_ball, non_striker_for_ball);
}
    
