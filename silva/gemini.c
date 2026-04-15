/*
 * gemini.c — gemini processuum
 *
 * Demonstrat fork(), waitpid(), et pipe().
 * Plures filios creat; quisque per fistulam parentem certiorem facit.
 * Parens omnes exspectat et exitus ordine scribit.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>

#define NUMERUS_FILIORUM 4

int main(void)
{
    pid_t filii[NUMERUS_FILIORUM];
    int fistulae[NUMERUS_FILIORUM][2];   /* pipe per filium */

    printf("parens: creo %d filios\n", NUMERUS_FILIORUM);
    fflush(stdout);

    for (int i = 0; i < NUMERUS_FILIORUM; i++) {
        if (pipe(fistulae[i]) == -1) {
            perror("pipe");
            return 1;
        }
        filii[i] = fork();
        if (filii[i] == -1) {
            perror("fork");
            return 1;
        }
        if (filii[i] == 0) {
            close(fistulae[i][0]);       /* filius lectorem claudit */
            long summa = 0;
            for (long k = 1; k <= 10000L * (i + 1); k++)
                summa += k;
            char buf[64];
            int n = snprintf(buf, sizeof buf, "%ld", summa);
            write(fistulae[i][1], buf, (size_t)n);
            close(fistulae[i][1]);
            return 0;
        }
        close(fistulae[i][1]);           /* parens scriptorem claudit */
    }

    /* parens omnes filios ordine legit et exspectat */
    for (int i = 0; i < NUMERUS_FILIORUM; i++) {
        char buf[64];
        ssize_t nr = read(fistulae[i][0], buf, sizeof buf - 1);
        if (nr > 0)
            buf[nr] = '\0';
        else
            strcpy(buf, "?");
        close(fistulae[i][0]);

        int status;
        waitpid(filii[i], &status, 0);
        printf(
            "  filius %d: summa = %s (status %d)\n",
            i, buf, WEXITSTATUS(status)
        );
    }

    printf("parens: omnes filii perfecerunt\n");
    return 0;
}
