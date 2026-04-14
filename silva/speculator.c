/*
 * speculator.c — speculator multiplexus (select)
 *
 * Demonstrat pipe(), select(), fork().
 * Plures tubulos creat et per select() multiplexat.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

#define NUMERUS_TUBULORUM 3

int main(void)
{
    int tubuli[NUMERUS_TUBULORUM][2];
    pid_t filii[NUMERUS_TUBULORUM];

    for (int i = 0; i < NUMERUS_TUBULORUM; i++) {
        if (pipe(tubuli[i]) == -1) {
            perror("pipe");
            return 1;
        }

        filii[i] = fork();
        if (filii[i] == -1) {
            perror("fork");
            return 1;
        }

        if (filii[i] == 0) {
            close(tubuli[i][0]);
            char nuntius[64];
            snprintf(
                nuntius, sizeof(nuntius),
                "Nuntius a filio %d", i
            );
            write(tubuli[i][1], nuntius, strlen(nuntius));
            close(tubuli[i][1]);
            return 0;
        }

        close(tubuli[i][1]);
    }

    /* parens legit per select() */
    int residua = NUMERUS_TUBULORUM;
    int aperti[NUMERUS_TUBULORUM];
    for (int i = 0; i < NUMERUS_TUBULORUM; i++)
        aperti[i] = 1;

    printf("speculator: exspecto nuntios per select()...\n");

    while (residua > 0) {
        fd_set lectura;
        FD_ZERO(&lectura);
        int max_desc = -1;

        for (int i = 0; i < NUMERUS_TUBULORUM; i++) {
            if (aperti[i]) {
                FD_SET(tubuli[i][0], &lectura);
                if (tubuli[i][0] > max_desc)
                    max_desc = tubuli[i][0];
            }
        }

        int paratum = select(max_desc + 1, &lectura, NULL, NULL, NULL);
        if (paratum <= 0)
            break;

        for (int i = 0; i < NUMERUS_TUBULORUM; i++) {
            if (!aperti[i])
                continue;
            if (!FD_ISSET(tubuli[i][0], &lectura))
                continue;

            char alveus[256];
            ssize_t n = read(
                tubuli[i][0], alveus, sizeof(alveus) - 1
            );
            if (n > 0) {
                alveus[n] = '\0';
                printf("  tubulus %d: %s\n", i, alveus);
            }
            close(tubuli[i][0]);
            aperti[i] = 0;
            residua--;
        }
    }

    for (int i = 0; i < NUMERUS_TUBULORUM; i++)
        waitpid(filii[i], NULL, 0);

    printf("speculator: omnes nuntii recepti\n");
    return 0;
}
