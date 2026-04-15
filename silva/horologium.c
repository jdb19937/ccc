/*
 * horologium.c — horologium signorum
 *
 * Demonstrat alarm(), sigaction(SIGALRM), pause().
 * Tria intervalla per SIGALRM metitur.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include <signal.h>
#include <unistd.h>

static volatile sig_atomic_t pulsus = 0;

static void tracta_alarma(int signum)
{
    (void)signum;
    pulsus++;
}

int main(void)
{
    struct sigaction actio;
    actio.sa_handler = tracta_alarma;
    actio.sa_flags   = 0;
    sigemptyset(&actio.sa_mask);

    if (sigaction(SIGALRM, &actio, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    printf("horologium: exspecto tres pulsus...\n");
    ualarm(100000, 0);  /* 100 ms */

    while (pulsus < 3) {
        pause();
        printf("  pulsus %d receptus\n", (int)pulsus);
        if (pulsus < 3)
            ualarm(100000, 0);
    }

    printf("horologium: tres pulsus recepti, finis\n");
    return 0;
}
