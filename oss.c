// oss.c
// Ahmed Ahmed CS4760 10/12/23

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
#include <string.h>

#define MAX_CHILDREN 20
#define SHM_KEY 12345
#define MSG_QUEUE_KEY 1234
#define TIMEOUT_SECONDS 60

// Message structure
typedef struct {
    long mtype;
    int data;
} Message;

// Process Control Block (PCB) structure
typedef struct {
    int occupied;
    pid_t pid;
    int startSeconds;
    int startNano;
} PCB;

// Clock structure
typedef struct {
    int seconds;
    int nanoseconds;
} Clock;

// Global variables
Clock *sharedClock;
PCB processTable[MAX_CHILDREN];
int msgQueueId;

void incrementClock(Clock *clock) {
    clock->nanoseconds += 100000000;
    if (clock->nanoseconds >= 1000000000) {
        clock->seconds++;
        clock->nanoseconds -= 1000000000;
    }
}

void handleSigint(int signum) {
    printf("\nReceived SIGINT (CTRL-C). Terminating gracefully...\n");
    exit(0);
}

void handleTimeout(int signum) {
    printf("Program timed out. Cleaning up...\n");
    exit(1);
}

void logOutput(FILE *logFile, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    va_start(args, format);
    vfprintf(logFile, format, args);
    va_end(args);
    fflush(logFile);
}

void launchChildProcesses(int maxChildren, FILE *logFile) {
    static int childrenLaunched = 0;
    if (childrenLaunched < maxChildren) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        } else if (pid == 0) {
            // Child process
            char *args[] = {"./worker", NULL}; // Add more arguments as needed
            execvp(args[0], args);
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

                // Example of sending a message using msgsnd
                Message msg;
                msg.mtype = terminatedPid; // Using PID as message type
                msg.data = 0; // Data to be sent
                msgsnd(msgQueueId, &msg, sizeof(msg.data), 0);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    // Signal handling setup
    signal(SIGINT, handleSigint);
    signal(SIGALRM, handleTimeout);
    alarm(TIMEOUT_SECONDS);

    // Shared memory setup
    int clockShmId = shmget(SHM_KEY, sizeof(Clock), IPC_CREAT | 0666);
    sharedClock = (Clock *)shmat(clockShmId, NULL, 0);

    // Message queue setup
    msgQueueId = msgget(MSG_QUEUE_KEY, IPC_CREAT | 0666);

    // Open log file
    FILE *logFile = fopen("oss_log.txt", "w");
    if (logFile == NULL) {
        perror("Error opening log file");
        exit(1);
    }

    // Command-line arguments handling
    int maxChildren = MAX_CHILDREN; // Default value
    // Add logic here to parse command-line arguments if needed

    int totalChildrenTerminated = 0;

    // Main loop
    while (totalChildrenTerminated < maxChildren) {
        incrementClock(sharedClock);
        launchChildProcesses(maxChildren, logFile);
        checkAndHandleTerminatedChildren(&totalChildrenTerminated, logFile);

        // Log current state
        logOutput(logFile, "Current system time: %d seconds, %d nanoseconds\n", 
                  sharedClock->seconds, sharedClock->nanoseconds);

        for (int i = 0; i < MAX_CHILDREN; i++) {
            if (processTable[i].occupied) {
                logOutput(logFile, "Entry %d: PID %d, Start Time %d.%d\n", 
                          i, processTable[i].pid, 
                          processTable[i].startSeconds, processTable[i].startNano);
            } else {
                logOutput(logFile, "Entry %d: Empty\n", i);
            }
        }
    }

    // Cleanup
    fclose(logFile);
    shmdt(sharedClock);
    msgctl(msgQueueId, IPC_RMID, NULL); // Remove message queue

    return 0;
}