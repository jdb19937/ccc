/*
 * ccc.c — CCC compilator principale
 *
 * Compilat unam plicam .c in plicam .o.
 * Functio main(), lectio plicarum.
 */

#include "utilia.h"
#include "biblio.h"
#include "parser.h"
#include "genera.h"

/* ================================================================
 * status globalis
 * ================================================================ */

int optio_Wall     = 0;
int optio_Wextra   = 0;
int optio_pedantic = 0;
int optio_O        = 0;   /* gradus optimizationis (0, 1, 2, 3) */

/* ================================================================
 * usus
 * ================================================================ */

static void usus(void)
{
    fprintf(
        stderr,
        "usus: ccc [-o plica.o] [optiones] plica.c\n"
        "\n"
        "optiones:\n"
        "  -c           (ignoratur)\n"
        "  -o <plica>   plica exitus (defalta: nomen.o)\n"
        "  -I <via>     adde viam inclusionis\n"
        "  -S <via>     via capitum systematis\n"
        "               (defalta: /opt/apotheca/var/ccc/capita)\n"
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

static int est_plica_fontis(const char *via)
{
    int lon = (int)strlen(via);
    return lon > 2 && via[lon-2] == '.' && via[lon-1] == 'c';
}

int main(int argc, char *argv[])
{
    const char *plica_fontis = NULL;
    const char *plica_exitus = NULL;
    const char *via_capitum  = "/opt/apotheca/var/ccc/capita";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc)
                usus();
            plica_exitus = argv[i];
        } else if (strcmp(argv[i], "-c") == 0) {
            /* ignoratur — ccc semper obiectum generat */
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            const char *via = argv[i][2] ? argv[i] + 2 : (++i < argc ? argv[i] : NULL);
            if (!via)
                usus();
            includ_adde(via);
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
        } else if (strncmp(argv[i], "-S", 2) == 0) {
            const char *via = argv[i][2] ? argv[i] + 2 : (++i < argc ? argv[i] : NULL);
            if (!via)
                usus();
            via_capitum = via;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usus();
        } else if (argv[i][0] == '-') {
            erratum("vexillum ignotum: %s", argv[i]);
        } else {
            if (plica_fontis)
                erratum("solum una plica fontis permissa");
            plica_fontis = argv[i];
        }
    }

    if (!plica_fontis)
        usus();

    if (!est_plica_fontis(plica_fontis))
        erratum("plica '%s' non desinit in .c", plica_fontis);

    /* computa nomen exitus si non datum */
    char auto_exitus[512];
    if (!plica_exitus) {
        strncpy(auto_exitus, plica_fontis, 507);
        auto_exitus[507] = '\0';
        int lon = (int)strlen(auto_exitus);
        auto_exitus[lon-1] = 'o';
        plica_exitus = auto_exitus;
    }

    /* adde viam capitum systematis (post vias -I) */
    includ_adde(via_capitum);

    int longitudo;
    char *fons = lege_plicam(plica_fontis, &longitudo);
    lex_initia(plica_fontis, fons, longitudo);
    parse_initia();
    nodus_t *radix = parse_translatio();
    genera_initia();
    genera_translatio(radix, plica_exitus, 1);
    free(fons);
    return 0;
}
