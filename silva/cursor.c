/*
 * cursor.c — cursor mandatorum (exec)
 *
 * Demonstrat fork(), execvp(), waitpid(), _exit().
 * Plura mandata in filiis exsequitur.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include <sys/wait.h>
#include <unistd.h>

static void exsequere(const char *nomen, char *const argumenta[])
{
    pid_t filius = fork();
    if (filius == -1) {
        perror("fork");
        return;
    }

    if (filius == 0) {
        execvp(argumenta[0], argumenta);
        perror("execvp");
        _exit(127);
    }

    int status;
    waitpid(filius, &status, 0);
    if (WIFEXITED(status))
        printf("  %s: status = %d\n", nomen, WEXITSTATUS(status));
}

int main(void)
{
    printf("cursor: exsequor mandata:\n\n");

    char *mandatum_i[]   = {"echo", "Alea iacta est!", NULL};
    char *mandatum_ii[]  = {"uname", "-s", NULL};
    char *mandatum_iii[] = {"date", "+%Y-%m-%d", NULL};

    exsequere("echo", mandatum_i);
    exsequere("uname", mandatum_ii);
    exsequere("date", mandatum_iii);

    printf("\ncursor: omnia mandata perfecta\n");
    return 0;
}
