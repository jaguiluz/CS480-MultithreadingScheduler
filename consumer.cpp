//
// Created by Justin Aguiluz on 11/12/25.
// RedID: 828223520
//

#include <iostream>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <atomic>
#include <cstring>

#include "consumer.h"
#include "producer.h"
#include "log.h"

// Atomic counters for the request type consumed by the robot type initialized to 0 (macro INIT_COUNTER)
atomic<int> txGeneral(INIT_COUNTER), txVIP(INIT_COUNTER), r9General(INIT_COUNTER), r9VIP(INIT_COUNTER);

pthread_cond_t emptyCond = PTHREAD_COND_INITIALIZER; // Conditional variable to wait for items in the queue
/**
 * Processes the requests in the buffer (tableQueue).
 * Able to distinguish between the T-X and Rev9 robots using the ID passed through the pointer argument.
 *
 * Consumer threads must wait until at least one request exists in the queue to consume.
 * After consuming a request, log information is printed and the thread sleeps (if specified by CLI argument).
 *
 * When the producer threads are terminated (production limit reached) and the queue is empty,
 * a summary is printed and the thread signals the main thread's semaphore barrier to exit.
 * Termination of the main thread terminates all active threads.
 *
 * @param ptr Void pointer to the thread arguments
 *              A ThreadArgs struct must be passed through. See producer.h for details.
 *              Essentially, ThreadArgs contains the request limit, thread ID, sleep amount,
 *              and the pointer to the main thread's barrier.
 */
void* consume(void* ptr)
{
    // Get the thread arguments from the pointer
    ThreadArgs* args = (ThreadArgs*) ptr;
    int requestLimit = args->requestLimit;
    int threadId = args->id;
    sem_t* semPtr = args->barrierPtr;

    // Get the sleep time from the thread arguments
    // Convert the time from milliseconds to nanoseconds for nanosleep
    timespec sleepTime;
    sleepTime.tv_sec = args->sleepTime / MSPERSEC; // Convert to nanoseconds
    sleepTime.tv_nsec = (args->sleepTime % MSPERSEC) * NSPERMS; // Remaining nanoseconds

    // Infinite loop to avoid prematurely terminating the thread
    while (true)
    {
        // Acquire mutex lock for all accesses to the tableQueue
        // Check if the table request queue is empty
        pthread_mutex_lock(&reqMutex);
        while (tableQueue.empty())
        {
            pthread_cond_wait(&emptyCond, &reqMutex);
        }

        // Get the request at the front of the queue and remove it
        int item = tableQueue.front();
        tableQueue.pop();

        RequestType requestType; // Store the request type
        // Item is an integer which can be:
        // 0: GENERAL_REQ, general request
        // 1: VIP_REQ, VIP room
        switch (item)
        {
            case GENERAL_REQ: // 0
            // Atomically decrement the counter
            generalReq.fetch_sub(1);
            // Check which consumer is processing the request and increment that consumer's request type counter
            // 0: TX_THREAD
            // 1: R9_THREAD
            if (threadId == TX_THREAD)
            {
                txGeneral.fetch_add(1);
            }
            else
            {
                r9General.fetch_add(1);
            }
            requestType = GeneralTable; // Set the request type
            break;

            case VIP_REQ: // 1
            // Atomically decrement the counter
            vipReq.fetch_sub(1);
            // Check which consumer is processing the request and increment that consumer's request type counter
            // 0: TX_THREAD
            // 1: R9_THREAD
            if (threadId == TX_THREAD)
            {
                txVIP.fetch_add(1);
            }
            else
            {
                r9VIP.fetch_add(1);
            }
            requestType = VIPRoom; // Set the request type
            break;
        }

        // Update numInQueue with the new counter values for the respective request types
        numInQueue[GENERAL_REQ] = generalReq.load();
        numInQueue[VIP_REQ] = vipReq.load();

        // Create a 2D array storing the robots' consumed stats for logging purposes
        // For consumed[ConsumerTypeN]:
        //      0: T-X robot (macro TYPE_TX)
        //      1: Rev9 robot (macro TYPE_R9)
        // For consumed[ConsumerTypeN][RequestTypeN]:
        //      0: General requests
        //      1: VIP requests
        int consumed[ConsumerTypeN][RequestTypeN] = {{txGeneral.load(), txVIP.load()},
                                                    {r9General.load(), r9VIP.load()}};
        ConsumerType consumerType;

        // Check the current thread's ID for the correct log output by storing the consumer's type
        if (threadId == TX_THREAD) consumerType = TX;
        else consumerType = Rev9;

        output_request_removed(consumerType, requestType,
            (unsigned int*) consumed[consumerType], (unsigned int*) numInQueue);

        // Check the queue capacity limits and the VIP request limits and signal the producer threads
        int numRequests = generalReq.load() + vipReq.load();
        if (numRequests < QUEUE_CAPACITY)
        {
            pthread_cond_signal(&limitCond);
        }
        // For the VIP request producers (macro VIP_LIMIT is 6)
        if (vipReq.load() < VIP_LIMIT)
        {
            pthread_cond_signal(&vipCond);
        }

        // Check if the request limit has been reached (producers have terminated) and if the queue is empty
        if (requestsMade.load() >= requestLimit && tableQueue.empty())
        {
            // Initialize a double pointer 2D array to store which consumer processed which request
            // Log method uses an array of pointers
            int** consumedFinal = new int*[ConsumerTypeN];
            consumedFinal[TYPE_TX] = new int[RequestTypeN];
            consumedFinal[TYPE_R9] = new int[RequestTypeN];
            for (int i = 0; i < ConsumerTypeN; i++)
            {
                for (int j = 0; j < RequestTypeN; j++)
                {
                    // Copy the contents of the previous consumed 2D array
                    consumedFinal[i][j] = consumed[i][j];
                }
            }
            output_production_history((unsigned int*) produced, (unsigned int**) consumedFinal);
            //Deallocate the double pointer 2D array
            for (int i = 0; i < ConsumerTypeN; i++)
            {
                delete[] consumedFinal[i];
            }
            delete[] consumedFinal;

            // Signal the barrier in the main thread to let it terminate
            // When main thread terminates, all threads are terminated
            sem_post(semPtr);
            pthread_exit(EXITCODE);
        }
        pthread_mutex_unlock(&reqMutex);
        // Have the thread sleep
        nanosleep(&sleepTime, NULL);
    }
}
