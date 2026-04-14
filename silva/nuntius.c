/*
 * nuntius.c — nuntius per memoriam communem
 *
 * Demonstrat mmap(MAP_SHARED), fork(), ftruncate().
 * Parens et filius per memoriam communem communicant.
 * Tubulus pro synchronizatione adhibetur.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAGNITUDO 4096

int main(void)
{
    const char *via = "/tmp/nuntius_memoria.tmp";

    /* crea plicam et extende */
    int desc = open(via, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (desc == -1) {
        perror("open");
        return 1;
    }
    if (ftruncate(desc, MAGNITUDO) == -1) {
        perror("ftruncate");
        close(desc);
        unlink(via);
        return 1;
    }

    /* mappa in memoriam communem */
    char *regio = mmap(
        NULL, MAGNITUDO, PROT_READ | PROT_WRITE,
        MAP_SHARED, desc, 0
    );
    if (regio == MAP_FAILED) {
        perror("mmap");
        close(desc);
        unlink(via);
        return 1;
    }
    close(desc);

    /* tubulus pro synchronizatione */
    int tub[2];
    if (pipe(tub) == -1) {
        perror("pipe");
        munmap(regio, MAGNITUDO);
        unlink(via);
        return 1;
    }

    pid_t filius = fork();
    if (filius == -1) {
        perror("fork");
        munmap(regio, MAGNITUDO);
        unlink(via);
        return 1;
    }

    if (filius == 0) {
        /* filius: exspecta signum, deinde lege */
        close(tub[1]);
        char c;
        read(tub[0], &c, 1);
        close(tub[0]);
        printf("filius legit: %s\n", regio);
        munmap(regio, MAGNITUDO);
        return 0;
    }

    /* parens: scribe in memoriam, deinde signa */
    close(tub[0]);
    const char *nuntius = "Ave, Caesar! Morituri te salutant.";
    memcpy(regio, nuntius, strlen(nuntius) + 1);
    printf("parens scripsit: %s\n", regio);
    write(tub[1], "x", 1);
    close(tub[1]);

    waitpid(filius, NULL, 0);
    munmap(regio, MAGNITUDO);
    unlink(via);
    printf("memoria communis purgata\n");
    return 0;
}
