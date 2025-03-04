#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <locale.h>
#include <termios.h>

extern char **environ;

int compare(const void *a, const void *b) {
    return strcoll(*(char **)a, *(char **)b);
}

int main() {
    //сортировка и вывод переменных окружения родителя
    char *old_locale = setlocale(LC_COLLATE, NULL);
    setlocale(LC_COLLATE, "C");
    int env_count_orig = 0;
    while (environ[env_count_orig] != NULL) env_count_orig++;
    char **env_copy = malloc(env_count_orig * sizeof(char *));
    for (int i = 0; i < env_count_orig; i++) env_copy[i] = environ[i];
    qsort(env_copy, env_count_orig, sizeof(char *), compare);
    for (int i = 0; i < env_count_orig; i++) printf("%s\n", env_copy[i]);
    free(env_copy);
    setlocale(LC_COLLATE, old_locale);

    //чтение файла env и создание сокращенной среды для child
    char **child_envp = NULL;
    int env_count = 0;
    FILE *fp = fopen("env", "r");
    if (fp == NULL) {
        perror("fopen env");
        exit(1);
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\n")] = 0;
        char *value = getenv(line);
        if (value != NULL) {
            char *env_str = malloc(strlen(line) + strlen(value) + 2);
            sprintf(env_str, "%s=%s", line, value);
            child_envp = realloc(child_envp, (env_count + 1) * sizeof(char *));
            child_envp[env_count] = env_str;
            env_count++;
        }
    }
    fclose(fp);
    child_envp = realloc(child_envp, (env_count + 1) * sizeof(char *));
    child_envp[env_count] = NULL;

    //получение пути к child из CHILD_PATH
    char *child_path = getenv("CHILD_PATH");
    if (child_path == NULL) {
        fprintf(stderr, "CHILD_PATH not set\n");
        exit(1);
    }
    char full_path[256];
    snprintf(full_path, sizeof(full_path), "%s/child", child_path);

    //настройка терминала в небуферизированный режим
    struct termios orig_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    //цикл обработки ввода
    int child_num = 0;
    while (1) {
        char key;
        if (read(STDIN_FILENO, &key, 1) == 1) {
            if (key == 'q') break; // Выход из цикла
            if (key == '+' || key == '*') {
                char child_name[10];
                sprintf(child_name, "child_%02d", child_num);

                char *argv[3];
                argv[0] = child_name;
                if (key == '+') {
                    argv[1] = "env";
                    argv[2] = NULL;
                } else { // key == '*'
                    argv[1] = NULL;
                }

                pid_t pid = fork();
                if (pid == 0) {
                    execve(full_path, argv, child_envp);
                    perror("execve");
                    exit(1);
                } else if (pid > 0) {
                    int status;
                    waitpid(pid, &status, 0);
                    child_num = (child_num + 1) % 100;
                } else {
                    perror("fork");
                }
            }
        }
    }

    //восстановление настроек терминала и освобождение памяти
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    for (int i = 0; i < env_count; i++) free(child_envp[i]);
    free(child_envp);

    return 0;
}