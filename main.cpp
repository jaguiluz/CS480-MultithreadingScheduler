// Justin Aguiluz
// Red ID: 828223520

#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <atomic>
#include <cstring>
#include "log.h"
#include "producer.h"
#include "consumer.h"

#define THREAD_COUNT 4
#define PROD_LIMIT 110
#define DEFAULT_SLEEP 0

#define GENERAL_ID 0
#define VIP_ID 1
#define TX_ID 2
#define R9_ID 3

#define NORMAL_EXIT 0
#define BAD_EXIT (-1)

using namespace std;

int main(int argc, char** argv)
{
    int option;   /* command line switch */
    int idx;  // general purpose index variable

    int requestLimit = PROD_LIMIT;
    int txSleep = DEFAULT_SLEEP;
    int r9Sleep = DEFAULT_SLEEP;
    int generalSleep = DEFAULT_SLEEP;
    int vipSleep = DEFAULT_SLEEP;

    /*
     * Optional arguments:
     *      -s N Number of seating requests
     *          Default is 110 if unspecified
     *
     *      The next arguments specify the amount of time the corresponding thread sleeps for
     *      in milliseconds.
     *      If unspecified for any of the options below, the default sleep time
     *      is 0 for the thread that corresponds to the options below.
     *      This means that by default, threads do not sleep.
     *
     *      -x N T-X robot average seating request time
     *
     *      -r N Rev-9 robot average seating request time
     *
     *      -g N General table greeter robot average seating production time
     *
     *      -v N VIP table greater average seating production time
     */
    while ((option = getopt(argc, argv, "s:x:r:g:v:")) != -1)
    {
        switch (option)
        {
        case 's':
            requestLimit = stoi(optarg);
            break;

        case 'x':
            txSleep = stoi(optarg);
            break;

        case 'r':
            r9Sleep = stoi(optarg);
            break;

        case 'g':
            generalSleep = stoi(optarg);
            break;

        case 'v':
            vipSleep = stoi(optarg);
            break;
        }
    }

    // Semaphore barrier to have the main thread wait for the producer and consumer threads to terminate
    sem_t Barrier;
    if (sem_init(&Barrier, 0, 0) == -1)
    {
        cerr << "Unable to initialize semaphore" << endl;
        exit(BAD_EXIT);
    }

    // Initialize a ThreadArgs array struct of size THREAD_COUNT (4) to pass to the threads
    // Each thread uses its own index of the array where:
    //      0: General producer (macro GENERAL_ID)
    //      1: VIP producer (macro VIP_ID)
    //      2: T-X robot consumer (macro TX_ID)
    //      3: Rev9 robot consumer (macro R9_ID)
    ThreadArgs threadArgs[THREAD_COUNT];
    for (idx = 0; idx < THREAD_COUNT; idx++)
    {
        threadArgs[idx].requestLimit = requestLimit;
        threadArgs[idx].barrierPtr = &Barrier;
        // For the producer and consumer types, assign their respective thread IDs and sleep times
        switch (idx)
        {
        case GENERAL_ID:
            threadArgs[idx].id = GENERAL_ID;
            threadArgs[idx].sleepTime = generalSleep;
            break;

        case VIP_ID:
            threadArgs[idx].id = VIP_ID;
            threadArgs[idx].sleepTime = vipSleep;
            break;

        case TX_ID:
            threadArgs[idx].id = TX_ID;
            threadArgs[idx].sleepTime = txSleep;
            break;

        case R9_ID:
            threadArgs[idx].id = R9_ID;
            threadArgs[idx].sleepTime = r9Sleep;
            break;
        }
    }

    // Initialize and start all producer and consumer threads at the same time
    pthread_t generalThread, vipThread, txThread, r9Thread;

    // Create the general producer thread before the VIP producer thread for a more deterministic output
    if (pthread_create(&generalThread, NULL, &produce, &threadArgs[GENERAL_ID]))
    {
        cerr << "Unable to create thread" << endl;
        exit(BAD_EXIT);
    }

    if (pthread_create(&vipThread, NULL, &produce, &threadArgs[VIP_ID]))
    {
        cerr << "Unable to create thread" << endl;
        exit(BAD_EXIT);
    }

    // Create the T-X robot consumer thread before the Rev9 robot consumer thread for a more deterministic output
    if (pthread_create(&txThread, NULL, &consume, &threadArgs[TX_ID]))
    {
        cerr << "Unable to create thread" << endl;
        exit(BAD_EXIT);
    }

    if (pthread_create(&r9Thread, NULL, &consume, &threadArgs[R9_ID]))
    {
        cerr << "Unable to create thread" << endl;
        exit(BAD_EXIT);
    }

    // Use the semaphore barrier to wait until the producer and consumer threads are terminated
    // Barrier is signaled by the consumer thread that processes the last request from the table queue
    // When main thread terminates, all remaining threads are terminated as well
    sem_wait(&Barrier);
    exit(NORMAL_EXIT);
}