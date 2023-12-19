// Ahmed Ahmed 
// CS4760
// Project 3(REDO)
// 12/18/23

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <time.h>
#include <stdarg.h>

#define MAX_CHILDREN 20
#define SHM_KEY 12345
#define MSG_QUEUE_KEY 1234
#define TIMEOUT_SECONDS 60

// Define the structure for messages in the message queue
typedef struct {
    long mtype;
    int data;
} Message;

// Define the structure for Process Control Blocks (PCBs)
typedef struct {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
} PCB;

// Define the structure for the shared clock
typedef struct {
    int seconds;
    int nanoseconds;
} Clock;

// Global variables for shared clock, process table, and message queue
Clock *sharedClock;
PCB processTable[MAX_CHILDREN];
int msgQueueId;

// Function to increment the shared clock
void incrementClock(Clock *clock) {
    clock->nanoseconds += 100000000;
    if (clock->nanoseconds >= 1000000000) {
        clock->seconds++;
        clock->nanoseconds -= 1000000000;
    }
}

// Signal handler for SIGINT (CTRL-C)
void handleSigint(int signum) {
    printf("\nReceived SIGINT (CTRL-C). Terminating gracefully...\n");
    exit(0);
}

// Signal handler for real-life timeout
void handleTimeout(int signum) {
    printf("Program timed out. Cleaning up...\n");
    exit(1);
}

// Function to log output both to the console and a log file
void logOutput(FILE *logFile, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args); // Print to console
    va_end(args);
    va_start(args, format);
    vfprintf(logFile, format, args); // Print to log file
    va_end(args);
    fflush(logFile); // Flush the log file to ensure all data is written
}

// Function to launch child processes (workers)
void launchChildProcesses(int maxChildren, FILE *logFile) {
    static int childrenLaunched = 0;
    if (childrenLaunched < maxChildren) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // Child process
            char *args[] = {"./worker", NULL}; // Replace with actual arguments
            execvp(args[0], args); // Execute the worker program
            perror("Exec failed");
            exit(1);
        } else {
            // Parent process
            processTable[childrenLaunched].occupied = 1;
            processTable[childrenLaunched].pid = pid;
            processTable[childrenLaunched].startSeconds = sharedClock->seconds;
            processTable[childrenLaunched].startNano = sharedClock->nanoseconds;
            logOutput(logFile, "Launched child process with PID: %d\n", pid);
            childrenLaunched++;
        }
    }
}

// Function to check and handle terminated child processes
void checkAndHandleTerminatedChildren(int *totalChildrenTerminated, FILE *logFile) {
    pid_t terminatedPid;
    int status;
    for (int i = 0; i < MAX_CHILDREN; i++) {
        if (processTable[i].occupied) {
            terminatedPid = waitpid(processTable[i].pid, &status, WNOHANG);
            if (terminatedPid > 0) {
                processTable[i].occupied = 0;
                (*totalChildrenTerminated)++;
                logOutput(logFile, "Child process with PID %d terminated\n", terminatedPid);

                // Send a message using msgsnd (for example)
                Message msg;
                msg.mtype = terminatedPid; // Using PID as message type
                msg.data = 0; // Data to be sent
                msgsnd(msgQueueId, &msg, sizeof(msg.data), 0);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Set up signal handlers
    signal(SIGINT, handleSigint);
    signal(SIGALRM, handleTimeout);
    alarm(TIMEOUT_SECONDS);

    // Set up shared memory for the clock
    int clockShmId = shmget(SHM_KEY, sizeof(Clock), IPC_CREAT | 0666);
    sharedClock = (Clock *)shmat(clockShmId, NULL, 0);

    // Set up the message queue
    msgQueueId = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0666);

    // Open a log file for output
    FILE *logFile = fopen("oss_log.txt", "w");
    if (logFile == NULL) {
        perror("Error opening log file");
        exit(1);
    }

    // Placeholder for handling command-line arguments
    int maxChildren = MAX_CHILDREN; // Default value for max children

    int totalChildrenTerminated = 0;

    // Main loop of the program
    while (totalChildrenTerminated < maxChildren) {
        incrementClock(sharedClock);
        launchChildProcesses(maxChildren, logFile);
        checkAndHandleTerminatedChildren(&totalChildrenTerminated, logFile);

        // Log the current state of the system
        logOutput(logFile, "Current system time: %d seconds, %d nanoseconds\n", 
                  sharedClock->seconds, sharedClock->nanoseconds);

        for (int i = 0; i < MAX_CHILDREN; i++) {
            if (processTable[i].occupied) {
              logOutput(logFile, "Entry %d: PID %d, Start Time Seconds: %d, Start Time Nanoseconds: %09d\n",   
                          i, processTable[i].pid, 
                          processTable[i].startSeconds, processTable[i].startNano);
            } else {
                logOutput(logFile, "Entry %d: Empty\n", i);
            }
        }
    }

    // Cleanup resources before terminating
    fclose(logFile);
    shmdt(sharedClock);
    msgctl(msgQueueId, IPC_RMID, NULL); // Remove the message queue

    return 0;
}
