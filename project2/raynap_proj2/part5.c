/*

Making a SMART schedular
inspecting behavior and adjusting time slices accordingly

requires heuristic: rules
    - is process CPU or IO bound
    - if usertime > systemtime: CPU bound
    - o/w IO bound

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

//global variables
pid_t pids[64];
int total_processes = 0;
int current_process_index = -1; 
int has_started[64] = {0}; 

//NEW: arrays for each process
//store time slice for each process
int time_slices[64];
//store "CPU" or "IO"
char *proc_types[64];

//helper function: read and print from /proc
//satisfies monitoring requirement
void update_and_print_process_info() {
    //clear the screen 
    //select what stats to display
    printf("\033[H\033[J"); 
    printf("MCP Smart Scheduler (Dynamic Time Slices)\n");
    printf("--------------------------------------------------------------------------------------------\n");
    printf("%-10s %-20s %-8s %-10s %-10s %-10s %-8s %-5s\n", 
           "PID", "Name", "State", "UTime(s)", "STime(s)", "Mem(KB)", "Type", "Slice");
    printf("--------------------------------------------------------------------------------------------\n");

    //clock ticks per second
    long clk_tck = sysconf(_SC_CLK_TCK);

    //skip dead processes
    for (int i = 0; i < total_processes; i++) {
        if (pids[i] == 0) continue;
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d/stat", pids[i]);

        FILE *f = fopen(path, "r");
        if (!f) {
            // Process dead before checked
            continue;
        }

        // Variables to hold data
        int pid;
        char comm[256];
        char state;
        int ppid, pgrp, session, tty_nr, tpgid;
        unsigned int flags;
        unsigned long minflt, cminflt, majflt, cmajflt;
        //user time, system time
        unsigned long utime, stime; 
        long cutime, cstime, priority, nice, num_threads, itrealvalue;
        unsigned long long starttime;
        //virtual memory size
        unsigned long vsize; 

        //scan file
        fscanf(f, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu %ld %ld %ld %ld %ld %ld %llu %lu",
               &pid, comm, &state, &ppid, &pgrp, &session, &tty_nr, &tpgid, &flags,
               &minflt, &cminflt, &majflt, &cmajflt, 
               &utime, &stime, 
               &cutime, &cstime, &priority, &nice, &num_threads, &itrealvalue, &starttime, 
               &vsize);

        fclose(f);

        // Parse command name
        //remove parenthesis
        char *name_clean = comm;
        if (comm[0] == '(') {
            name_clean++;
            size_t len = strlen(name_clean);
            if (len > 0 && name_clean[len-1] == ')') {
                name_clean[len-1] = '\0';
            }
        }

        //convert ticks to seconds
        double u_cpu = (double)utime / clk_tck;
        double s_cpu = (double)stime / clk_tck;
        //convert bytes to kb
        unsigned long mem_kb = vsize / 1024;

        //NEW: part 5
        // Heuristic: 
        //If user time > system time --> CPU Bound
        //If system time > user time --> I/O Bound
        if (utime > stime) {
            proc_types[i] = "CPU";
            //more time for CPU bound
            time_slices[i] = 2;
        } else {
            proc_types[i] = "I/O";
            //normal time for I/O bound
            time_slices[i] = 1;
        }

        printf("%-10d %-20s %-8c %-10.2f %-10.2f %-10lu %-8s %ds\n", 
               pids[i], name_clean, state, u_cpu, s_cpu, mem_kb, proc_types[i], time_slices[i]);
    }
    printf("--------------------------------------------------------------------------------------------\n");
}

//schedule handler
void schedule_handler(int signum) {
    int status;
    pid_t finished_pid;

    //reap zombies
    while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < total_processes; i++) {
            if (pids[i] == finished_pid) {
                pids[i] = 0;
            }
        }
    }

    //check termination
    int active_count = 0;
    for (int i = 0; i < total_processes; i++) {
        if (pids[i] != 0) active_count++;
    }

    if (active_count == 0) {
        exit(0);
    }

    //update dashboard
    //time slices
    update_and_print_process_info();

    //stop current process
    if (current_process_index != -1 && pids[current_process_index] != 0) {
        kill(pids[current_process_index], SIGSTOP);
    }

    //find next process
    do {
        current_process_index = (current_process_index + 1) % total_processes;
    } while (pids[current_process_index] == 0);

    //start/continue process
    if (has_started[current_process_index] == 0) {
        kill(pids[current_process_index], SIGUSR1);
        has_started[current_process_index] = 1;
    } else {
        kill(pids[current_process_index], SIGCONT);
    }

    //set alarm based on process type
    int next_slice = time_slices[current_process_index];
    //default for safety
    if (next_slice <= 0) next_slice = 1;

    alarm(next_slice);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Invalid use: incorrect number of parameters\n");
        exit(1);
    }
    
    FILE *file = fopen(argv[2], "r");
    if (!file) { perror("Error opening file"); exit(1); }

    // Initialize arrays
    for(int k=0; k<64; k++) {
        //default slice
        time_slices[k] = 1;
        proc_types[k] = "Init";
    }

    signal(SIGALRM, schedule_handler);

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    while ((nread = getline(&line, &len, file)) != -1) {
        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            line[nread - 1] = '\0'; nread--;
        }
        
        char *args[64];
        int i = 0;
        char *token = strtok(line, " \t\n");
        while (token != NULL && i < 63) {
            args[i++] = token;
            token = strtok(NULL, " \t\n");
        }
        args[i] = NULL;

        if (args[0] == NULL) continue;

        pid_t pid = fork();

        if (pid < 0) { perror("Fork Failed"); exit(1); }
        else if (pid == 0) {
            sigset_t sigset;
            sigemptyset(&sigset);
            sigaddset(&sigset, SIGUSR1);
            sigprocmask(SIG_BLOCK, &sigset, NULL);
            int caught_sig;
            sigwait(&sigset, &caught_sig); 
            
            execvp(args[0], args); 
            perror("Execvp");
            exit(1);
        }
        else {
            pids[total_processes++] = pid;
        }
    }

    free(line);
    fclose(file);

    //start
    schedule_handler(SIGALRM);

    while (1) {
        pause();
    }

    return 0;
}