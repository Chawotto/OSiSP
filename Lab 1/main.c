#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <locale.h>

void ListEntries(const char* dir, int show_links, int show_dirs, int show_files, int sort);
int CompareEntries(const void* a, const void* b);

int main(int argc, char* argv[]) {
    int opt;
    int show_links = 0, show_dirs = 0, show_files = 0, sort = 0;
    const char* dir = ".";

    while ((opt = getopt(argc, argv, "ldfs")) != -1) {
        switch (opt) {
            case 'l':
                show_links = 1;
                break;
            case 'd':
                show_dirs = 1;
                break;
            case 'f':
                show_files = 1;
                break;
            case 's':
                sort = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [dir] [-l] [-d] [-f] [-s]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (optind < argc) {
        dir = argv[optind];
    }

    if (sort) {
        setlocale(LC_COLLATE, "");
    }

    ListEntries(dir, show_links, show_dirs, show_files, sort);

    return 0;
}

void ListEntries(const char* dir, int show_links, int show_dirs, int show_files, int sort) {
    DIR* dp;
    struct dirent* entry;
    struct stat statbuf;
    char fullpath[PATH_MAX];
    struct dirent** namelist = NULL;
    int n = 0;

    if ((dp = opendir(dir)) == NULL) {
        perror("opendir");
        exit(EXIT_FAILURE);
    }

    while ((entry = readdir(dp)) != NULL) {
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, entry->d_name);
        if (lstat(fullpath, &statbuf) == -1) {
            perror("lstat");
            continue;
        }

        if (sort) {
            namelist = realloc(namelist, (n + 1) * sizeof(struct dirent*));
            namelist[n] = malloc(sizeof(struct dirent));
            memcpy(namelist[n], entry, sizeof(struct dirent));
            n++;
        } else {
            if ((S_ISDIR(statbuf.st_mode) && show_dirs) ||
                (S_ISREG(statbuf.st_mode) && show_files) ||
                (S_ISLNK(statbuf.st_mode) && show_links)) {
                printf("%s\n", fullpath);
            }
        }
    }
    closedir(dp);

    if (sort) {
        qsort(namelist, n, sizeof(struct dirent*), CompareEntries);
        for (int i = 0; i < n; i++) {
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dir, namelist[i]->d_name);
            if (lstat(fullpath, &statbuf) == -1) {
                perror("lstat");
                continue;
            }

            if ((S_ISDIR(statbuf.st_mode) && show_dirs) ||
                (S_ISREG(statbuf.st_mode) && show_files) ||
                (S_ISLNK(statbuf.st_mode) && show_links)) {
                printf("%s\n", fullpath);
            }
            free(namelist[i]);
        }
        free(namelist);
    }
}

int CompareEntries(const void* a, const void* b) {
    const struct dirent* entry_a = *(const struct dirent**)a;
    const struct dirent* entry_b = *(const struct dirent**)b;
    return strcmp(entry_a->d_name, entry_b->d_name);
}
