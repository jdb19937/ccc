/*
 * memoria.c — memoria mappata
 *
 * Demonstrat open(), ftruncate(), mmap(), msync(), munmap().
 * Plicam per mmap scribit et per read() relegit.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAGNITUDO 4096

int main(void)
{
    const char *via = "/tmp/memoria_mappata.tmp";

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

    /* mappa plicam */
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

    /* scribe per mappam */
    const char *textus = "Cogito, ergo sum.";
    memcpy(regio, textus, strlen(textus) + 1);
    printf("scriptum per mmap: %s\n", regio);

    /* synchroniza ad discum */
    msync(regio, MAGNITUDO, MS_SYNC);
    munmap(regio, MAGNITUDO);

    /* relege per read() */
    lseek(desc, 0, SEEK_SET);
    char alveus[256];
    ssize_t num_lecta = read(desc, alveus, sizeof(alveus) - 1);
    if (num_lecta > 0) {
        alveus[num_lecta] = '\0';
        printf("relectum per read: %s\n", alveus);
    }

    close(desc);
    unlink(via);
    printf("plica %s deleta\n", via);
    return 0;
}
