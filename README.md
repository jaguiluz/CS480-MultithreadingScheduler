# Concurrent Task Scheduler with Bounded Queue
A multithreaded task scheduling system implemented in C++ using a producer-consumer architecture with POSIX threads, mutexes, condition variables, semaphores, and atomic operations.

The project simulates a concurrent execution pipeline where producer threads generate tasks and consumer threads process them while enforcing queue capacity and VIP task constraints through synchronization primitives.

## Overview
This project demonstrates the design and implementation of a multithreaded task execution system that coordinates multiple worker threads through a shared task queue.

The system consists of:
- Two producer threads that generate tasks
- Two consumer threads that execute tasks
- A shared FIFO work queue
- Synchronization mechanisms to prevent race conditions and enforce capacity limits

The implementation focuses on thread coordination, synchronization primitives, and safe concurrent access to shared resources.

## Project Structure
```bash
.
├── .gitignore
├── consumer.cpp
├── consumer.h
├── log.cpp
├── log.h
├── main.cpp
├── Makefile
├── producer.cpp
├── producer.h
└── task_types.h
```

## Key Features
- Multithreaded producer-consumer architecture
- Thread-safe shared task queue with a 20-task limit
- VIP task quota enforcement limiting the queue to 6 VIP tasks at any given time
- POSIX thread (pthreads) implementation
- Mutex-protected critical sections
- Condition variables for thread signaling
- Semaphores for thread coordination and program termination
- Atomic counters for thread-safe statistics tracking
- FIFO task scheduling behavior

## System Architecture
```text
             Producer Threads
          +--------------------+
          | General Producer   |
          | VIP Producer       |
          +---------+----------+
                    |
                    v
          +--------------------+
          |  Bounded Task Queue |
          |  Capacity: 20      |
          |  VIP Limit: 6      |
          +---------+----------+
                    |
          +---------+----------+
          | Consumer Threads   |
          | T-X Worker         |
          | Rev-9 Worker       |
          +--------------------+
```

The system uses a bounded shared task queue with the following constraints:

- Maximum queue capacity: 20 tasks
- Maximum VIP tasks in queue: 6
- Remaining capacity may be occupied by general tasks

These constraints require producers to coordinate through synchronization primitives to ensure queue limits are never exceeded.

### Producers
Two producer threads continuously generate tasks and place them into the shared queue:
- General Task Producer
- VIP Task Producer

Producers block when:
- The queue reaches maximum capacity
- The VIP task limit has been reached

### Consumers
Two consumer threads remove tasks from the queue and process them:
- T-X Worker
- Rev-9 Worker

Consumers block when no tasks are available in the queue.

### Synchronization Components
| Component | Purpose |
|---|---|
| Mutex | Protects access to the shared queue. |
| Condition Variables | Coordinate producer and consumer execution. |
| Semaphores | Signal application completion. |
| Atomic Variables | Maintain thread-safe statistics. |

## Queue Management and Synchronization
The shared buffer enforces two independent constraints:
1. Total queue capacity is limited to 20 tasks.
2. No more than 6 VIP tasks may exist in the queue simultaneously

When either limit is reached:
- Producers block and wait on condition variables.
- Consumers continue processing available tasks.
- Producers are awakened when sufficient space becomes available.

This design prevents buffer overflow while ensuring VIP tasks cannot monopolize queue resources.

## Concurrency Design
The implementation uses the classic Producer-Consumer pattern with additional synchronization constraints:

### Capacity Enforcement
The system maintains a bounded buffer with a maximum capacity of 20 tasks. Producer threads are suspended whenever the queue is full and resume only after consumers remove tasks.

### VIP Quota Enforcement
VIP tasks are subject to an additional constraint: no more than 6 VIP tasks may be present in the queue at any given time. This prevents VIP tasks from monopolizing queue capacity and demonstrates synchronization around multiple shared-state conditions.

### Consumer Coordination
Consumer threads remain blocked while the queue is empty and are awakened when new work becomes available.

### Atomic Statistics Tracking
Runtime metrics are maintained using atomic variables to avoid race conditions while minimizing synchronization overhead.

## Technologies
- C++
- POSIX Threads (pthreads)
- Mutexes
- Condition Variables
- Semaphores
- Atomic Operations
- GNU Make

## Requirements
- Linux environment
- GCC/G++ compiler supporting C++11 or later
- GNU Make

## Building
Clone the repository and run:

    make

This generates the executable:

    concurrent_scheduler

## Usage

### Default Execution
    ./concurrent_scheduler

With no arguments, the program defaults to generating 110 tasks. Additionally, the sleep time for all threads defaults to 0ms, meaning that threads never sleep.

### Custom Configuration
    ./concurrent_scheduler -s 200 -g 25 -v 50 -x 30 -r 30

Request limits and thread sleep times can be specified using the command-line options:
| Option | Description |
|---|---|
| -s N | Total number of tasks to generate |
| -g N | General producer delay (ms) |
| -v N | VIP producer delay (ms) |
| -x N | T-X consumer delay (ms) |
| -r N | Rev-9 consumer delay (ms) |

## Concepts Demonstrated
- Producer-Consumer Pattern
- Thread Synchronization
- Shared Resource Protection
- Inter-Thread Communication
- Queue-Based Task Scheduling
- Atomic Operations
- Deadlock Prevention
- Concurrent System Design

## Author
Justine Aguiluz
