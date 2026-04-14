/*
 * tubulus.c — communicatio per tubulum inter parentem et filium
 *
 * Demonstrat pipe(), fork(), write(), read().
 * Parens nuntium ad filium per tubulum mittit;
 * filius nuntium legit et scribit.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    int tub[2];

    if (pipe(tub) == -1) {
        perror("pipe");
        return 1;
    }

    pid_t filius = fork();
    if (filius == -1) {
        perror("fork");
        return 1;
    }

    if (filius == 0) {
        /* filius legit ex tubulo */
        close(tub[1]);
        char alveus[256];
        ssize_t num_lecta = read(tub[0], alveus, sizeof(alveus) - 1);
        if (num_lecta > 0) {
            alveus[num_lecta] = '\0';
            printf("filius accepit: %s\n", alveus);
        }
        close(tub[0]);
        return 0;
    }

    /* parens scribit in tubulum */
    close(tub[0]);
    const char *nuntius = "Salve, fili! Parens per tubulum loquitur.";
    write(tub[1], nuntius, strlen(nuntius));
    close(tub[1]);

    waitpid(filius, NULL, 0);
    printf("parens: filius perfecit\n");
    return 0;
}
