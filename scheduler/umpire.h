#pragma once

#define NUM_BOWLERS 5
#define OVER_BALLS 6

extern int current_bowler;
extern int balls_in_over;
extern int death_bowler_1;
extern int death_bowler_2;
extern float match_intensity;

//PCB for each bowler to store stats
struct BowlerPCB {
    int runs_conceded;
    int balls_bowled;
    int wickets;
};

extern BowlerPCB bowlers[NUM_BOWLERS];

void init_scheduler();
void on_ball_completed();//called when a ball is completed, to update the bowler and schedule the next one.
int get_current_bowler();
void record_delivery_start_context(int ball, int bowler, int striker, int non_striker);

//to decide if a LBW decision should be given based on the current match context
bool decide_lbw();
