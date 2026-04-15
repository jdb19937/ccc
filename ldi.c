/*
 * ldi.c — ligator principale
 *
 * Ligat plicas .o et .a in executabile Mach-O ARM64.
 * Usus: ldi -o executabile plica1.o plica2.o [-L via] [-l nomen]
 */

#include "ccc.h"
#include "biblio.h"
#include "liga.h"

#include <errno.h>

/* ================================================================
 * status globalis
 * ================================================================ */

int optio_Wall     = 0;
int optio_Wextra   = 0;
int optio_pedantic = 0;
int optio_O        = 0;

/* ================================================================
 * errores
 * ================================================================ */

void erratum(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ldi: erratum: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void erratum_ad(int linea, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ldi:%d: erratum: ", linea);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

/* ================================================================
 * lectio plicae
 * ================================================================ */

char *lege_plicam(const char *via, int *longitudo)
{
    FILE *fp = fopen(via, "rb");
    if (!fp) {
        fprintf(
            stderr, "ldi: non possum aperire '%s': %s\n",
            via, strerror(errno)
        );
        exit(1);
    }

    fseek(fp, 0, SEEK_END);
    long mag = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *data = malloc(mag + 1);
    if (!data)
        erratum("memoria exhausta");
    fread(data, 1, mag, fp);
    data[mag] = '\0';
    fclose(fp);

    if (longitudo)
        *longitudo = (int)mag;
    return data;
}

char *via_directoria(const char *via)
{
    char *copia = strdup(via);
    char *ult   = strrchr(copia, '/');
    if (ult) {
        *(ult + 1) = '\0';
    } else {
        free(copia);
        copia = strdup("./");
    }
    return copia;
}

/* ================================================================
 * usus
 * ================================================================ */

static void usus(void)
{
    fprintf(
        stderr,
        "usus: ldi [-o executabile] [optiones] plica.o [...]\n"
        "\n"
        "optiones:\n"
        "  -o <plica>   plica exitus (defalta: prima plica sine .o/.a)\n"
        "  -L <via>     adde viam bibliothecarum\n"
        "  -l <nomen>   liga cum bibliotheca\n"
        "  -h, --help   monstra hunc nuntium\n"
    );
    exit(1);
}

/* ================================================================
 * principale
 * ================================================================ */

static int est_plica_objecti(const char *via)
{
    int lon = (int)strlen(via);
    return lon > 2 && via[lon-2] == '.' && via[lon-1] == 'o';
}

static int est_plica_archivi(const char *via)
{
    int lon = (int)strlen(via);
    return lon > 2 && via[lon-2] == '.' && via[lon-1] == 'a';
}

int main(int argc, char *argv[])
{
    const char *plicae[256];
    int num_plicarum         = 0;
    const char *plica_exitus = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc)
                usus();
            plica_exitus = argv[i];
        } else if (strncmp(argv[i], "-L", 2) == 0) {
            const char *via = argv[i][2] ? argv[i] + 2 : (++i < argc ? argv[i] : NULL);
            if (!via)
                usus();
            biblio_via_adde(via);
        } else if (strncmp(argv[i], "-l", 2) == 0) {
            const char *nomen = argv[i][2] ? argv[i] + 2 : (++i < argc ? argv[i] : NULL);
            if (!nomen)
                usus();
            biblio_adde(nomen);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usus();
        } else if (argv[i][0] == '-') {
            erratum("vexillum ignotum: %s", argv[i]);
        } else {
            if (num_plicarum < 256)
                plicae[num_plicarum++] = argv[i];
        }
    }

    if (num_plicarum == 0) {
        fprintf(stderr, "ldi: nulla plica data\n");
        usus();
    }

    /* valida: omnes plicae debent esse .o vel .a */
    for (int i = 0; i < num_plicarum; i++)
        if (!est_plica_objecti(plicae[i]) && !est_plica_archivi(plicae[i]))
            erratum("plica '%s' non est .o neque .a", plicae[i]);

    if (!plica_exitus) {
        /* exue extensionem .o vel .a ex prima plica */
        int lon       = (int)strlen(plicae[0]);
        char *defalta = malloc(lon - 1);
        if (!defalta)
            erratum("memoria exhausta");
        memcpy(defalta, plicae[0], lon - 2);
        defalta[lon - 2] = '\0';
        plica_exitus     = defalta;
    }

    /* adde .a plicae ad biblio_res ut extrahantur */
    for (int i = 0; i < num_plicarum; i++)
        if (est_plica_archivi(plicae[i]))
            res_adde(plicae[i], BIBLIO_A);

    /* extrahe objecta ex archivis .a */
    int num_extractorum = 0;
    char **extractae    = biblio_extrahe_objecta(&num_extractorum);

    /* compone tabulam omnium obiectorum */
    int num_obj = 0;
    for (int i = 0; i < num_plicarum; i++)
        if (est_plica_objecti(plicae[i]))
            num_obj++;
    int totum = num_obj + num_extractorum;
    const char **omnes = malloc(totum * sizeof(const char *));
    if (!omnes)
        erratum("memoria exhausta");
    int oi = 0;
    for (int i = 0; i < num_plicarum; i++)
        if (est_plica_objecti(plicae[i]))
            omnes[oi++] = plicae[i];
    for (int i = 0; i < num_extractorum; i++)
        omnes[oi + i] = extractae[i];

    liga_objecta(totum, omnes, plica_exitus);

    free(omnes);
    if (extractae)
        biblio_purga_temporarias(extractae, num_extractorum);
    return 0;
}
