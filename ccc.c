/*
 * ccc.c — CCC compilator principale
 *
 * Functio main(), lectio plicarum, nuntii errorum.
 */

#include "ccc.h"
#include "biblio.h"
#include "parser.h"
#include "genera.h"
#include "liga.h"

#include <errno.h>

/* ================================================================
 * status globalis
 * ================================================================ */

char *fons_directorium = NULL;

/* ================================================================
 * errores
 * ================================================================ */

void erratum(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ccc: erratum: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

void erratum_ad(int linea, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "ccc:%d: erratum: ", linea);
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
            stderr, "ccc: non possum aperire '%s': %s\n",
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
        "usus: ccc [optiones] plica.c [...]\n"
        "\n"
        "optiones:\n"
        "  -o <plica>   plica exitus\n"
        "  -c           compila solum (sine ligatione)\n"
        "  -I <via>     adde viam inclusionis\n"
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

int main(int argc, char *argv[])
{
    const char *plicae[256];
    int num_plicarum         = 0;
    const char *plica_exitus = NULL;
    int modus_objecti        = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc)
                usus();
            plica_exitus = argv[i];
        } else if (strcmp(argv[i], "-c") == 0) {
            modus_objecti = 1;
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            /* -Ivia vel -I via */
            const char *via = argv[i][2] ? argv[i] + 2 : (++i < argc ? argv[i] : NULL);
            if (!via)
                usus();
            includ_adde(via);
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

    if (num_plicarum == 0)
        usus();

    /* si omnes plicae sunt .o → liga */
    int omnes_objecti = 1;
    for (int i = 0; i < num_plicarum; i++)
        if (!est_plica_objecti(plicae[i]))
            omnes_objecti = 0;

    if (omnes_objecti && !modus_objecti) {
        if (!plica_exitus)
            plica_exitus = "a.out";

        /* extrahe objecta ex archivis .a */
        int num_extractorum = 0;
        char **extractae    = biblio_extrahe_objecta(&num_extractorum);

        /* compone tabulam omnium obiectorum */
        int totum = num_plicarum + num_extractorum;
        const char **omnes = malloc(totum * sizeof(const char *));
        if (!omnes)
            erratum("memoria exhausta");
        for (int i = 0; i < num_plicarum; i++)
            omnes[i] = plicae[i];
        for (int i = 0; i < num_extractorum; i++)
            omnes[num_plicarum + i] = extractae[i];

        liga_objecta(totum, omnes, plica_exitus);

        free(omnes);
        if (extractae)
            biblio_purga_temporarias(extractae, num_extractorum);
        return 0;
    }

    /* compilatio .c */
    const char *plica_fontis = plicae[0];

    /* si -c sine -o, .c → .o */
    if (!plica_exitus) {
        if (modus_objecti) {
            static char auto_exitus[512];
            strncpy(auto_exitus, plica_fontis, 507);
            int lon = (int)strlen(auto_exitus);
            if (lon > 2 && auto_exitus[lon-2] == '.' && auto_exitus[lon-1] == 'c')
                auto_exitus[lon-1] = 'o';
            else {
                auto_exitus[lon]   = '.';
                auto_exitus[lon+1] = 'o';
                auto_exitus[lon+2] = '\0';
            }
            plica_exitus = auto_exitus;
        } else {
            plica_exitus = "a.out";
        }
    }

    /* lege fontem */
    int longitudo;
    char *fons       = lege_plicam(plica_fontis, &longitudo);
    fons_directorium = via_directoria(plica_fontis);

    /* initia lexatorem */
    lex_initia(plica_fontis, fons, longitudo);

    /* initia parserem et parse */
    parse_initia();
    nodus_t *radix = parse_translatio();

    /* genera codicem */
    genera_initia();

    /* si .a archiva adsunt et non -c, compila ad .o temporarium, deinde liga */
    int num_extractorum = 0;
    char **extractae    = biblio_extrahe_objecta(&num_extractorum);

    if (num_extractorum > 0 && !modus_objecti) {
        /* compila ad .o temporarium */
        const char *via_tmp = "/tmp/ccc_compilatum.o";
        genera_translatio(radix, via_tmp, 1);

        /* liga .o temporarium cum objectis ex .a */
        int totum = 1 + num_extractorum;
        const char **omnes = malloc(totum * sizeof(const char *));
        if (!omnes)
            erratum("memoria exhausta");
        omnes[0] = via_tmp;
        for (int i = 0; i < num_extractorum; i++)
            omnes[1 + i] = extractae[i];

        liga_objecta(totum, omnes, plica_exitus);

        remove(via_tmp);
        free(omnes);
        biblio_purga_temporarias(extractae, num_extractorum);
    } else {
        genera_translatio(radix, plica_exitus, modus_objecti);
        if (extractae)
            biblio_purga_temporarias(extractae, num_extractorum);
    }

    free(fons);
    free(fons_directorium);
    return 0;
}
