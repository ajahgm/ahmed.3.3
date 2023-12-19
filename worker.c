// worker.c
// Ahmed Ahmed CS4760 10/12/23

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>

#define SHM_KEY 12345
#define MSG_QUEUE_KEY 1234

typedef struct {
    long mtype;
    int data;
} Message;

typedef struct {
    int seconds;
    int nanoseconds;
} Clock;

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <TermTimeS> <TermTimeNano>\n", argv[0]);
        exit(1);
    }
    int termTimeS = atoi(argv[1]);
    int termTimeNano = atoi(argv[2]);

    printf("WORKER PID:%d PPID:%d Called with oss: TermTimeS: %d TermTimeNano: %d\n",
           getpid(), getppid(), termTimeS, termTimeNano);

    int clockShmId = shmget(SHM_KEY, sizeof(Clock), 0666);
    Clock *sharedClock = (Clock *)shmat(clockShmId, NULL, 0);

    int msgQueueId = msgget(MSG_QUEUE_KEY, 0666);

    int lastReportedSecond = -1;
    while (1) {
        Message msg;
        if (msgrcv(msgQueueId, &msg, sizeof(msg.data), getpid(), 0) == -1) {
            perror("msgrcv failed");
            exit(1);
        }
        printf("WORKER PID:%d PPID:%d --Received message\n", getpid(), getppid());

        if (sharedClock->seconds > lastReportedSecond) {
            printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d\n",
                   getpid(), getppid(), sharedClock->seconds, sharedClock->nanoseconds, termTimeS, termTimeNano);
            printf("--%d seconds have passed since starting\n", sharedClock->seconds - lastReportedSecond);
            lastReportedSecond = sharedClock->seconds;
        }

        if (sharedClock->seconds >= termTimeS && sharedClock->nanoseconds >= termTimeNano) {
            printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d --Terminating\n",
                   getpid(), getppid(), sharedClock->seconds, sharedClock->nanoseconds);
            msg.data = 0; // Indicating termination
            if (msgsnd(msgQueueId, &msg, sizeof(msg.data), 0) == -1) {
                perror("msgsnd failed");
                exit(1);
            }
            break;
        } else {
            msg.data = 1; // Indicating continuation
            if (msgsnd(msgQueueId, &msg, sizeof(msg.data), 0) == -1) {
                perror("msgsnd failed");
                exit(1);
            }
        }
    }

    shmdt(sharedClock);
    return 0;
}
