/*
 * nexus.c — nexus per par basium (socketpair)
 *
 * Demonstrat socketpair(), fork(), send(), recv().
 * Parens et filius per par basium UNIX communicant.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int bases[2];

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, bases) == -1) {
        perror("socketpair");
        return 1;
    }

    pid_t filius = fork();
    if (filius == -1) {
        perror("fork");
        return 1;
    }

    if (filius == 0) {
        /* filius: lege et responde */
        close(bases[0]);
        char alveus[256];
        ssize_t n = recv(bases[1], alveus, sizeof(alveus) - 1, 0);
        if (n > 0) {
            alveus[n] = '\0';
            printf("  filius accepit: %s\n", alveus);
        }
        const char *resp = "Etiam, parens. Filius respondet.";
        send(bases[1], resp, strlen(resp), 0);
        close(bases[1]);
        return 0;
    }

    /* parens: scribe et lege responsum */
    close(bases[1]);
    const char *nuntius = "Esne ibi, fili?";
    send(bases[0], nuntius, strlen(nuntius), 0);

    char alveus[256];
    ssize_t n = recv(bases[0], alveus, sizeof(alveus) - 1, 0);
    if (n > 0) {
        alveus[n] = '\0';
        printf("  parens accepit: %s\n", alveus);
    }

    close(bases[0]);
    waitpid(filius, NULL, 0);
    printf("nexus clausus\n");
    return 0;
}
