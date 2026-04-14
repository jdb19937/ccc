/*
 * explorator.c — explorator directoriorum
 *
 * Demonstrat mkdir(), opendir(), readdir(), stat(), rmdir(), unlink().
 * Directorium temporarium cum pliculis creat, inspicit, purgat.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *radix = "/tmp/explorator_temp";

static void purga_directorium(const char *via_dir)
{
    DIR *dir = opendir(via_dir);
    if (!dir)
        return;

    struct dirent *res;
    while ((res = readdir(dir)) != NULL) {
        if (res->d_name[0] == '.')
            continue;
        char via[256];
        snprintf(via, sizeof(via), "%s/%s", via_dir, res->d_name);
        unlink(via);
    }
    closedir(dir);
    rmdir(via_dir);
}

static void crea_plicam(const char *nomen)
{
    char via[256];
    snprintf(via, sizeof(via), "%s/%s", radix, nomen);
    int desc = open(via, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (desc != -1) {
        write(desc, nomen, strlen(nomen));
        close(desc);
    }
}

int main(void)
{
    /* purga residua si adsunt */
    purga_directorium(radix);

    if (mkdir(radix, 0755) == -1) {
        perror("mkdir");
        return 1;
    }

    crea_plicam("alpha.txt");
    crea_plicam("beta.txt");
    crea_plicam("gamma.txt");
    printf("directorium %s creatum cum tribus pliculis\n\n", radix);

    /* inspice directorium */
    DIR *dir = opendir(radix);
    if (!dir) {
        perror("opendir");
        return 1;
    }

    struct dirent *res;
    while ((res = readdir(dir)) != NULL) {
        if (res->d_name[0] == '.')
            continue;

        char via[256];
        snprintf(via, sizeof(via), "%s/%s", radix, res->d_name);

        struct stat info;
        if (stat(via, &info) == 0) {
            printf(
                "  %-12s  magnitudo=%4lld  modus=%04o\n",
                res->d_name, (long long)info.st_size,
                (unsigned)(info.st_mode & 0777)
            );
        }
    }
    closedir(dir);

    /* purga */
    printf("\npurgo...\n");
    purga_directorium(radix);
    printf("directorium %s deletum\n", radix);
    return 0;
}
