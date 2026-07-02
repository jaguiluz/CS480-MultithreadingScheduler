//
// Created by Justin Aguiluz on 11/12/25.
// RedID: 828223520
//

#include "producer.h"

#ifndef CS480_ASSIGNMENT4_CONSUMER_H
#define CS480_ASSIGNMENT4_CONSUMER_H

#define TX_THREAD 2
#define TYPE_TX 0
#define R9_THREAD 3
#define TYPE_R9 1

// Atomic integer counters for the respective robot types and their consumed request types
extern atomic<int> txGeneral, txVIP, r9General, r9VIP;

/**
 * Processes the items in the buffer (tableQueue).
 * Able to distinguish between the T-X and Rev9 robots using the ID passed through the pointer argument.
 *
 * Consumer threads must wait until at least one item exists in the queue to consume.
 * After consuming an item, log information is printed and the thread sleeps (if specified by CLI argument).
 *
 * When the producer threads are terminated (production limit reached) and the queue is empty,
 * a summary is printed and the thread signals the main thread's semaphore barrier to exit.
 * Termination of the main thread terminates all active threads.
 *
 * @param ptr Void pointer to the thread arguments
 *              A ThreadArgs struct must be passed through. See producer.h for details.
 *              Essentially, ThreadArgs contains the request limit, thread ID, sleep time,
 *              and the pointer to the main thread's barrier.
 */
void* consume(void* ptr);

#endif //CS480_ASSIGNMENT4_CONSUMER_H