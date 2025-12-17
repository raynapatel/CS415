/*
CS 415 Project 3: Duck Park - Part 2
Multi-Threaded Solution with Queues, Timeouts, and FIFO Ordering.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>

// CONFIG CONSTANTS
int N = 10; // passengers
int C = 2;  // cars
int P = 2;  // cap per car
int W = 1;  // wait period
int R = 1;  // ride duration
int T = 30; // park duration
int J = 5;  // max ride queue

// sim status flag
int volatile park_open = 1;

// QUEUE DATA STRUCTURE
// linked list node --> specific passenger
typedef struct Node {
    int id;
    struct Node* next;
    // private condition var
    // car can wake specific passenger
    pthread_cond_t my_cond; 
    // state lifecyle:
    // 0: Waiting, 1: Boarded, 2: Allowed to Unboard, 3: Unboarded, 4: Released
    int state; 
} Node;

Node *head = NULL;
Node *tail = NULL;
int queue_size = 0;

// SYNCHRONIZATION

// 1. queue lock
// protect the linked list and queue_size
pthread_mutex_t m_queue = PTHREAD_MUTEX_INITIALIZER;
// wait here if queue >= J
pthread_cond_t c_queue_not_full = PTHREAD_COND_INITIALIZER;
// wait here if queue empty
pthread_cond_t c_passenger_available = PTHREAD_COND_INITIALIZER;

// 2. loading lock
// exclusive Boarding (one car loads at a time)
pthread_mutex_t m_load = PTHREAD_MUTEX_INITIALIZER;

// 3. unload order lock
// ensures FIFO unloading
pthread_mutex_t m_unload_order = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c_unload_turn = PTHREAD_COND_INITIALIZER;
// ticket # currently allowed to unload
int next_unload_ticket = 0;
// ticket # assigned to departing cars
int ticket_dispenser = 0;

// 4. output lock
// prevents gibberish terminal output
pthread_mutex_t m_print = PTHREAD_MUTEX_INITIALIZER;

// TIME HELPER
long start_time_ms;

long get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long current_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return (current_ms - start_time_ms) / 1000;
}

// PASSENGER THREAD
void* passenger(void* arg) {
    int id = *(int*)arg;
    free(arg);

    while (park_open) {
        // 1. explore phase
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d is exploring the park\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        int sleep_time = (rand() % 10) + 1;
        sleep(sleep_time);

        if (!park_open) break;

        // 2. ticket phase
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d entering the ticket booth\n", get_time(), id);
        printf("[Time: %ld] Passenger %d entering the ticket queue\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        // 3. enter ride queue
        pthread_mutex_lock(&m_queue);

        // queue bound enforcement
        while (queue_size >= J && park_open) {
            pthread_cond_wait(&c_queue_not_full, &m_queue);
        }

        if (!park_open) { pthread_mutex_unlock(&m_queue); break; }

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d acquired a ticket\n", get_time(), id);
        printf("[Time: %ld] Passenger %d has entered the ride queue\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        // create node
        Node* node = malloc(sizeof(Node));
        node->id = id;
        node->next = NULL;
        node->state = 0; // waiting
        pthread_cond_init(&node->my_cond, NULL);

        // enqueue
        if (tail) tail->next = node; else head = node;
        tail = node;
        queue_size++;

        // notify cars
        pthread_cond_signal(&c_passenger_available);

        // 4. wait to board
        while (node->state == 0 && park_open) {
            pthread_cond_wait(&node->my_cond, &m_queue);
        }
        
        // release the queue lock (temp) --> print boarding status safely
        pthread_mutex_unlock(&m_queue);

        if (!park_open) break;

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d is boarding\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        // 5. wait to unboard
        pthread_mutex_lock(&m_queue);
        while (node->state == 1) {
            pthread_cond_wait(&node->my_cond, &m_queue);
        }

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d unboarded\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        // complete carload rule
        // signal the car printing finished and leaving
        node->state = 3;
        pthread_cond_signal(&node->my_cond);
        
        // wait for car to release passenger
        while (node->state != 4) {
            pthread_cond_wait(&node->my_cond, &m_queue);
        }

        pthread_mutex_unlock(&m_queue);

        // cleanup node
        pthread_cond_destroy(&node->my_cond);
        free(node);
    }
    return NULL;
}

// CAR THREAD
void* car(void* arg) {
    int id = *(int*)arg;
    free(arg);

    while (park_open) {
        // 1. load phase
        // exclusive boarding
        pthread_mutex_lock(&m_load);

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d invoked load()\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        Node* riders[P];
        int count = 0;
        struct timespec ts;
        struct timeval now;

        pthread_mutex_lock(&m_queue);
        
        // attempt to fill the car
        while (count < P && park_open) {
            if (head) {
                // take passengeer
                riders[count] = head;
                head = head->next;
                if (!head) tail = NULL;
                queue_size--;

                // wake up ticket queue (space available)
                pthread_cond_signal(&c_queue_not_full);

                // signal passenger to board
                riders[count]->state = 1; // boarded
                pthread_cond_signal(&riders[count]->my_cond);
                
                count++;
            } else {
                // queue empty
                // wait or timeout
                if (count == 0) {
                    // empty car
                    // wait indefinitely
                    pthread_cond_wait(&c_passenger_available, &m_queue);
                } else {
                    // timeout mechanism 
                    // wait W seconds
                    gettimeofday(&now, NULL);
                    ts.tv_sec = now.tv_sec + W;
                    ts.tv_nsec = now.tv_usec * 1000;

                    int rc = pthread_cond_timedwait(&c_passenger_available, &m_queue, &ts);
                    
                    if (rc == ETIMEDOUT) {
                        pthread_mutex_lock(&m_print);
                        printf("[Time: %ld] Car %d waiting period expired\n", get_time(), id);
                        pthread_mutex_unlock(&m_print);
                        break; // stop loading
                    }
                }
            }
        }
        pthread_mutex_unlock(&m_queue);

        // get unload ticket (assing order)
        pthread_mutex_lock(&m_unload_order);
        int my_ticket = ticket_dispenser++;
        pthread_mutex_unlock(&m_unload_order);

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d has departed to ride\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        pthread_mutex_unlock(&m_load); // allow next car to load

        // 2. ride phase
        // concurrent running
        sleep(R);

        // 3. unload phase
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d has returned from the ride\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        // FIFO unloading
        pthread_mutex_lock(&m_unload_order);
        while (my_ticket != next_unload_ticket) {
            pthread_cond_wait(&c_unload_turn, &m_unload_order);
        }

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d has invoked unload()\n", get_time(), id);
        pthread_mutex_unlock(&m_print);

        // complete carload rule
        pthread_mutex_lock(&m_queue);
        
        // signal all passengers to unboard
        for (int i = 0; i < count; i++) {
            riders[i]->state = 2;
            pthread_cond_signal(&riders[i]->my_cond);
        }

        // wait for all passengers to unboard
        for (int i = 0; i < count; i++) {
            while (riders[i]->state != 3) {
                pthread_cond_wait(&riders[i]->my_cond, &m_queue);
            }
            // tell passengers they can go
            riders[i]->state = 4; 
            pthread_cond_signal(&riders[i]->my_cond);
        }
        pthread_mutex_unlock(&m_queue);

        // pass turn to next car
        next_unload_ticket++;
        pthread_cond_broadcast(&c_unload_turn);
        pthread_mutex_unlock(&m_unload_order);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    // 1. argument parsing
    int opt;
    while ((opt = getopt(argc, argv, "n:c:p:w:r:t:j:")) != -1) {
        switch (opt) {
            case 'n': N = atoi(optarg); break;
            case 'c': C = atoi(optarg); break;
            case 'p': P = atoi(optarg); break;
            case 'w': W = atoi(optarg); break;
            case 'r': R = atoi(optarg); break;
            case 't': T = atoi(optarg); break;
            case 'j': J = atoi(optarg); break;
        }
    }

    // 2. initialize timer
    struct timeval tv;
    gettimeofday(&tv, NULL);
    start_time_ms = tv.tv_sec * 1000 + tv.tv_usec / 1000;

    // 3. create threads
    pthread_t pt[N], ct[C];

    for (int i = 0; i < C; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        if (pthread_create(&ct[i], NULL, car, id) != 0) {
            perror("Failed to create car thread");
        }
    }

    for (int i = 0; i < N; i++) {
        int* id = malloc(sizeof(int));
        *id = i;
        if (pthread_create(&pt[i], NULL, passenger, id) != 0) {
            perror("Failed to create passenger thread");
        }
    }

    // 4. run simulation
    sleep(T);
    park_open = 0;

    // 5. cleanup
    // wake up stuck threads
    pthread_cond_broadcast(&c_queue_not_full);
    pthread_cond_broadcast(&c_passenger_available);

    for (int i = 0; i < N; i++) {
        pthread_join(pt[i], NULL);
    }
    for (int i = 0; i < C; i++) {
        pthread_cancel(ct[i]);
        pthread_join(ct[i], NULL);
    }

    printf("PARK CLOSED\n");
    return 0;
}