/*
CS 415 Project 3: Duck Park - Part 3 (IPC Monitor)
Part 3 Features Implemented:
 1. IPC: Uses pipe() to send state snapshots from Parent (Sim) to Child (Monitor).
 2. Monitor: Child process reads from pipe and displays Queue + Car status.
 3. Visual Queues: Tracks Ticket Queue (visual) and Ride Queue (functional + visual).
 4. Car Tracking: Global arrays track car states for detailed monitoring.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

// CONFIG CONSTANTS
int N = 10; 
int C = 2;  
int P = 2;  
int W = 1;  
int R = 1;  
int T = 30; 
int J = 5;  
int volatile park_open = 1;

// IPC GLOBAL VARIABLES
int pipe_fd[2]; 

// CAR STATUS GLOBALS
int car_state_flags[100]; 
int car_rider_counts[100];
int total_rides_completed = 0;
int total_passengers_served = 0;

// NODE STRUCTURE
typedef struct Node {
    int id;
    struct Node* next;
    pthread_cond_t my_cond; 
    int state; 
} Node;

// QUEUE STRUCTURES
Node *ticket_head = NULL;
Node *ticket_tail = NULL;
pthread_mutex_t m_ticket_list = PTHREAD_MUTEX_INITIALIZER;

Node *ride_head = NULL;
Node *ride_tail = NULL;
int ride_queue_size = 0;
pthread_mutex_t m_ride_queue = PTHREAD_MUTEX_INITIALIZER;

// SYNCHRONIZATION
pthread_cond_t c_ride_queue_not_full = PTHREAD_COND_INITIALIZER;
pthread_cond_t c_passenger_available = PTHREAD_COND_INITIALIZER;

pthread_mutex_t m_load = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m_unload_order = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t c_unload_turn = PTHREAD_COND_INITIALIZER;

int next_unload_ticket = 0;
int ticket_dispenser = 0;

pthread_mutex_t m_print = PTHREAD_MUTEX_INITIALIZER;

// TIME HELPER
long start_time_ms;
long get_time() {
    struct timeval tv; gettimeofday(&tv, NULL);
    return ((tv.tv_sec * 1000 + tv.tv_usec / 1000) - start_time_ms) / 1000;
}

// IPC HELPER: BROADCAST STATE
void broadcast_state() {
    if (!park_open) return;

    char buffer[4096];
    int offset = 0;

    offset += sprintf(buffer + offset, "\n[Monitor] SYSTEM STATE (Time: %ld) =>\n", get_time());

    // 1. snapshot ticket queue
    pthread_mutex_lock(&m_ticket_list);
    offset += sprintf(buffer + offset, "Ticket Queue: [");
    Node* curr = ticket_head;
    while (curr) {
        offset += sprintf(buffer + offset, "P%d ", curr->id);
        curr = curr->next;
    }
    offset += sprintf(buffer + offset, "]\n");
    pthread_mutex_unlock(&m_ticket_list);

    // 2. snapshot ride queue
    pthread_mutex_lock(&m_ride_queue);
    offset += sprintf(buffer + offset, "Ride Queue:   [");
    curr = ride_head;
    while (curr) {
        offset += sprintf(buffer + offset, "P%d ", curr->id);
        curr = curr->next;
    }
    offset += sprintf(buffer + offset, "]\n");
    pthread_mutex_unlock(&m_ride_queue);

    // 3. car status snapshot
    for(int i = 0; i < C; i++) {
        char *status_str = "WAITING";
        if (car_state_flags[i] == 1) status_str = "LOADING";
        if (car_state_flags[i] == 2) status_str = "RUNNING";
        
        offset += sprintf(buffer + offset, "Car status %d %s (%d/%d Passengers)\n", 
                          i, status_str, car_rider_counts[i], P);
    }
    offset += sprintf(buffer + offset, "Passengers served: %d | Rides: %d\n", 
                      total_passengers_served, total_rides_completed);

    write(pipe_fd[1], buffer, strlen(buffer));
}

// HELPER: ENTER TICKET QUEUE
void enter_ticket_queue(int id) {
    pthread_mutex_lock(&m_ticket_list);
    Node* n = malloc(sizeof(Node));
    n->id = id; n->next = NULL;
    if (ticket_tail) ticket_tail->next = n; else ticket_head = n;
    ticket_tail = n;
    pthread_mutex_unlock(&m_ticket_list);
}

// HELPER: LEAVE TICKET QUEUE
void leave_ticket_queue(int id) {
    pthread_mutex_lock(&m_ticket_list);
    Node *curr = ticket_head, *prev = NULL;
    while (curr) {
        if (curr->id == id) {
            if (prev) prev->next = curr->next; else ticket_head = curr->next;
            if (curr == ticket_tail) ticket_tail = prev;
            free(curr);
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    pthread_mutex_unlock(&m_ticket_list);
}

// PASSENGER THREAD
void* passenger(void* arg) {
    int id = *(int*)arg; free(arg);

    pthread_mutex_lock(&m_print);
    printf("[Time: %ld] Passenger %d entered the park\n", get_time(), id);
    fflush(stdout);
    pthread_mutex_unlock(&m_print);

    while (park_open) {
        // 1. explore
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d is exploring the park\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        int sleep_time = (rand() % 10) + 1;
        sleep(sleep_time);
        if (!park_open) break;

        // 2. ticket phase
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d finished exploring, entering the ticket booth\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        enter_ticket_queue(id);

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d entering the ticket queue\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        // 3. enter ride queue
        pthread_mutex_lock(&m_ride_queue);
        while (ride_queue_size >= J && park_open) {
            pthread_cond_wait(&c_ride_queue_not_full, &m_ride_queue);
        }
        if (!park_open) { pthread_mutex_unlock(&m_ride_queue); break; }

        leave_ticket_queue(id);

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d acquired a ticket\n", get_time(), id);
        printf("[Time: %ld] Passenger %d has entered the ride queue\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        Node* node = malloc(sizeof(Node));
        node->id = id; node->next = NULL; node->state = 0;
        pthread_cond_init(&node->my_cond, NULL);

        if (ride_tail) ride_tail->next = node; else ride_head = node;
        ride_tail = node;
        ride_queue_size++;
        
        pthread_cond_signal(&c_passenger_available);

        // 4. wait to board
        while (node->state == 0 && park_open) 
            pthread_cond_wait(&node->my_cond, &m_ride_queue);
        
        if (node->state == 0 && !park_open) {
             pthread_mutex_unlock(&m_ride_queue); free(node); break; 
        }
        pthread_mutex_unlock(&m_ride_queue);

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d is boarding\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        // 5. ride & unboard
        pthread_mutex_lock(&m_ride_queue);
        while (node->state == 1) 
            pthread_cond_wait(&node->my_cond, &m_ride_queue);
        
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Passenger %d unboarded\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        node->state = 3;
        pthread_cond_signal(&node->my_cond);

        while (node->state != 4)
            pthread_cond_wait(&node->my_cond, &m_ride_queue);
        
        pthread_mutex_unlock(&m_ride_queue);

        pthread_cond_destroy(&node->my_cond);
        free(node);
        
        total_passengers_served++;
    }
    return NULL;
}

// CAR THREAD
void* car(void* arg) {
    int id = *(int*)arg; free(arg);
    while (park_open) {
        // 1. load
        pthread_mutex_lock(&m_load);

        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d invoked load()\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        Node* riders[P];
        int count = 0;
        struct timespec ts;
        struct timeval now;

        car_state_flags[id] = 1; 
        car_rider_counts[id] = 0;

        pthread_mutex_lock(&m_ride_queue);
        
        while (count < P && park_open) {
            if (ride_head) {
                riders[count] = ride_head;
                ride_head = ride_head->next;
                if (!ride_head) ride_tail = NULL;
                ride_queue_size--;
                
                riders[count]->state = 1; 
                pthread_cond_signal(&riders[count]->my_cond);
                pthread_cond_signal(&c_ride_queue_not_full);
                count++;
                
                car_rider_counts[id] = count;
                
            } else {
                if (count == 0) {
                    car_state_flags[id] = 0; // WAITING
                    pthread_cond_wait(&c_passenger_available, &m_ride_queue);
                    car_state_flags[id] = 1; // LOADING
                } else {
                    gettimeofday(&now, NULL);
                    ts.tv_sec = now.tv_sec + W; ts.tv_nsec = now.tv_usec * 1000;
                    if (pthread_cond_timedwait(&c_passenger_available, &m_ride_queue, &ts) == ETIMEDOUT) {
                        pthread_mutex_lock(&m_print);
                        printf("[Time: %ld] Car %d waiting period expired\n", get_time(), id);
                        fflush(stdout);
                        pthread_mutex_unlock(&m_print);
                        break;
                    }
                }
            }
        }
        pthread_mutex_unlock(&m_ride_queue);

        // depart
        pthread_mutex_lock(&m_unload_order);
        int my_ticket = ticket_dispenser++;
        pthread_mutex_unlock(&m_unload_order);
        
        pthread_mutex_unlock(&m_load);

        car_state_flags[id] = 2; // RUNNING
        
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d has departed to ride\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        sleep(R);

        // unload
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d has returned from the ride\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        pthread_mutex_lock(&m_unload_order);
        while (my_ticket != next_unload_ticket) 
            pthread_cond_wait(&c_unload_turn, &m_unload_order);
        
        pthread_mutex_lock(&m_print);
        printf("[Time: %ld] Car %d has invoked unload()\n", get_time(), id);
        fflush(stdout);
        pthread_mutex_unlock(&m_print);

        pthread_mutex_lock(&m_ride_queue);
        for(int i=0; i<count; i++) {
            riders[i]->state = 2;
            pthread_cond_signal(&riders[i]->my_cond);
        }
        for(int i=0; i<count; i++) {
            while(riders[i]->state != 3) 
                pthread_cond_wait(&riders[i]->my_cond, &m_ride_queue);
            riders[i]->state = 4;
            pthread_cond_signal(&riders[i]->my_cond);
        }
        pthread_mutex_unlock(&m_ride_queue);

        next_unload_ticket++;
        pthread_cond_broadcast(&c_unload_turn);
        pthread_mutex_unlock(&m_unload_order);
        
        total_rides_completed++;
    }
    return NULL;
}

// MONITOR PROCESS
void run_monitor() {
    close(pipe_fd[1]); 
    char buffer[4096];
    ssize_t bytes;
    
    while ((bytes = read(pipe_fd[0], buffer, sizeof(buffer)-1)) > 0) {
        buffer[bytes] = 0;
        printf("%s", buffer); 
        fflush(stdout); 
    }
    close(pipe_fd[0]);
    exit(0);
}

int main(int argc, char* argv[]) {
    int opt;
    while((opt = getopt(argc, argv, "n:c:p:w:r:t:j:")) != -1) {
        switch(opt) {
            case 'n': N=atoi(optarg); break;
            case 'c': C=atoi(optarg); break;
            case 'p': P=atoi(optarg); break;
            case 'w': W=atoi(optarg); break;
            case 'r': R=atoi(optarg); break;
            case 't': T=atoi(optarg); break;
            case 'j': J=atoi(optarg); break;
        }
    }

    struct timeval tv; gettimeofday(&tv, NULL);
    start_time_ms = tv.tv_sec*1000 + tv.tv_usec/1000;
    
    if (pipe(pipe_fd) == -1) { perror("pipe"); exit(1); }

    pid_t pid = fork();

    if (pid == 0) {
        run_monitor();
    } else {
        close(pipe_fd[0]); 

        pthread_t pt[N], ct[C];
        for(int i=0; i<C; i++) { int* x=malloc(4); *x=i; pthread_create(&ct[i], NULL, car, x); }
        for(int i=0; i<N; i++) { int* x=malloc(4); *x=i; pthread_create(&pt[i], NULL, passenger, x); }

        // samples every 1 second, prints Monitor state every 5th second (4, 9, 14...)
        for (int i = 0; i < T; i++) {
            sleep(1);
            long now = get_time();
            // check if we hit the interval target (approx 4, 9, 14, 19...)
            if (now % 5 == 4) {
                broadcast_state();
            }
        }
        
        park_open = 0;

        // cleanup
        pthread_cond_broadcast(&c_ride_queue_not_full);
        pthread_cond_broadcast(&c_passenger_available);
        for(int i=0; i<N; i++) pthread_join(pt[i], NULL);
        for(int i=0; i<C; i++) { pthread_cancel(ct[i]); pthread_join(ct[i], NULL); }

        close(pipe_fd[1]); 
        wait(NULL); 
        
        printf("=========== PARK CLOSED ==========\n");
        printf("[Monitor] FINAL STATISTICS:\n");
        printf("Total Simulation time: [Time: %d]\n", T);
        printf("Total Passengers Served: %d\n", total_passengers_served);
        printf("Total Rides: %d\n", total_rides_completed);
    }
    return 0;
}