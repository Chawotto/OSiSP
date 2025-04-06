#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>

#define QUEUE_SIZE 10
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

enum {
    SEM_MUTEX = 0,
    SEM_FILLCOUNT,
    SEM_EMPTYCOUNT,
    SEM_COUNT
};

typedef struct {
    unsigned char  type;
    unsigned short hash;
    unsigned char  size;
    unsigned char  data[256];
} message_t;

typedef struct {
    message_t buffer[QUEUE_SIZE];
    int head;
    int tail;
    unsigned long producedCount;
    unsigned long consumedCount;
    int producers;
    int consumers;
} shm_data_t;

static int shmid = -1;
static int semid = -1;
static shm_data_t *shm_ptr = NULL;
static volatile sig_atomic_t needTerminate = 0;

int sem_op(int sem_id, int sem_num, int op) {
    struct sembuf sb;
    sb.sem_num = sem_num;
    sb.sem_op  = op;
    sb.sem_flg = 0;
    return semop(sem_id, &sb, 1);
}

int sem_setval(int sem_id, int sem_num, int val) {
    union semun {
        int val;
        struct semid_ds *buf;
        unsigned short  *array;
    } argument;
    argument.val = val;
    return semctl(sem_id, sem_num, SETVAL, argument);
}

unsigned short compute_hash(const message_t *msg) {
    unsigned short result = 0;
    unsigned short old_hash = msg->hash;
    message_t temp = *msg;
    temp.hash = 0;
    int data_len = (int)temp.size;
    if (data_len > 256) {
        data_len = 256;
    }
    result += temp.type;
    result += temp.size;
    for (int i = 0; i < data_len; i++) {
        result += temp.data[i];
    }
    (void)old_hash;
    return result;
}

int verify_hash(const message_t *msg) {
    unsigned short real_hash = compute_hash(msg);
    return (real_hash == msg->hash) ? 1 : 0;
}

void fill_random_message(message_t *msg) {
    msg->type = (unsigned char)(rand() % 256);
    msg->size = (unsigned char)(rand() % 256);
    for (int i = 0; i < msg->size; i++) {
        msg->data[i] = (unsigned char)(rand() % 256);
    }
    for (int i = msg->size; i < 256; i++) {
        msg->data[i] = 0;
    }
    msg->hash = 0;
    unsigned short h = compute_hash(msg);
    msg->hash = h;
}

void sig_handler(int signo) {
    (void)signo;
    needTerminate = 1;
}

void producer_loop(void) {
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
    while (!needTerminate) {
        if (sem_op(semid, SEM_EMPTYCOUNT, -1) == -1) {
            if (errno == EINTR) continue;
            break;
        }
        if (sem_op(semid, SEM_MUTEX, -1) == -1) {
            break;
        }
        int tail = shm_ptr->tail;
        fill_random_message(&shm_ptr->buffer[tail]);
        shm_ptr->tail = (tail + 1) % QUEUE_SIZE;
        shm_ptr->producedCount++;
        unsigned long producedNow = shm_ptr->producedCount;
        if (sem_op(semid, SEM_MUTEX, 1) == -1) {
            break;
        }
        if (sem_op(semid, SEM_FILLCOUNT, 1) == -1) {
            break;
        }
        printf("[Producer %d] Produced message #%lu (type=%u, size=%u)\n",
               getpid(), producedNow,
               (unsigned)shm_ptr->buffer[tail].type,
               (unsigned)shm_ptr->buffer[tail].size);
        fflush(stdout);
        sleep(1);
    }
    _exit(0);
}

void consumer_loop(void) {
    while (!needTerminate) {
        if (sem_op(semid, SEM_FILLCOUNT, -1) == -1) {
            if (errno == EINTR) continue;
            break;
        }
        if (sem_op(semid, SEM_MUTEX, -1) == -1) {
            break;
        }
        int head = shm_ptr->head;
        message_t msg = shm_ptr->buffer[head];
        shm_ptr->head = (head + 1) % QUEUE_SIZE;
        shm_ptr->consumedCount++;
        unsigned long consumedNow = shm_ptr->consumedCount;
        if (sem_op(semid, SEM_MUTEX, 1) == -1) {
            break;
        }
        if (sem_op(semid, SEM_EMPTYCOUNT, 1) == -1) {
            break;
        }
        int ok = verify_hash(&msg);
        printf("[Consumer %d] Consumed message #%lu (type=%u, size=%u, hash_ok=%s)\n",
               getpid(), consumedNow, (unsigned)msg.type,
               (unsigned)msg.size, ok ? "YES" : "NO");
        fflush(stdout);
        sleep(1);
    }
    _exit(0);
}

