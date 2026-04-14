/*
 * catena.c — catena filorum (threads)
 *
 * Demonstrat pthread_create(), pthread_join(), pthread_mutex.
 * Plures fili numeratorem communem per mutex augent.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>

#include <pthread.h>

#define NUMERUS_FILORUM 4
#define ITERATIONES     100000

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static long numerator        = 0;

static void *opus_fili(void *argumentum)
{
    (void)argumentum;
    for (int i = 0; i < ITERATIONES; i++) {
        pthread_mutex_lock(&mutex);
        numerator++;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(void)
{
    pthread_t fila[NUMERUS_FILORUM];
    int indices[NUMERUS_FILORUM];

    printf(
        "catena: creo %d fila, singula %d iterationum\n",
        NUMERUS_FILORUM, ITERATIONES
    );

    for (int i = 0; i < NUMERUS_FILORUM; i++) {
        indices[i] = i;
        int err = pthread_create(
            &fila[i], NULL, opus_fili, &indices[i]
        );
        if (err != 0) {
            fprintf(stderr, "pthread_create: erratum %d\n", err);
            return 1;
        }
    }

    for (int i = 0; i < NUMERUS_FILORUM; i++)
        pthread_join(fila[i], NULL);

    long expectatum = (long)NUMERUS_FILORUM * ITERATIONES;
    printf(
        "numerator: %ld (expectatum: %ld)\n",
        numerator, expectatum
    );
    printf(
        "catena: %s\n",
        numerator == expectatum ? "rectum" : "erratum"
    );
    return 0;
}
