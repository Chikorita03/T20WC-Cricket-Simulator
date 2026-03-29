#include "log.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <ctime>

// static member definitions 
std::ofstream Logger::file_;
pthread_mutex_t Logger::mutex_  = PTHREAD_MUTEX_INITIALIZER;
bool Logger::initialized_ = false;

void Logger::init(const std::string& filename) {
    pthread_mutex_lock(&mutex_);

    file_.open(filename, std::ios::out | std::ios::trunc);
    if (!file_.is_open()) {
        std::cerr << "[Logger] WARNING: could not open " << filename << std::endl;
    } else {
        initialized_ = true;
        file_ << "========================================\n";
        file_ << "  T20 Cricket Simulator — Match Log\n";
        file_ << "  Started: " << timestamp() << "\n";
        file_ << "========================================\n\n";
        file_.flush();
    }

    pthread_mutex_unlock(&mutex_);
}

void Logger::close() {
    pthread_mutex_lock(&mutex_);

    if (initialized_ && file_.is_open()) {
        file_ << "\n========================================\n";
        file_ << "  Match Ended: " << timestamp() << "\n";
        file_ << "========================================\n";
        file_.close();
        initialized_ = false;
    }

    pthread_mutex_unlock(&mutex_);
}

void Logger::log(const std::string& message, const std::string& tag) {
    pthread_mutex_lock(&mutex_);

    std::ostringstream line;
    line << "[" << timestamp() << "]";
    if (!tag.empty()) {
        line << "[" << std::setw(8) << std::left << tag << "]";
    } else {
        line << "          "; //align with tagged lines
    }
    line << " " << message;

    std::string formatted = line.str();

    //write to console 
    std::cout << message << std::endl;

    //write richer formatted line to log file
    if (initialized_ && file_.is_open()) {
        file_ << formatted << "\n";
        file_.flush();   //flush every line so log is readable mid-match
    }

    pthread_mutex_unlock(&mutex_);
}

void Logger::section(const std::string& title) {
    pthread_mutex_lock(&mutex_);

    std::string sep = "\n--- " + title + " ---";

    std::cout << sep << std::endl;

    if (initialized_ && file_.is_open()) {
        file_ << "\n";
        file_ << "════════════════════════════════════════\n";
        file_ << "  " << title << "  [" << timestamp() << "]\n";
        file_ << "════════════════════════════════════════\n";
        file_.flush();
    }

    pthread_mutex_unlock(&mutex_);
}

std::string Logger::timestamp() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    struct tm tm_info;
    localtime_r(&ts.tv_sec, &tm_info);

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld",
             tm_info.tm_hour,
             tm_info.tm_min,
             tm_info.tm_sec,
             ts.tv_nsec / 1000000L);
    return std::string(buf);
}