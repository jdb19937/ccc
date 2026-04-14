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


int optio_Wall     = 0;
int optio_Wextra   = 0;
int optio_pedantic = 0;
int optio_O        = 0;   /* gradus optimizationis (0, 1, 2, 3) */

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
        "  -Wall        activa monitiones omnes\n"
        "  -Wextra      activa monitiones extra\n"
        "  -pedantic    activa modum pedanticum\n"
        "  -std=c99     norma linguae (solum c99)\n"
        "  -O<gradus>   gradus optimizationis (0-3)\n"
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
        } else if (strcmp(argv[i], "-Wall") == 0) {
            optio_Wall = 1;
        } else if (strcmp(argv[i], "-Wextra") == 0) {
            optio_Wextra = 1;
        } else if (strcmp(argv[i], "-pedantic") == 0) {
            optio_pedantic = 1;
        } else if (strncmp(argv[i], "-std=", 5) == 0) {
            if (strcmp(argv[i] + 5, "c99") != 0)
                erratum("norma non sustenta: %s (solum -std=c99)", argv[i]);
        } else if (strncmp(argv[i], "-O", 2) == 0) {
            int g = argv[i][2] ? argv[i][2] - '0' : 2;
            if (g < 0 || g > 3)
                erratum("gradus optimizationis invalidus: %s", argv[i]);
            optio_O = g;
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

    /* si omnes plicae sunt .o vel .a → liga */
    int omnes_objecti = 1;
    for (int i = 0; i < num_plicarum; i++)
        if (!est_plica_objecti(plicae[i]) && !est_plica_archivi(plicae[i]))
            omnes_objecti = 0;

    if (omnes_objecti && !modus_objecti) {
        if (!plica_exitus)
            plica_exitus = "a.out";

        /* adde .a plicae ad biblio_res ut extrahantur */
        for (int i = 0; i < num_plicarum; i++)
            if (est_plica_archivi(plicae[i]))
                res_adde(plicae[i], BIBLIO_A);

        /* extrahe objecta ex archivis .a */
        int num_extractorum = 0;
        char **extractae    = biblio_extrahe_objecta(&num_extractorum);

        /* compone tabulam omnium obiectorum (.o solum) */
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

    /* numera plicas .c, .o, .a */
    int num_fontium = 0;
    int num_obj     = 0;
    for (int i = 0; i < num_plicarum; i++) {
        if (est_plica_objecti(plicae[i]))
            num_obj++;
        else if (!est_plica_archivi(plicae[i]))
            num_fontium++;
    }

    if (num_fontium == 0)
        erratum("nulla plica fontis inventa");

    /* si -c: compila quamque plicam .c ad .o */
    if (modus_objecti) {
        for (int i = 0; i < num_plicarum; i++) {
            if (est_plica_objecti(plicae[i]) || est_plica_archivi(plicae[i]))
                continue;
            const char *plica_fontis = plicae[i];

            char auto_exitus[512];
            if (plica_exitus && num_fontium == 1) {
                strncpy(auto_exitus, plica_exitus, 511);
            } else {
                strncpy(auto_exitus, plica_fontis, 507);
                int lon = (int)strlen(auto_exitus);
                if (lon > 2 && auto_exitus[lon-2] == '.' && auto_exitus[lon-1] == 'c')
                    auto_exitus[lon-1] = 'o';
                else {
                    auto_exitus[lon]   = '.';
                    auto_exitus[lon+1] = 'o';
                    auto_exitus[lon+2] = '\0';
                }
            }
            auto_exitus[511] = '\0';

            int longitudo;
            char *fons = lege_plicam(plica_fontis, &longitudo);
            lex_initia(plica_fontis, fons, longitudo);
            parse_initia();
            nodus_t *radix = parse_translatio();
            genera_initia();
            genera_translatio(radix, auto_exitus, 1);
            free(fons);
        }
        return 0;
    }

    /* compilatio ad executabile — compila quamque .c ad .o temporarium */
    if (!plica_exitus)
        plica_exitus = "a.out";

    /* adde .a plicae ad biblio_res */
    for (int i = 0; i < num_plicarum; i++)
        if (est_plica_archivi(plicae[i]))
            res_adde(plicae[i], BIBLIO_A);

    int num_extractorum = 0;
    char **extractae    = biblio_extrahe_objecta(&num_extractorum);

    /* si una sola plica .c et nullae .o nec .a → via simplex */
    if (num_fontium == 1 && num_obj == 0 && num_extractorum == 0) {
        const char *plica_fontis = plicae[0];
        int longitudo;
        char *fons = lege_plicam(plica_fontis, &longitudo);
        lex_initia(plica_fontis, fons, longitudo);
        parse_initia();
        nodus_t *radix = parse_translatio();
        genera_initia();
        genera_translatio(radix, plica_exitus, 0);
        free(fons);
        if (extractae)
            biblio_purga_temporarias(extractae, num_extractorum);
        return 0;
    }

    /* compila quamque .c ad .o temporarium, deinde liga omnia */
    char *viae_tmp[256];
    int num_tmp = 0;

    for (int i = 0; i < num_plicarum; i++) {
        if (est_plica_objecti(plicae[i]) || est_plica_archivi(plicae[i]))
            continue;
        char via_tmp[512];
        snprintf(via_tmp, 512, "/tmp/ccc_%d.o", num_tmp);

        int longitudo;
        char *fons = lege_plicam(plicae[i], &longitudo);
        lex_initia(plicae[i], fons, longitudo);
        parse_initia();
        nodus_t *radix = parse_translatio();
        genera_initia();
        genera_translatio(radix, via_tmp, 1);
        free(fons);

        viae_tmp[num_tmp] = strdup(via_tmp);
        num_tmp++;
    }

    /* compone tabulam omnium obiectorum */
    int totum = num_tmp + num_obj + num_extractorum;
    const char **omnes = malloc(totum * sizeof(const char *));
    if (!omnes)
        erratum("memoria exhausta");
    int oi = 0;
    for (int i = 0; i < num_tmp; i++)
        omnes[oi++] = viae_tmp[i];
    for (int i = 0; i < num_plicarum; i++)
        if (est_plica_objecti(plicae[i]))
            omnes[oi++] = plicae[i];
    for (int i = 0; i < num_extractorum; i++)
        omnes[oi++] = extractae[i];

    liga_objecta(totum, omnes, plica_exitus);

    /* purga temporaria */
    for (int i = 0; i < num_tmp; i++) {
        remove(viae_tmp[i]);
        free(viae_tmp[i]);
    }
    free(omnes);
    if (extractae)
        biblio_purga_temporarias(extractae, num_extractorum);
    return 0;
}
