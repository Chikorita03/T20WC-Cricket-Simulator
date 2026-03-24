#pragma once
#include <pthread.h>

// ============================================================
//  THREAD FUNCTIONS
// ============================================================

// Bowler: owns the pitch (critical section) for one full ball lifecycle.
// Generates a BallEvent, signals batsman, waits for stroke_finished.
void* bowler_thread(void* arg);

// Batsman: two instances share this function (id = 1 or 2).
// Only the striker (id == striker) plays the shot; the non-striker
// participates only in running between creases.
void* batsman_thread(void* arg);

// Fielder: sleeps on fielder_wake_cond until ball_in_air == true,
// then simulates catch / run-out attempt before returning to position.
void* fielder_thread(void* arg);

// Wicket Keeper: same wait pattern as fielder but additionally handles
// stumping and behind-the-wicket catches.
void* wicket_keeper_thread(void* arg);

// ============================================================
//  HELPER DECLARATIONS
// ============================================================

// Returns a human-readable string for a WicketType value.
const char* wicket_name(int wt);

// Returns a human-readable string for a BallType value.
const char* ball_type_name(int bt);