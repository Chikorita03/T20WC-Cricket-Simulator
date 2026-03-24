#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

// Creates all player threads (bowler, batsmen, fielders, keeper).
void create_all_threads();

// Joins all player threads — call after setting match_running = false.
void join_all_threads();

#endif