/*
 * vigil.c — vigil signorum
 *
 * Demonstrat sigaction() et kill().
 * Processus signa SIGUSR1 et SIGUSR2 sibi mittit et captat.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include <signal.h>
#include <unistd.h>

static volatile sig_atomic_t signa_capta = 0;

static void tracta_signum(int signum)
{
    signa_capta++;
    if (signum == SIGUSR1)
        write(STDOUT_FILENO, "  vigil: SIGUSR1 captum\n", 24);
    else
        write(STDOUT_FILENO, "  vigil: SIGUSR2 captum\n", 24);
}

int main(void)
{
    struct sigaction actio;
    actio.sa_handler = tracta_signum;
    actio.sa_flags   = 0;
    sigemptyset(&actio.sa_mask);

    if (sigaction(SIGUSR1, &actio, NULL) == -1) {
        perror("sigaction SIGUSR1");
        return 1;
    }
    if (sigaction(SIGUSR2, &actio, NULL) == -1) {
        perror("sigaction SIGUSR2");
        return 1;
    }

    pid_t ego = getpid();
    printf("vigil: mitto signa...\n");

    kill(ego, SIGUSR1);
    kill(ego, SIGUSR2);
    kill(ego, SIGUSR1);

    printf("signa capta: %d\n", (int)signa_capta);
    return 0;
}