int main(void) {
    signal(SIGINT, SIG_IGN);
    semid = semget(SEM_KEY, SEM_COUNT, IPC_CREAT | 0666);
    if (semid < 0) {
        perror("semget");
        exit(1);
    }
    if (sem_setval(semid, SEM_MUTEX, 1) < 0) {
        perror("semctl(mutex)");
        exit(1);
    }
    if (sem_setval(semid, SEM_FILLCOUNT, 0) < 0) {
        perror("semctl(fillcount)");
        exit(1);
    }
    if (sem_setval(semid, SEM_EMPTYCOUNT, QUEUE_SIZE) < 0) {
        perror("semctl(emptycount)");
        exit(1);
    }
    shmid = shmget(SHM_KEY, sizeof(shm_data_t), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    shm_ptr = (shm_data_t*)shmat(shmid, NULL, 0);
    if (shm_ptr == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    shm_ptr->head = 0;
    shm_ptr->tail = 0;
    shm_ptr->producedCount = 0;
    shm_ptr->consumedCount = 0;
    shm_ptr->producers = 0;
    shm_ptr->consumers = 0;
    pid_t producers_pid[100];
    pid_t consumers_pid[100];
    int pCount = 0;
    int cCount = 0;

    printf("Press:\n"
           "  p - spawn producer\n"
           "  c - spawn consumer\n"
           "  P - kill one producer\n"
           "  C - kill one consumer\n"
           "  s - show status\n"
           "  q - quit\n");

    while (1) {
        int ch = getchar();
        if (ch == EOF) {
            break;
        }
        if (ch == '\n') {
            continue;
        }
        if (ch == 'p') {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork producer");
            } else if (pid == 0) {
                signal(SIGINT, sig_handler);
                signal(SIGTERM, sig_handler);
                producer_loop();
            } else {
                producers_pid[pCount++] = pid;
                shm_ptr->producers++;
                printf("Spawned producer PID=%d\n", pid);
            }
        } else if (ch == 'c') {
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork consumer");
            } else if (pid == 0) {
                signal(SIGINT, sig_handler);
                signal(SIGTERM, sig_handler);
                consumer_loop();
            } else {
                consumers_pid[cCount++] = pid;
                shm_ptr->consumers++;
                printf("Spawned consumer PID=%d\n", pid);
            }
        } else if (ch == 'P') {
            if (pCount > 0) {
                pid_t killPid = producers_pid[pCount - 1];
                kill(killPid, SIGTERM);
                waitpid(killPid, NULL, 0);
                printf("Killed producer PID=%d\n", killPid);
                pCount--;
                shm_ptr->producers--;
            } else {
                printf("No producers to kill.\n");
            }
        } else if (ch == 'C') {
            if (cCount > 0) {
                pid_t killPid = consumers_pid[cCount - 1];
                kill(killPid, SIGTERM);
                waitpid(killPid, NULL, 0);
                printf("Killed consumer PID=%d\n", killPid);
                cCount--;
                shm_ptr->consumers--;
            } else {
                printf("No consumers to kill.\n");
            }
        } else if (ch == 's') {
            int head = shm_ptr->head;
            int tail = shm_ptr->tail;
            int used = (tail >= head) ? (tail - head) : (QUEUE_SIZE - head + tail);
            int free_slots = QUEUE_SIZE - used;
            printf("Queue status:\n");
            printf("  producers: %d\n", shm_ptr->producers);
            printf("  consumers: %d\n", shm_ptr->consumers);
            printf("  producedCount: %lu\n", shm_ptr->producedCount);
            printf("  consumedCount: %lu\n", shm_ptr->consumedCount);
            printf("  queue used: %d\n", used);
            printf("  queue free: %d\n", free_slots);
        } else if (ch == 'q') {
            break;
        } else {
            printf("Unknown command '%c'\n", ch);
        }
    }

    for (int i = 0; i < pCount; i++) {
        kill(producers_pid[i], SIGTERM);
    }
    for (int i = 0; i < cCount; i++) {
        kill(consumers_pid[i], SIGTERM);
    }
    for (int i = 0; i < pCount; i++) {
        waitpid(producers_pid[i], NULL, 0);
    }
    for (int i = 0; i < cCount; i++) {
        waitpid(consumers_pid[i], NULL, 0);
    }
    if (semctl(semid, 0, IPC_RMID, 0) < 0) {
        perror("semctl(IPC_RMID)");
    }
    if (shmdt(shm_ptr) < 0) {
        perror("shmdt");
    }
    if (shmctl(shmid, IPC_RMID, NULL) < 0) {
        perror("shmctl(IPC_RMID)");
    }
    printf("Main process exiting.\n");
    return 0;
}