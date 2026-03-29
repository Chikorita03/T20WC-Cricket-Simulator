#pragma once
#include <fstream>
#include <string>
#include <pthread.h>

//Simple thread-safe logger that writes to both console and a log file with timestamps and optional tags.
class Logger {
public:
    //call once at startup (before threads are created)
    static void init(const std::string& filename = "log.txt");

    //call once at shutdown (after threads are joined)
    static void close();

    //thread-safe log — writes to both cout and log file
    static void log(const std::string& message, const std::string& tag = "");

    //log a section separator (makes log easier to scan)
    static void section(const std::string& title);

private:
    static std::ofstream file_;
    static pthread_mutex_t mutex_;
    static bool initialized_;

    static std::string timestamp();
};