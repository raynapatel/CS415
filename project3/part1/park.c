/*
CS 415 Project 3: Duck Park - Part 1
Single Passenger & Single Car Verification
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>

// GLOBAL VARIABLES
// constant with 1 passenger and 1 car
int num_passengers = 1;
int num_cars = 1;
int car_capacity = 1;
int park_duration = 25; //how long park is open

// volatile flag --> compiler won't cache this value
// all threads see change immediately when park closes
int volatile park_open = 1;

// SYNCHRONIZATION
// mutex to protect shared state vars
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// condition vars
// allow threads to sleep until specific event happens
pthread_cond_t cond_passenger_arrived = PTHREAD_COND_INITIALIZER; // passenger signals in queue
pthread_cond_t cond_board = PTHREAD_COND_INITIALIZER; // car signals to get in
pthread_cond_t cond_passenger_boarded = PTHREAD_COND_INITIALIZER; // passenger signals seated
pthread_cond_t cond_unboard = PTHREAD_COND_INITIALIZER; // car signals to get out
pthread_cond_t cond_passenger_left = PTHREAD_COND_INITIALIZER; // passenger signals out of car

// state flags
// queue and capacity buffers
int passenger_waiting = 0; // 1 if passenger in queue
int passenger_on_board = 0; // 1 if passenger in car

// TIME HELPER
long start_time_ms; //store timestamp when sim began

// return # of sec passed since sim started
long get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL); // current clock time
    return ((tv.tv_sec * 1000 + tv.tv_usec / 1000) - start_time_ms) / 1000;
}

// PASSENGER THREAD
void* passenger_routine(void* arg) {
    int id = *(int*)arg; // dereference argument to find ID
    free(arg); // prevent memory leak

    // run as long as park is open
    while (park_open) {
        // 1. explore phase 
        printf("[Time: %ld] Passenger %d is exploring the park\n", get_time(), id);
        sleep(1); // Simulate exploration

        // check if park closed while sleeping
        if (!park_open) break;

        // 2. ticket and queue phase 
        pthread_mutex_lock(&lock); // enter critical section

        // ticket booth
        printf("[Time: %ld] Passenger %d entering the ticket queue\n", get_time(), id);
        printf("[Time: %ld] Passenger %d acquired a ticket\n", get_time(), id);
        
        // queue
        printf("[Time: %ld] Passenger %d has entered the ride queue\n", get_time(), id);
        // update state --> let car know passenger is ready
        passenger_waiting = 1; // atomic state change inside lock
        
        // wake up car if sleeping
        pthread_cond_signal(&cond_passenger_arrived);

        // 3. wait to board
        // busy-waiting while not on board
        while (passenger_on_board == 0 && park_open) {
            //sleep until car signals 'cond_board'
            pthread_cond_wait(&cond_board, &lock);
        }
        
        // check exit
        if (!park_open) { pthread_mutex_unlock(&lock); break; }

        printf("[Time: %ld] Passenger %d is boarding\n", get_time(), id);
        
        // signal the car that passenger seated
        pthread_cond_signal(&cond_passenger_boarded);

        // exit critical section
        pthread_mutex_unlock(&lock); 

        // 4. ride phase (wait to unload)
        // wait to unboard
        pthread_mutex_lock(&lock);
        //busy-waiting until car signals ride over
        while (passenger_on_board == 1 && park_open) {
            pthread_cond_wait(&cond_unboard, &lock);
        }

        printf("[Time: %ld] Passenger %d unboarded\n", get_time(), id);
        
        // signal car passenger has left
        pthread_cond_signal(&cond_passenger_left);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// CAR THREAD
void* car_routine(void* arg) {
    // dereference ID
    int id = *(int*)arg;
    // avoid memory leak
    free(arg);

    while (park_open) {
        // 1. load phase
        printf("[Time: %ld] Car %d invoked load()\n", get_time(), id);
        pthread_mutex_lock(&lock); // enter critical section
        
        // busy-waiting for passenger
        while (passenger_waiting == 0 && park_open) {
            pthread_cond_wait(&cond_passenger_arrived, &lock);
        }
        
        if (!park_open) { pthread_mutex_unlock(&lock); break; }

        // update state
        passenger_waiting = 0; //passenger leaves queue
        passenger_on_board = 1; // passenger enters car
        
        // tell passenger to board
        pthread_cond_signal(&cond_board);

        // wait for passenger to board
        pthread_cond_wait(&cond_passenger_boarded, &lock);

        //exit critical section (temporarily)
        pthread_mutex_unlock(&lock);

        // 2. run phase
        printf("[Time: %ld] Car %d is full with 1 passengers\n", get_time(), id);
        printf("[Time: %ld] Car %d has departed to ride\n", get_time(), id);
        
        sleep(1); // simulate ride

        // 3. unload phase 
        printf("[Time: %ld] Car %d has returned from the ride\n", get_time(), id);
        printf("[Time: %ld] Car %d has invoked unload()\n", get_time(), id);

        pthread_mutex_lock(&lock); // enter critical section
        passenger_on_board = 0; // car now empty
        
        // signal passenger to leave
        pthread_cond_signal(&cond_unboard);
        
        // wait for passenger to leave (atomic safety)
        pthread_cond_wait(&cond_passenger_left, &lock);
        // exit critical section
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    // initialize start time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    start_time_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // create ids
    pthread_t p_thread, c_thread;
    
    // allocate heap memory for IDs
    // ensure memory safety
    int *pid = malloc(sizeof(int)); *pid = 0;
    int *cid = malloc(sizeof(int)); *cid = 0;

    // create threads (1 car, 1 passenger)
    pthread_create(&c_thread, NULL, car_routine, cid);
    pthread_create(&p_thread, NULL, passenger_routine, pid);

    // run simulation
    sleep(park_duration);
    park_open = 0;
    
    // cleanup: wake up stuck threads
    // they can check 'park_open' and exit
    pthread_mutex_lock(&lock);
    pthread_cond_broadcast(&cond_passenger_arrived);
    pthread_cond_broadcast(&cond_board);
    pthread_cond_broadcast(&cond_passenger_boarded); // Wake if stuck in handshake
    pthread_cond_broadcast(&cond_unboard);
    pthread_cond_broadcast(&cond_passenger_left);
    pthread_mutex_unlock(&lock);

    // wait for threads to finish
    pthread_join(p_thread, NULL);
    // cancel car thread if stuck in loop
    pthread_cancel(c_thread); 
    pthread_join(c_thread, NULL);

    printf("PARK CLOSED\n");
    return 0;
}