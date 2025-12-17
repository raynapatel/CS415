/*

 * Part 2: MCP Controls the Workload
 * - Children wait for SIGUSR1 before starting.
 * - Parent sends SIGUSR1 (Start), SIGSTOP (Pause), SIGCONT (Resume).
 
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
//NEW
#include <signal.h>

int main(int argc, char *argv[]) {
    //check for correct command line arguments
    //needs to have exactly 3 arguments
    if (argc != 3) {
        fprintf(stderr, "Invalid flag: incorrect number of parameters");
        exit(1);
    }

    //check if second argument is -f
    if (strcmp(argv[1], "-f") != 0) {
        fprintf(stderr, "Invalid flag: '%s", argv[1]);
        exit(1);
    }

    //open input file
    //argv[2] holds filename
    FILE *file = fopen(argv[2], "r");
    if (file == NULL) {
        //if fopen returns NULL, file could not open
        perror("Error opening file");
        exit(1);
    }

    //NEW: need to remember children
    pid_t pids[64];
    int pid_count = 0;

    //setup variables for reading file line by line
    //line is pointer to buffer containing text line
    char *line = NULL;
    //len is size of buffer
    size_t len = 0;
    //nread is number of chars read
    ssize_t nread;

    //loop through file one line at a time
    //getline auto allocates memory for line if NULL
    while ((nread = getline(&line, &len, file)) != -1) {
        //remove newline
        while (nread > 0 && (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
            line[nread - 1] = '\0';
            nread--;
        }

        //tokenize line: seperate command and args
        //need array of strings (char pointers)
        char * args[64];
        int i = 0;

        //get first command
        char *token = strtok(line, " \t\n");

        //loop for remaning tokens
        while (token != NULL && i < 63) {
            //store token in array
            args[i++] = token;
            //get next token
            token = strtok(NULL, " \t\n");
        }
        //arg list has to be terminated by NULL pointer
        args[i] = NULL;

        //if line empty or whitespace, skip
        if (args[0] == NULL) {
            continue;
        }

        //fork new process to run command
        //fork() returns 0 to child, returns childs pid to parent
        pid_t pid = fork();

        //fork fails
        if (pid < 0) {
            perror("Fork Failed");
            exit(1);
        }
        else if (pid == 0) {
            //NEW: set op signal mask
            //must BLOCK SIGUSR1 so OS queues instead of ignoring
            sigset_t sigset;
            sigemptyset(&sigset);
            sigaddset(&sigset, SIGUSR1);
            sigprocmask(SIG_BLOCK, &sigset, NULL);

            //DEBUG
            //fprintf(stderr, 'Child %d waiting \n', getpid());

            //wait for signal
            //pauses execution until SIGUSR1 recieved
            int caught_sig;
            sigwait(&sigset, &caught_sig);

            //parent says go to get to here
            execvp(args[0], args);
            //if execvp returns, error occured
            perror("Execvp");
            exit(1);
        }
        else {
            //parent process code
            //NEW: store PID to signal child later
            pids[pid_count++] = pid;
        }
    }

    //cleanup: close file and free memory
    free(line);
    fclose(file);

    //NEW: manage processes
    //now all children are created and paused
    //start --> stop --> continue sequence
    printf("Starting processes...\n");
    //give process moment to get sigwait state
    sleep(1);

    //send SIGUSR1 to all children to wake them
    for (int i = 0; i < pid_count; i++) {
        kill(pids[i], SIGUSR1);
    }
    printf("Processes running: Sleeping for 2 seconds...\n");
    sleep(2);

    //send SIGSTOP to all children to suspend them
    printf("Stopping processes...\n");
    for (int i = 0; i < pid_count; i++) {
        kill(pids[1], SIGSTOP);
    }
    printf("Processes stopped. Sleeping for 2 seconds...\n");
    sleep(2);

    //send SIGCONT to all children to resume them
    printf("Continuing processes...\n");
    for (int i = 0; i < pid_count; i++) {
        kill(pids[i], SIGCONT);
    }

    //wait for child processes to finish
    //calling wait(NULL) blocks until any child finishes
    //loop until wait returns -1 (no children are left)
    while (wait(NULL) > 0);

    //exit MCP
    return 0;
}