#pragma once
#include <fstream>
#include <string>
#include <pthread.h>

// ===================================================
// Logger — thread-safe dual output (cout + log file)
// ===================================================
// Usage:
//   Logger::log("[Bowler] Delivering ball 5");
//   Logger::log("[Batsman 1] SIX!", "BATSMAN");
//
// Drop-in replacement for:
//   pthread_mutex_lock(&print_mutex);
//   cout << "..." << endl;
//   pthread_mutex_unlock(&print_mutex);
// becomes:
//   Logger::log("...");
// ===================================================

class Logger {
public:
    // Call once at startup (before threads are created)
    static void init(const std::string& filename = "log.txt");

    // Call once at shutdown (after threads are joined)
    static void close();

    // Thread-safe log — writes to both cout and log file
    // tag is optional context label (e.g. "BOWLER", "BATSMAN", "FIELDER")
    static void log(const std::string& message, const std::string& tag = "");

    // Log a section separator (makes log easier to scan)
    static void section(const std::string& title);

private:
    static std::ofstream   file_;
    static pthread_mutex_t mutex_;
    static bool            initialized_;

    static std::string timestamp();
};