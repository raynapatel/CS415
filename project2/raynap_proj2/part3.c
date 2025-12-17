/*

MCP Schedules Processes
Round Robin scheduling using SIGALRM
    - processes taking turns (unlike part 2)

*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

//initialize global variables
//signal handler cannot access main's local variable
pid_t pids[64];
int total_processes = 0;
int current_process_index = -1;
int has_started[64] = {0};

//signal handler: scheduler
//runs every time the alarm goes off
void schedule_handler(int signum) {
    (void)signum;
    int status;
    pid_t finished_pid;

    //check for terminated processess
    //dont wait, just check
    while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < total_processes; i++) {
            if (pids[i] == finished_pid) {
                //mark as terminated
                pids[i] = 0;
            }
        }
    }

    //check if all processes are done
    int active_count = 0;
    for (int i = 0; i < total_processes; i++) {
        if (pids[i] != 0) active_count++;
    }
    //done and MCP exits
    if (active_count == 0) {
        exit(0);
    }

    //stop current running process (if exists and not terminated)
    if (current_process_index != -1 && pids[current_process_index] != 0) {
        kill(pids[current_process_index], SIGSTOP);
    }

    //find next runnable process
    //loop until finding PID that isn't 0
    do {
        current_process_index = (current_process_index + 1) % total_processes;
    } while (pids[current_process_index] == 0);

    //start/resume next process
    if (has_started[current_process_index] == 0) {
        //send SIGUSR1 if first time running
        kill(pids[current_process_index], SIGUSR1);
        has_started[current_process_index] = 1;
    } else {
        //send SIGCONT if run before
        kill(pids[current_process_index], SIGCONT);
    }

    //resent timer for next time slice
    alarm(1);
}

int main(int argc, char *argv[]) {
    if (argc!= 3) {
        fprintf(stderr, "Incalid use: incorrect number of parameters\n");
        exit(1);
    }

    FILE *file = fopen(argv[2], "r");
    if (!file) {
        perror("Error opening file");
        exit(1);
    }

    //setup signal handler
    //link SIGALRM to function 'schedule_handler'
    signal(SIGALRM, schedule_handler);

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    //launch loop (as in part 2)
    while ((nread = getline(&line, &len, file)) != -1) {
        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            line[nread - 1] = '\0';
            nread--;
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

        if (pid < 0) {
            perror("Fork Failed");
            exit(1);
        }

        else if (pid == 0) {
            //child: wait for SIGUSR1
            sigset_t sigset;
            sigemptyset(&sigset);
            sigaddset(&sigset, SIGUSR1);
            sigprocmask(SIG_BLOCK, &sigset, NULL);
            int caught_sig;
            sigwait(&sigset, &caught_sig);
            execvp(args[0], args);
            perror("Execvp");
            exit(1);
        } else {
            //parent: save pid to global array
            pids[total_processes++] = pid;
        }
    }

    free(line);
    fclose(file);

    //start scheduling

    //manually trigger scheduler once to move first process
    schedule_handler(SIGALRM);

    //main loop waits, work happens in signal handler
    while (1) {
        //wait for signal
        pause();
    }

    return 0;
}