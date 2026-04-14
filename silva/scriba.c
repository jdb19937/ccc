/*
 * scriba.c — scriba plicarum
 *
 * Demonstrat open(), write(), lseek(), read(), close(), unlink().
 * Plicam temporariam creat, scribit, quaerit, relegit, delet.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    const char *via = "/tmp/scriba_temporaria.txt";

    int desc = open(via, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (desc == -1) {
        perror("open");
        return 1;
    }

    const char *versus_i  = "Ars longa, vita brevis.\n";
    const char *versus_ii = "Verba volant, scripta manent.\n";
    write(desc, versus_i, strlen(versus_i));
    write(desc, versus_ii, strlen(versus_ii));

    /* quaere ad principium */
    off_t positio = lseek(desc, 0, SEEK_SET);
    printf("positio post lseek: %lld\n", (long long)positio);

    /* relege totum */
    char alveus[256];
    ssize_t num_lecta = read(desc, alveus, sizeof(alveus) - 1);
    if (num_lecta > 0) {
        alveus[num_lecta] = '\0';
        printf("lectum (%zd octeti):\n%s", num_lecta, alveus);
    }

    close(desc);
    unlink(via);
    printf("plica temporaria deleta\n");
    return 0;
}
