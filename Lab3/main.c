#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#define CYCLE_LIMIT 101
#define MAX_CHILDREN 100

pid_t children[MAX_CHILDREN];
int child_count = 0;

void parent_sigusr1_handler(int signo, siginfo_t *info, void *context) {
    (void)signo; (void)context;
    pid_t child_pid = info->si_pid;
    printf("Parent (PID=%d): Received ready signal from child (PID=%d)\n", getpid(), child_pid);
    fflush(stdout);
    kill(child_pid, SIGUSR2);
}

void setup_parent_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_sigaction = parent_sigusr1_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("parent sigaction");
        exit(EXIT_FAILURE);
    }
}

struct pair {
    int a;
    int b;
};

volatile struct pair data;
volatile sig_atomic_t count00 = 0, count01 = 0, count10 = 0, count11 = 0;
volatile sig_atomic_t cycle_count = 0;

void child_sigusr1_handler(int signo) {
    (void)signo;
    int a = data.a;
    int b = data.b;
    if(a == 0 && b == 0)
        count00++;
    else if(a == 0 && b == 1)
        count01++;
    else if(a == 1 && b == 0)
        count10++;
    else if(a == 1 && b == 1)
        count11++;
    cycle_count++;
}

void child_sigusr2_handler(int signo) {
    (void)signo;
}

void setup_child_signal_handlers(void) {
    struct sigaction sa;
    sa.sa_handler = child_sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if(sigaction(SIGUSR1, &sa, NULL) == -1) {
        perror("child sigaction SIGUSR1");
        exit(EXIT_FAILURE);
    }
    sa.sa_handler = child_sigusr2_handler;
    if(sigaction(SIGUSR2, &sa, NULL) == -1) {
        perror("child sigaction SIGUSR2");
        exit(EXIT_FAILURE);
    }
}

void child_process(void) {
    setup_child_signal_handlers();

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100 * 1000000;

    int toggle = 0;
    for(;;) {
        nanosleep(&ts, NULL);

        if(toggle) {
            data.a = 0;
            for(volatile int i = 0; i < 100000; i++);
            data.b = 0;
        } else {
            data.a = 1;
            for(volatile int i = 0; i < 100000; i++);
            data.b = 1;
        }
        toggle = !toggle;

        raise(SIGUSR1);

        if(cycle_count >= CYCLE_LIMIT) {
            kill(getppid(), SIGUSR1);
            pause();
            printf("Child: PPID=%d, PID=%d, (0,0)=%d, (0,1)=%d, (1,0)=%d, (1,1)=%d\n",
                   getppid(), getpid(), count00, count01, count10, count11);
            fflush(stdout);
            cycle_count = 0;
            count00 = count01 = count10 = count11 = 0;
        }
    }
}

int main(void) {
    setup_parent_signal_handlers();
    printf("Parent process started. PID=%d\n", getpid());
    fflush(stdout);

    char ch;
    while(1) {
        ch = getchar();
        if(ch == EOF)
            continue;
        if(ch == '+') {
            pid_t pid = fork();
            if(pid < 0) {
                perror("fork");
            } else if(pid == 0) {
                child_process();
                exit(EXIT_SUCCESS);
            } else {
                if(child_count < MAX_CHILDREN) {
                    children[child_count++] = pid;
                    printf("Parent: Created child, PID=%d. Total children: %d\n", pid, child_count);
                } else {
                    printf("Parent: Maximum children reached!\n");
                }
                fflush(stdout);
            }
        } else if(ch == '-') {
            if(child_count > 0) {
                pid_t pid = children[child_count - 1];
                kill(pid, SIGTERM);
                waitpid(pid, NULL, 0);
                child_count--;
                printf("Parent: Killed child PID=%d. Remaining children: %d\n", pid, child_count);
                fflush(stdout);
            } else {
                printf("Parent: No children to kill.\n");
                fflush(stdout);
            }
        } else if(ch == 'l') {
            printf("Parent: PID=%d\n", getpid());
            for(int i = 0; i < child_count; i++) {
                printf("Child[%d]: PID=%d\n", i, children[i]);
            }
            fflush(stdout);
        } else if(ch == 'k') {
            for(int i = 0; i < child_count; i++) {
                kill(children[i], SIGTERM);
                waitpid(children[i], NULL, 0);
            }
            child_count = 0;
            printf("Parent: Killed all children.\n");
            fflush(stdout);
        } else if(ch == 'q') {
            for(int i = 0; i < child_count; i++) {
                kill(children[i], SIGTERM);
                waitpid(children[i], NULL, 0);
            }
            printf("Parent: Killed all children. Exiting.\n");
            fflush(stdout);
            exit(EXIT_SUCCESS);
        }
    }
}
