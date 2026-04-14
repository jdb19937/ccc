/*
 * ambulator.c — ambulator arboris directorii
 *
 * Demonstrat mkdir(), opendir(), readdir(), stat(), rmdir(), unlink().
 * Arborem directoriorum creat, recursim ambulat, purgat.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static void crea_plicam(const char *via)
{
    int desc = open(via, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (desc != -1) {
        write(desc, "data", 4);
        close(desc);
    }
}

static void ambula(const char *via, int profunditas)
{
    DIR *dir = opendir(via);
    if (!dir)
        return;

    struct dirent *res;
    while ((res = readdir(dir)) != NULL) {
        if (strcmp(res->d_name, ".") == 0)
            continue;
        if (strcmp(res->d_name, "..") == 0)
            continue;

        char plena_via[512];
        snprintf(
            plena_via, sizeof(plena_via),
            "%s/%s", via, res->d_name
        );

        struct stat info;
        if (stat(plena_via, &info) != 0)
            continue;

        for (int i = 0; i < profunditas; i++)
            printf("  ");

        if (S_ISDIR(info.st_mode)) {
            printf("[dir] %s/\n", res->d_name);
            ambula(plena_via, profunditas + 1);
        } else {
            printf(
                "      %s (%lld oct)\n",
                res->d_name, (long long)info.st_size
            );
        }
    }
    closedir(dir);
}

static void purga(const char *via)
{
    DIR *dir = opendir(via);
    if (!dir)
        return;

    struct dirent *res;
    while ((res = readdir(dir)) != NULL) {
        if (strcmp(res->d_name, ".") == 0)
            continue;
        if (strcmp(res->d_name, "..") == 0)
            continue;

        char plena_via[512];
        snprintf(
            plena_via, sizeof(plena_via),
            "%s/%s", via, res->d_name
        );

        struct stat info;
        if (stat(plena_via, &info) != 0)
            continue;

        if (S_ISDIR(info.st_mode))
            purga(plena_via);
        else
            unlink(plena_via);
    }
    closedir(dir);
    rmdir(via);
}

int main(void)
{
    const char *arbor = "/tmp/ambulator_arbor";
    char via[512];

    /* purga residua si adsunt */
    purga(arbor);

    /* crea arborem */
    mkdir(arbor, 0755);

    snprintf(via, sizeof(via), "%s/ramus_alpha", arbor);
    mkdir(via, 0755);
    snprintf(via, sizeof(via), "%s/ramus_alpha/folium.txt", arbor);
    crea_plicam(via);

    snprintf(via, sizeof(via), "%s/ramus_beta", arbor);
    mkdir(via, 0755);
    snprintf(via, sizeof(via), "%s/ramus_beta/flos.txt", arbor);
    crea_plicam(via);
    snprintf(via, sizeof(via), "%s/ramus_beta/fructus.txt", arbor);
    crea_plicam(via);

    snprintf(via, sizeof(via), "%s/ramus_beta/subramus", arbor);
    mkdir(via, 0755);
    snprintf(
        via, sizeof(via),
        "%s/ramus_beta/subramus/semen.txt", arbor
    );
    crea_plicam(via);

    snprintf(via, sizeof(via), "%s/radix.txt", arbor);
    crea_plicam(via);

    /* ambula arborem */
    printf("arbor %s:\n", arbor);
    ambula(arbor, 1);

    /* purga */
    printf("\npurgo arborem...\n");
    purga(arbor);
    printf("arbor deleta\n");
    return 0;
}
