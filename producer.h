//
// Created by Justin Aguiluz on 11/12/25.
// RedID: 828223520
//

#ifndef CS480_ASSIGNMENT4_PRODUCER_H
#define CS480_ASSIGNMENT4_PRODUCER_H

#define QUEUE_CAPACITY 20
#define VIP_LIMIT 6
#define INIT_COUNTER 0

#define GENERAL_REQ 0
#define VIP_REQ 1
#define EMPTY_QUEUE 0

/* One million nanoseconds per millisecond */
#define	NSPERMS		1000000
/* One thousand milliseconds per second */
#define MSPERSEC	1000

#define EXITCODE 0

using namespace std;

/**
 * Struct to pass the necessary arguments to the thread.
 * Threads will need the limit to the number of total requests made, its thread ID, and its sleep time.
 * Additionally, the consumer threads will need a pointer to the main thread's semaphore barrier
 **/
typedef struct Args
{
    int requestLimit; // Limit to the total number of requests that can be produced
    int id; // Thread ID used to differentiate between the general and VIP producers and the TX and Rev9 consumers
    int sleepTime; // The amount a thread sleeps for (in milliseconds)
    sem_t* barrierPtr; // Used by a consumer thread to signal the main thread to terminate
} ThreadArgs;

// Atomic integer counters
// generalReq and vipReq are the numbers of the respective request types currently in tableQueue
// generalTotal and vipTotal are the total number of general and VIP requests respectively
// requestsMade is the total amount of requests made
extern atomic<int> generalReq, vipReq, generalTotal, vipTotal, requestsMade;
extern queue<int> tableQueue; // Barrier to store the requests in a FIFO order

// These arrays are for logging purposes
extern int produced[]; // Stores the total number of requests produced for each type
extern int numInQueue[]; // Stores the number of requests in the queue for each type

// Mutex lock for the tableQueue
extern pthread_mutex_t reqMutex;

extern pthread_cond_t limitCond; // Used when there is no available space in tableQueue
extern pthread_cond_t vipCond; // Condition exclusively for the VIP producer when the VIP request limit is reached
extern pthread_cond_t emptyCond; // Used when tableQueue is empty

/**
 * Creates and adds requests to the buffer (tableQueue) until the total number of requests made reaches the limit.
 * The general request thread adds general requests, while the VIP request thread adds VIP requests.
 * Able to distinguish between the general and VIP request concierges using the ID passed through the pointer argument.
 *
 * When the queue is at capacity, producer threads must wait until the consumer threads
 * free up space in the queue.
 * After producing a request, log information is printed and the thread is put to sleep (if specified by CLI argument).
 *
 * When the total number of requests made reaches the request limit, the producer thread terminates.
 *
 * @param ptr Void pointer to the thread arguments
 *              A ThreadArgs struct must be passed through. See the above for details.
 *              Essentially, ThreadArgs contains the request limit, thread ID, and sleep time.
 *              The pointer to the main thread's barrier is also included, but is not used here.
 */
void* produce(void* ptr);

#endif //CS480_ASSIGNMENT4_PRODUCER_H
