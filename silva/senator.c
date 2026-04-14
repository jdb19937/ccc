/*
 * senator.c — senator informationis systematis
 *
 * Demonstrat getpid(), getppid(), getuid(), getgid(),
 * uname(), getcwd(), getenv().
 * Informationes de processu et systemate colligit et monstrat.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>

#include <sys/utsname.h>
#include <unistd.h>

int main(void)
{
    printf("=== Informationes Processus ===\n");
    printf("PID:  (vivus)\n");
    printf("PPID: (vivus)\n");
    printf("UID:  %u\n", (unsigned)getuid());
    printf("GID:  %u\n", (unsigned)getgid());

    char directorium[1024];
    if (getcwd(directorium, sizeof(directorium)) != NULL)
        printf("CWD:  %s\n", directorium);

    printf("\n=== Informationes Systematis ===\n");
    struct utsname info;
    if (uname(&info) == 0) {
        printf("sysname:  %s\n", info.sysname);
        printf("nodename: %s\n", info.nodename);
        printf("release:  %s\n", info.release);
        printf("version:  %s\n", info.version);
        printf("machine:  %s\n", info.machine);
    }

    printf("\n=== Variabiles Ambitus ===\n");
    const char *nomina[] = {
        "HOME", "USER", "SHELL", "PATH", "LANG", NULL
    };
    for (int i = 0; nomina[i] != NULL; i++) {
        const char *valor = getenv(nomina[i]);
        if (valor)
            printf("%-8s %s\n", nomina[i], valor);
        else
            printf("%-8s (non definitum)\n", nomina[i]);
    }

    return 0;
}
