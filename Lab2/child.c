#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

//export CHILD_PATH=/home/morra/CLionProjects/Lab2

extern char **environ;

int main(int argc, char *argv[]) {
    printf("Name: %s, PID: %d, PPID: %d\n", argv[0], getpid(), getppid());

    if (argc == 2) {
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) {
            perror("fopen");
            exit(1);
        }
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            line[strcspn(line, "\n")] = 0;
            char *value = getenv(line);
            if (value != NULL) {
                printf("%s=%s\n", line, value);
            }
        }
        fclose(fp);
    } else if (argc == 1) {
        for (char **env = environ; *env != NULL; env++) {
            printf("%s\n", *env);
        }
    } else {
        fprintf(stderr, "Usage: %s [env_file]\n", argv[0]);
        exit(1);
    }

    return 0;
}