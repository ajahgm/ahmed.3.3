// Ahmed Ahmed 
// CS4760
// Project 3(REDO)
// 12/18/23

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>

#define SHM_KEY 12345
#define MSG_QUEUE_KEY 1234

// Message structure used for message queue communication
typedef struct {
    long mtype;
    int data;
} Message;

// Clock structure to hold the shared clock information
typedef struct {
    int seconds;
    int nanoseconds;
} Clock;

int main(int argc, char *argv[]) {
    // Ensure correct number of command-line arguments
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <TermTimeS> <TermTimeNano>\n", argv[0]);
        exit(1);
    }

    // Parse termination time from command-line arguments
    int termTimeS = atoi(argv[1]);
    int termTimeNano = atoi(argv[2]);

    // Initial output upon starting
    printf("WORKER PID:%d PPID:%d Called with oss: TermTimeS: %d TermTimeNano: %d\n",
           getpid(), getppid(), termTimeS, termTimeNano);

    // Attach to shared memory for the system clock
    int clockShmId = shmget(SHM_KEY, sizeof(Clock), 0666);
    Clock *sharedClock = (Clock *)shmat(clockShmId, NULL, 0);

    // Access the message queue created by oss
    int msgQueueId = msgget(MSG_QUEUE_KEY, 0666);

    int lastReportedSecond = -1;
    while (1) {
        // Receive a message from oss
        Message msg;
        if (msgrcv(msgQueueId, &msg, sizeof(msg.data), getpid(), 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }
        printf("WORKER PID:%d PPID:%d --Received message\n", getpid(), getppid());

        // Output the time and check if a second has passed since last report
        if (sharedClock->seconds > lastReportedSecond) {
            printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
                   getpid(), getppid(), sharedClock->seconds, sharedClock->nanoseconds, termTimeS, termTimeNano);
            printf("--%d seconds have passed since starting\n", sharedClock->seconds - lastReportedSecond);
            lastReportedSecond = sharedClock->seconds;
        }

        // Check if the termination condition is met
        if (sharedClock->seconds >= termTimeS && sharedClock->nanoseconds >= termTimeNano) {
            printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --Terminating\n",
                   getpid(), getppid(), sharedClock->seconds, sharedClock->nanoseconds);
            msg.data = 0; // Indicating termination
            if (msgsnd(msgQueueId, &msg, sizeof(msg.data), 0) == -1) {
                perror("msgsnd failed");
                exit(1);
            }
            break; // Exit the loop and terminate
        } else {
            msg.data = 1; // Indicating continuation
            if (msgsnd(msgQueueId, &msg, sizeof(msg.data), 0) == -1) {
                perror("msgsnd failed");
                exit(1);
            }
        }
    }

    // Detach from the shared memory segment before terminating
    shmdt(sharedClock);
    return 0;
}
