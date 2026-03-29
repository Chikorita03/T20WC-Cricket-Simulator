# T20 Cricket Simulator

A multi-threaded T20 cricket simulation engine with realistic gameplay, scheduling algorithms, and match statistics.

## Overview

This project simulates a complete T20 cricket match using POSIX threads to model concurrent on-field actions including bowler-batsman interactions, fielding dynamics, and umpire decisions.

## Features

- **Complete T20 Match** - Full 20-over innings with realistic cricket rules
- **Multi-threaded Architecture** - Separate threads for bowler, batsmen, fielders, and wicket-keeper
- **Two Scheduling Algorithms** - FCFS and SJF for middle-order batting
- **5 Types of Dismissals** - Bowled, Caught, Run Out, LBW (60% out rate), Stumped
- **Zone-Based Shot Direction** - 8 sectors with weighted probability (70/15/15) and left-handed adjustment
- **Probabilistic Batting** - Different probability tables for openers, middle-order, and tail-enders
- **Realistic Mechanics** - Wides, no-balls, free hits, boundaries, overthrows, leg byes
- **Death Overs Strategy** - Priority scheduling for last 4 overs
- **Deadlock Simulation** - Run-out scenarios with circular wait detection
- **Comprehensive Logging** - Thread-safe logging with timestamps
- **Gantt Chart** - Ball-by-ball visualization
- **Performance Analytics** - Waiting time, turnaround time, strike rate tracking

## Technical Stack

- **Language**: C++11
- **Concurrency**: POSIX threads (pthreads), mutexes, condition variables, semaphores
- **Platform**: Linux/Unix systems

## Project Structure

```
t20-simulator/
├── main.cpp                 # Entry point, innings management
├── critical_section_2/
│   ├── pitch_2.cpp/h       # Pitch state, ball generation, zone mapping
├── thread_pool/
│   ├── thread_manager.cpp/h # Thread creation/joining
│   ├── player_threads_2.cpp/h # Player logic
│   ├── gantt.cpp/h         # Gantt chart generation
├── scheduler/
│   ├── umpire.cpp/h        # Bowler scheduling
├── log.cpp/h               # Thread-safe logging
```

## Getting Started

### Prerequisites

- Linux/Unix OS (or WSL on Windows)
- GCC/G++ with pthread support

### Compilation

```bash
g++ -pthread -std=c++11 main.cpp critical_section_2/pitch_2.cpp thread_pool/thread_manager.cpp thread_pool/player_threads_2.cpp thread_pool/gantt.cpp scheduler/umpire.cpp log.cpp -o t20_simulator
```

### Running

```bash
./t20_simulator
```

Select scheduling algorithm when prompted:
- **FCFS** - Batsmen 4,5,6,7,8 in order
- **SJF** - Batsmen ordered by expected balls faced (lowest first)

## How It Works

### Match Flow
1. **First Innings** - Team 1 bats 20 overs
2. **Innings Break** - Target calculated
3. **Second Innings** - Team 2 chases
4. **Result** - Winner announced

### Thread Architecture

| Thread | Count | Responsibility |
|--------|-------|----------------|
| Bowler | 1 | Generates ball events |
| Batsmen | 2 | Strike/non-strike rotation |
| Fielders | 9 | Chase airborne balls |
| Keeper | 1 | Handles stumpings |

### Zone-Based Fielding System

- **8 sectors** - fine leg, square leg, mid-wicket, long on, long off, cover, point, third man
- **Angle generation** - Random 0-360° converted to sector
- **Left-handed adjustment** - Sector rotated 180°
- **Weighted selection** - 70% original, 15% left adjacent, 15% right adjacent
- **Single fielder activation** - Eliminates race conditions

## Output

### Console Output
```
--- Over 19.6 | Ball 120 ---
[Bowler 4] Delivering ball 120
[Batsman 6] Dot ball
[Outcome] Dot ball
[Score] +0 run(s) Total: 229
[Pitch] Ball completed | Balls: 120 | Wickets: 5 | Extras: 11
[STATS] CRR: 11.45
[PRIORITY] High intensity phase! Bowler 4 is bowling (intensity: 1.000000)
[MATCH] Innings 1 complete! 20 overs finished.
```

### Gantt Chart Example
```
Over 1 (6 ball(s)):
---------------------------------------------
  Ball  |  1  |  2  |  3  |  4  |  5  |  6  |
 Bowler | B1  | B1  | B1  | B1  | B1  | B1  |
 BTM 1  | BT1 | BT1 | BT1 | BT1 | BT1 | BT1 |
 BTM 2  |     | BT2 | BT2 |     |     |     |
---------------------------------------------
```

### Statistics Tracked
- **Per Batsman**: Runs, balls faced, waiting time, turnaround time, strike rate
- **Per Bowler**: Overs, runs, wickets
- **Match**: Run rates, extras breakdown (wides, no-balls, byes, leg-byes)

## Configuration

Modify constants in code:
- `TOTAL_OVERS` - Match length (default: 20)
- `NUM_BOWLERS` - Available bowlers (default: 5)
- `MAX_BALLS_PER_BOWLER` - Max overs per bowler (default: 24 balls)
- Probability distributions in `generate_event()`

## License

Educational purposes only.
