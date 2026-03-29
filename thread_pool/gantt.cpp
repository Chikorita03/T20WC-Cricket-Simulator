#include "gantt.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

vector<GanttEvent> gantt_log;
pthread_mutex_t gantt_mutex = PTHREAD_MUTEX_INITIALIZER;

static constexpr int ROW_LABEL_WIDTH = 8;
static constexpr int CELL_WIDTH = 6;

static string center_text(const string& text, int width) {
    if (static_cast<int>(text.size()) >= width) return text;
    int total_pad = width - static_cast<int>(text.size());
    int left_pad = total_pad / 2;
    int right_pad = total_pad - left_pad;
    return string(left_pad, ' ') + text + string(right_pad, ' ');
}

static string batsman_token(int batsman_id) {
    if (batsman_id <= 0) return "";
    return "BT" + to_string(batsman_id);
}

static void print_row(
    const string& label,
    const vector<GanttEvent>& events,
    int start,
    int end,
    const string& kind,
    const vector<int>* slot1_active = nullptr,
    const vector<int>* slot2_active = nullptr
) {
    cout << center_text(label, ROW_LABEL_WIDTH);
    for (int i = start; i < end; i++) {
        string value;
        if (kind == "ball") value = to_string(events[i].ball);
        else if (kind == "bowler") value = "B" + to_string(events[i].bowler);
        else if (kind == "bat1") value = batsman_token((*slot1_active)[i]);
        else if (kind == "bat2") value = batsman_token((*slot2_active)[i]);
        cout << "|" << center_text(value, CELL_WIDTH);
    }
    cout << "|\n";
}

// log per ball
void log_gantt(int ball, int bowler, int striker, int non_striker) {

    pthread_mutex_lock(&gantt_mutex);

    gantt_log.push_back({
        ball,
        bowler,
        striker,
        non_striker
    });
    pthread_mutex_unlock(&gantt_mutex);
}

// print clean grid
void print_gantt_chart() {
    pthread_mutex_lock(&gantt_mutex);

    cout << "\n========== GANTT CHART ==========\n";

    const int total_balls = static_cast<int>(gantt_log.size());
    if (total_balls == 0) {
        cout << "(No legal deliveries logged)\n";
        pthread_mutex_unlock(&gantt_mutex);
        return;
    }

    const int overs = static_cast<int>(ceil(total_balls / 6.0));
    vector<int> slot1_active(total_balls, 0);
    vector<int> slot2_active(total_balls, 0);

    int slot1 = gantt_log[0].striker;
    int slot2 = gantt_log[0].non_striker;

    for (int i = 0; i < total_balls; i++) {
        const int striker = gantt_log[i].striker;
        const int non_striker = gantt_log[i].non_striker;

        // keep two persistent batting slots. If a new batsman appears (wicket), replace only the slot that no longer matches the current pair.
        const bool striker_in_slots = (striker == slot1 || striker == slot2);
        const bool non_striker_in_slots = (non_striker == slot1 || non_striker == slot2);

        if (!striker_in_slots && non_striker == slot1) {
            slot2 = striker;
        } else if (!striker_in_slots && non_striker == slot2) {
            slot1 = striker;
        }

        if (!non_striker_in_slots && striker == slot1) {
            slot2 = non_striker;
        } else if (!non_striker_in_slots && striker == slot2) {
            slot1 = non_striker;
        }

        // fallback if both changed unexpectedly.
        if ((striker != slot1 && striker != slot2) ||
            (non_striker != slot1 && non_striker != slot2)) {
            slot1 = striker;
            slot2 = non_striker;
        }

        // only striker's slot is shown as active for this ball.
        if (striker == slot1) {
            slot1_active[i] = slot1;
            slot2_active[i] = 0;
        } else {
            slot1_active[i] = 0;
            slot2_active[i] = slot2;
        }
    }

    for (int o = 0; o < overs; o++) {
        const int start = o * 6;
        const int end = min(start + 6, total_balls);
        const int balls_this_over = end - start;
        const int table_width = ROW_LABEL_WIDTH + (balls_this_over * (CELL_WIDTH + 1)) + 1;
        const string border(table_width, '-');

        cout << "\nOver " << o + 1 << " (" << balls_this_over << " ball(s)):\n";
        cout << border << "\n";
        print_row("Ball", gantt_log, start, end, "ball");
        print_row("Bowler", gantt_log, start, end, "bowler");
        print_row("Batsman 1", gantt_log, start, end, "bat1", &slot1_active, nullptr);
        print_row("Batsman 2", gantt_log, start, end, "bat2", nullptr, &slot2_active);
        cout << border << "\n";

    }

    pthread_mutex_unlock(&gantt_mutex);
}

void clear_gantt_chart() {
    pthread_mutex_lock(&gantt_mutex);
    gantt_log.clear();
    pthread_mutex_unlock(&gantt_mutex);
}
