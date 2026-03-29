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
int balls_in_over  = 0;
// Internal indices are 0-based, so 2/3 map to printed bowlers 3/4.
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
static constexpr float DEATH_OVERS_INTENSITY_THRESHOLD = 0.80f; // ~last 20%

static void refresh_match_intensity() {
    float raw = static_cast<float>(balls_bowled) / static_cast<float>(TOTAL_BALLS);
    match_intensity = std::clamp(raw, 0.0f, 1.0f);
}

static bool in_death_phase() {
    return match_intensity >= DEATH_OVERS_INTENSITY_THRESHOLD;
}

// ===== ROUND ROBIN: Bowler PCBs =====
BowlerPCB bowlers[NUM_BOWLERS];

void init_scheduler() {
    current_bowler = 0;
    balls_in_over  = 0;
    match_intensity = 0.0f;

    // ===== Initialize Bowler PCBs =====
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

// =============================================================
// decide_lbw() — declared here so umpire.h can expose it.
//
// Implementation lives in player_threads_2.cpp where it is
// called from batsman_thread().  This forward-declaration in
// umpire.cpp keeps the scheduling module aware of the LBW
// adjudication function without duplicating logic.
//
// OS analogy: The umpire is the kernel scheduler.  decide_lbw()
// is a privileged system call — only the "kernel" (umpire) may
// authoritatively resolve an LBW appeal, and batsman_thread()
// invokes it like a syscall, blocking until a verdict is returned.
// =============================================================
// (decide_lbw body is in player_threads_2.cpp; see umpire.h
//  for the extern declaration used across translation units.)

void on_ball_completed() {
    // Gantt should reflect who was on strike at the start of the ball.
    const int ball_number = delivery_start_ctx.valid ? delivery_start_ctx.ball : balls_bowled;
    const int bowler_for_ball = delivery_start_ctx.valid ? delivery_start_ctx.bowler : (current_bowler + 1);
    const int striker_for_ball = delivery_start_ctx.valid ? delivery_start_ctx.striker : striker;
    const int non_striker_for_ball = delivery_start_ctx.valid ? delivery_start_ctx.non_striker : non_striker;
    delivery_start_ctx.valid = false;

    balls_in_over++;
    refresh_match_intensity();

    // ===== PRIORITY: HIGH-INTENSITY PHASE (DEATH OVERS) =====
    if (in_death_phase()) {

        if (balls_in_over >= OVER_BALLS) {

            balls_in_over = 0;

            static int toggle = 0;

            if (toggle == 0)
                current_bowler = death_bowler_1;
            else
                current_bowler = death_bowler_2;

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
    
    // ===== NORMAL RR =====
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
            // Max 4 overs for ALL bowlers
            if (bowlers[candidate].balls_bowled >= MAX_BALLS_PER_BOWLER)
            continue;

            // Death bowlers are conserved in low intensity and released
            // gradually as the match intensity rises.
            int pre_death_cap_balls = static_cast<int>((2.0f + match_intensity * 2.0f) * OVER_BALLS);
            if ((candidate == death_bowler_1 || candidate == death_bowler_2) &&
                !in_death_phase() &&
                bowlers[candidate].balls_bowled >= pre_death_cap_balls) continue;
            current_bowler = candidate;
            break;
        }
        // SAFETY FALLBACK
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
    
