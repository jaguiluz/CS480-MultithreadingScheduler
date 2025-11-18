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

#include "consumer.h"
#include "producer.h"
#include "log.h"

// Atomic counters for the request type consumed by the robot type initialized to 0 (macro INIT_COUNTER)
atomic<int> txGeneral(INIT_COUNTER), txVIP(INIT_COUNTER), r9General(INIT_COUNTER), r9VIP(INIT_COUNTER);

pthread_mutex_t emptyMutex = PTHREAD_MUTEX_INITIALIZER; // Mutex lock for checking queue emptiness
pthread_cond_t emptyCond = PTHREAD_COND_INITIALIZER; // Conditional variable to wait for items in the queue
/**
 * Processes the requests in the buffer (tableQueue).
 * Able to distinguish between the T-X and Rev9 robots using the ID passed through the pointer argument.
 *
 * Consumer threads must wait until at least one request exists in the queue to consume.
 * The thread sleeps first (if specified by CLI argument) to simulate request consumption.
 * After consuming a request, log information is printed.
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
        // Check if the table request queue is empty
        // Use the emptyMutex lock to have the thread wait for an item in the queue
        pthread_mutex_lock(&emptyMutex);
        if (tableQueue.empty())
        {
            pthread_cond_wait(&emptyCond, &emptyMutex);
        }
        pthread_mutex_unlock(&emptyMutex);

        // Have the thread sleep
        nanosleep(&sleepTime, NULL);

        // ALl accesses and modifications to tableQueue MUST be protected using the request mutex lock
        pthread_mutex_lock(&reqMutex);
        int item = tableQueue.front(); // Get the front of the queue

        // Check if the table request queue is empty again in the case that both consumers go after each other
        if (tableQueue.empty())
        {
            // Unlock the request mutex to allow producer threads to add an item to the queue
            pthread_mutex_unlock(&reqMutex);

            // Use the emptyMutex lock and emptyCond to have the thread wait for an item in the queue
            pthread_mutex_lock(&emptyMutex);
            pthread_cond_wait(&emptyCond, &emptyMutex);
            pthread_mutex_unlock(&emptyMutex);

            // Relock the thread
            pthread_mutex_lock(&reqMutex);
            item = tableQueue.front(); // Get the updated front of the queue
        }

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
        tableQueue.pop(); // Remove the request from the queue

        // Iterate through the request types and update numInQueue
        // with the new counter values for the respective request types
        for (int i = 0; i < RequestTypeN; i++)
        {
            switch (i)
            {
                case GENERAL_REQ:
                numInQueue[i] = generalReq.load();
                break;

                case VIP_REQ:
                numInQueue[i] = vipReq.load();
                break;
            }
        }

        // Create a 2D array storing the robots' consumed stats for logging purposes
        // For consumed[ConsumerTypeN]:
        //      0: T-X robot (macro TYPE_TX)
        //      1: Rev9 robot (macro TYPE_R9)
        // For consumed[ConsumerTypeN][RequestTypeN]:
        //      0: General requests
        //      1: VIP requests
        int consumed[ConsumerTypeN][RequestTypeN] = {{txGeneral.load(), txVIP.load()},
                                                    {r9General.load(), r9VIP.load()}};

        // Check the current thread's ID for the correct log output
        if (threadId == TX_THREAD)
        {
            output_request_removed(TX, requestType, (unsigned int*) consumed[TYPE_TX], (unsigned int*) numInQueue);
        }
        else
        {
            output_request_removed(Rev9, requestType, (unsigned int*) consumed[TYPE_R9], (unsigned int*) numInQueue);
        }

        // Check the queue capacity limits and the VIP request limits and signal the producer threads
        int numRequests = generalReq.load() + vipReq.load();
        if (numRequests < requestLimit)
        {
            pthread_cond_signal(&limitCond);
        }
        // For the VIP request producers (macro VIP_REQ is 6)
        if (vipReq.load() < VIP_REQ)
        {
            pthread_cond_signal(&vipCond);
        }

        pthread_mutex_unlock(&reqMutex);

        // Check if the request limit has been reached (producers have terminated) and if the queue is empty
        // Since we are checking tableQueue for emptiness, the reqMutex lock is used
        pthread_mutex_lock(&reqMutex);
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
    }
}
