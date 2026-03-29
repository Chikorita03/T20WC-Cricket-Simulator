#ifndef THREAD_MANAGER_H
#define THREAD_MANAGER_H

//creates all player threads (bowler, batsmen, fielders and keeper)
void create_all_threads();

//wait for all threads to finish before exiting
void join_all_threads();

#endif