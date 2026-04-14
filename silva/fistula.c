/*
 * fistula.c — fistula nominata (FIFO)
 *
 * Demonstrat mkfifo(), fork(), open(), read(), write(), unlink().
 * Per fistulam nominatam inter parentem et filium communicat.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *via_fistulae = "/tmp/fistula_nominata";

int main(void)
{
    /* purga si restat ex cursione priore */
    unlink(via_fistulae);

    if (mkfifo(via_fistulae, 0644) == -1) {
        perror("mkfifo");
        return 1;
    }

    pid_t filius = fork();
    if (filius == -1) {
        perror("fork");
        unlink(via_fistulae);
        return 1;
    }

    if (filius == 0) {
        /* filius legit ex fistula */
        int desc = open(via_fistulae, O_RDONLY);
        if (desc == -1) {
            perror("open (lectura)");
            return 1;
        }
        char alveus[256];
        ssize_t num_lecta = read(desc, alveus, sizeof(alveus) - 1);
        if (num_lecta > 0) {
            alveus[num_lecta] = '\0';
            printf("filius ex fistula: %s\n", alveus);
        }
        close(desc);
        return 0;
    }

    /* parens scribit in fistulam */
    int desc = open(via_fistulae, O_WRONLY);
    if (desc == -1) {
        perror("open (scriptura)");
        unlink(via_fistulae);
        return 1;
    }
    const char *nuntius = "Veni, vidi, vici.";
    write(desc, nuntius, strlen(nuntius));
    close(desc);

    waitpid(filius, NULL, 0);
    unlink(via_fistulae);
    printf("fistula %s deleta\n", via_fistulae);
    return 0;
}
