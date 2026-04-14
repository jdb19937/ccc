/*
 * custos.c — custos plicarum (file locking)
 *
 * Demonstrat open(), fcntl() cum F_SETLK/F_GETLK.
 * Parens et filius claustra consultiva in plica ponunt.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

static int pone_claustrum(int desc, short genus)
{
    struct flock clausura;
    clausura.l_type   = genus;
    clausura.l_whence = SEEK_SET;
    clausura.l_start  = 0;
    clausura.l_len    = 0;
    return fcntl(desc, F_SETLK, &clausura);
}

int main(void)
{
    const char *via = "/tmp/custos_claustrum.tmp";

    int desc = open(via, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (desc == -1) {
        perror("open");
        return 1;
    }
    write(desc, "claustrum\n", 10);

    /* parens ponit claustrum scripturae */
    if (pone_claustrum(desc, F_WRLCK) == -1)
        perror("claustrum parentis");
    else
        printf("parens: claustrum scripturae positum\n");

    pid_t filius = fork();
    if (filius == -1) {
        perror("fork");
        close(desc);
        unlink(via);
        return 1;
    }

    if (filius == 0) {
        /* filius temptat claustrum ponere */
        if (pone_claustrum(desc, F_WRLCK) == -1)
            printf("  filius: claustrum denegatum (ut expectatum)\n");
        else
            printf("  filius: claustrum obtentum (inexpectatum!)\n");

        /* inspice quis tenet */
        struct flock info;
        info.l_type   = F_WRLCK;
        info.l_whence = SEEK_SET;
        info.l_start  = 0;
        info.l_len    = 0;
        if (fcntl(desc, F_GETLK, &info) == 0) {
            if (info.l_type != F_UNLCK)
                printf("  filius: claustrum tentum a parente\n");
        }
        close(desc);
        return 0;
    }

    waitpid(filius, NULL, 0);

    pone_claustrum(desc, F_UNLCK);
    printf("parens: claustrum liberatum\n");

    close(desc);
    unlink(via);
    printf("plica %s deleta\n", via);
    return 0;
}
