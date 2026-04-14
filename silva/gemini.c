/*
 * gemini.c — gemini processuum
 *
 * Demonstrat fork() et waitpid().
 * Plures filios creat; quisque opus suum agit.
 * Parens omnes exspectat.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include <unistd.h>
#include <sys/wait.h>

#define NUMERUS_FILIORUM 4

static void opus_filii(int index)
{
    pid_t ego  = getpid();
    long summa = 0;
    for (long i = 1; i <= 10000L * (index + 1); i++)
        summa += i;
    printf(
        "  filius %d (PID %d): summa = %ld\n",
        index, (int)ego, summa
    );
}

int main(void)
{
    pid_t filii[NUMERUS_FILIORUM];

    printf(
        "parens (PID %d): creo %d filios\n",
        (int)getpid(), NUMERUS_FILIORUM
    );

    for (int i = 0; i < NUMERUS_FILIORUM; i++) {
        filii[i] = fork();
        if (filii[i] == -1) {
            perror("fork");
            return 1;
        }
        if (filii[i] == 0) {
            opus_filii(i);
            return 0;
        }
    }

    /* parens exspectat omnes filios */
    for (int i = 0; i < NUMERUS_FILIORUM; i++) {
        int status;
        waitpid(filii[i], &status, 0);
        printf(
            "filius %d (PID %d) rediit cum statu %d\n",
            i, (int)filii[i], WEXITSTATUS(status)
        );
    }

    printf("parens: omnes filii perfecerunt\n");
    return 0;
}
