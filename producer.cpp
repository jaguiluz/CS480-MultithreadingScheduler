//
// Created by Justin Aguiluz on 11/12/25.
//

#include <iostream>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <atomic>
#include <cstring>

#include "producer.h"
#include "log.h"

// Atomic counters initialized to 0 (macro INIT_COUNTER)
atomic<int> generalReq(INIT_COUNTER), vipReq(INIT_COUNTER), generalTotal(INIT_COUNTER),
    vipTotal(INIT_COUNTER), requestsMade(INIT_COUNTER);

// Shared FIFO queue barrier between the producer and consumer threads
// Queue can only hold 20 requests and VIP requests are limited to 6 requests
queue<int> tableQueue;

// For logging purposes
int produced[RequestTypeN];
int numInQueue[RequestTypeN];

pthread_mutex_t reqMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex lock for request production
pthread_mutex_t limitMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex lock for queue capacity
pthread_mutex_t vipMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex lock for VIP request limit

pthread_cond_t limitCond = PTHREAD_COND_INITIALIZER; // Conditional variable to wait for open space in the queue
pthread_cond_t vipCond = PTHREAD_COND_INITIALIZER; // Conditional variable for VIP request limits in the queue

/**
 * Creates and adds requests to the buffer (tableQueue) until the total number of requests made reaches the limit.
 * The general request thread adds general requests, while the VIP request thread adds VIP requests.
 * Able to distinguish between the general and VIP request concierges using the ID passed through the pointer argument.
 *
 * The thread sleeps first (if specified by CLI argument) to simulate request production
 * When the queue is at capacity, producer threads must wait until the consumer threads
 * make available spaces in the queue.
 * After producing a request, log information is printed.
 *
 * When the total number of requests made reaches the request limit, the producer thread terminates.
 *
 * @param ptr Void pointer to the thread arguments
 *              A ThreadArgs struct must be passed through. See the above for details.
 *              Essentially, ThreadArgs contains the request limit, thread ID, and sleep time.
 *              The pointer to the main thread's barrier is also included, but is not used here.
 */
void* produce(void* ptr)
{
    // Get the thread arguments from the pointer
    ThreadArgs* args = (ThreadArgs*) ptr;
    int requestLimit = args->requestLimit;
    int threadId = args->id;

    // Get the sleep time from the thread arguments
    // Convert the time from milliseconds to nanoseconds for nanosleep
    timespec sleepTime;
    sleepTime.tv_sec = args->sleepTime / MSPERSEC; // Convert to nanoseconds
    sleepTime.tv_nsec = (args->sleepTime % MSPERSEC) * NSPERMS; // Remaining nanoseconds

    // Infinite loop to avoid prematurely terminating the thread
    while (true)
    {
        // Check if the queue is full
        // Use the limitMutex lock to check for the queue capacity
        pthread_mutex_lock(&limitMutex);
        int numRequests = generalReq.load() + vipReq.load();
        if (numRequests >= QUEUE_CAPACITY)
        {
            pthread_cond_wait(&limitCond, &limitMutex);
        }
        pthread_mutex_unlock(&limitMutex);

        // For the VIP producer thread specifically, check if 6 VIP requests are in the queue already
        if (threadId == VIP_REQ)
        {
            // Use the vipMutex lock to check for the number of VIP requests in the queue
            pthread_mutex_lock(&vipMutex);
            if (vipReq.load() >= VIP_LIMIT)
            {
                pthread_cond_wait(&vipCond, &vipMutex);
            }
            pthread_mutex_unlock(&vipMutex);
        }

        // Have the thread sleep
        nanosleep(&sleepTime, NULL);

        // Before adding a request, check if the number of requests made has reached the limit
        // Terminate the thread if it does
        if (requestsMade.load() >= requestLimit)
        {
            pthread_exit(EXITCODE);
        }

        // Item is an integer which can be:
        // 0: GENERAL_REQ, general request
        // 1: VIP_REQ, VIP room
        int item; // Declare an item
        RequestType requestType; // Store the request type

        // ALL accesses and modifications to tableQueue MUST be protected using the request mutex lock
        pthread_mutex_lock(&reqMutex);
        // Check the current producer's thread ID to determine which request is being made
        if (threadId == GENERAL_REQ)
        {
            item = GENERAL_REQ; // 0

            // Add the item to the table request queue and atomically increment the related counters
            tableQueue.push(item);
            generalReq.fetch_add(1);
            generalTotal.fetch_add(1);
            requestsMade.fetch_add(1);
            requestType = GeneralTable; // Set the request type
        }
        else
        {
            item = VIP_REQ; // 1

            // Add the item to the table request queue and atomically increment the related counters
            tableQueue.push(item);
            vipReq.fetch_add(1);
            vipTotal.fetch_add(1);
            requestsMade.fetch_add(1);
            requestType = VIPRoom; // Set the request type
        }

        // Iterate through the request types and update numInQueue and produced
        // with the new counter values for the respective request types
        for (int i = 0; i < RequestTypeN; i++)
        {
            switch (i)
            {
                case GENERAL_REQ:
                produced[i] = generalTotal.load();
                numInQueue[i] = generalReq.load();
                break;

                case VIP_REQ:
                produced[i] = vipTotal.load();
                numInQueue[i] = vipReq.load();
                break;
            }
        }
        output_request_added(requestType, (unsigned int*) produced, (unsigned int*)numInQueue);

        // Check if the table queue is not empty to be sure before signaling to the consumer threads
        if (!tableQueue.empty())
        {
            pthread_cond_signal(&emptyCond);
        }
        pthread_mutex_unlock(&reqMutex);
    }
}
