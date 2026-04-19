/*
 * ccc0.c — CCC compilator veterrimus (pristinus, sine imm)
 *
 * Compilat plicam .c aut .i in plicam .o via genera.c, quod codicem
 * obiecti directe scribit (nullus transitus per .s + imm). Hic est
 * aedificator prior; ccc currens utitur generasym + imm.
 */

#include "utilia.h"
#include "parser.h"
#include "genera.h"

#include <errno.h>
#include <sys/wait.h>
#include <unistd.h>

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
        "usus: ccc0 [-o plica.o] [optiones] plica.c|plica.i\n"
        "\n"
        "optiones:\n"
        "  -c           (ignoratur)\n"
        "  -o <plica>   plica exitus (defalta: nomen.o)\n"
        "  -I <via>     adde viam inclusionis (transmissa ad iccc)\n"
        "  -S <via>     via capitum systematis (transmissa ad iccc)\n"
        "  -D<nomen>    defini macram (transmissa ad iccc)\n"
        "  -U<nomen>    exde macram (transmissa ad iccc)\n"
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

static int finit_in(const char *via, const char *suf)
{
    int lon = (int)strlen(via);
    int ls  = (int)strlen(suf);
    return lon >= ls && memcmp(via + lon - ls, suf, ls) == 0;
}

/* via ad executabile 'iccc': si argv[0] continet '/', adhibetur
 * dirname(argv0) + "/iccc"; alias redditur "iccc" pro quaestione PATH. */
static char *quaere_iccc(const char *argv0)
{
    const char *slash = strrchr(argv0, '/');
    if (!slash)
        return strdup("iccc");
    int dirlon = (int)(slash - argv0);
    char *via  = malloc(dirlon + 6);
    if (!via)
        erratum("memoria exhausta");
    memcpy(via, argv0, dirlon);
    memcpy(via + dirlon, "/iccc", 6);
    return via;
}

/* executa iccc ad .c praeprocessandum; plicam .i scribit */
static void exsecute_iccc(
    const char *iccc_via,
    const char *plica_c,
    const char *plica_i,
    char **iccc_args, int num_iccc_args
) {
    pid_t pid = fork();
    if (pid < 0)
        erratum("fork: %s", strerror(errno));
    if (pid == 0) {
        char **arg = malloc((num_iccc_args + 5) * sizeof(char *));
        if (!arg)
            _exit(127);
        int n = 0;
        arg[n++] = (char *)iccc_via;
        for (int i = 0; i < num_iccc_args; i++)
            arg[n++] = iccc_args[i];
        arg[n++] = "-o";
        arg[n++] = (char *)plica_i;
        arg[n++] = (char *)plica_c;
        arg[n]   = NULL;
        execvp(iccc_via, arg);
        fprintf(
            stderr, "ccc0: non possum exsecure '%s': %s\n",
            iccc_via, strerror(errno)
        );
        _exit(127);
    }
    int status;
    if (waitpid(pid, &status, 0) < 0)
        erratum("waitpid: %s", strerror(errno));
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        erratum("iccc defecit (status %d)", WEXITSTATUS(status));
}

int main(int argc, char *argv[])
{
    const char *plica_fontis = NULL;
    const char *plica_exitus = NULL;

    char **iccc_args     = malloc(argc * sizeof(char *));
    int    num_iccc_args = 0;
    if (!iccc_args)
        erratum("memoria exhausta");

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc)
                usus();
            plica_exitus = argv[i];
        } else if (strcmp(argv[i], "-c") == 0) {
            /* ignoratur — ccc0 semper obiectum generat */
        } else if (
            strncmp(argv[i], "-I", 2) == 0 ||
            strncmp(argv[i], "-S", 2) == 0 ||
            strncmp(argv[i], "-D", 2) == 0 ||
            strncmp(argv[i], "-U", 2) == 0
        ) {
            iccc_args[num_iccc_args++] = argv[i];
            if (argv[i][2] == '\0') {
                if (++i >= argc)
                    usus();
                iccc_args[num_iccc_args++] = argv[i];
            }
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
            if (plica_fontis)
                erratum("solum una plica fontis permissa");
            plica_fontis = argv[i];
        }
    }

    if (!plica_fontis)
        usus();

    int est_i = finit_in(plica_fontis, ".i");
    int est_c = finit_in(plica_fontis, ".c");
    if (!est_i && !est_c)
        erratum("plica '%s' non desinit in .c vel .i", plica_fontis);

    char auto_exitus[512];
    if (!plica_exitus) {
        strncpy(auto_exitus, plica_fontis, 507);
        auto_exitus[507] = '\0';
        int lon = (int)strlen(auto_exitus);
        auto_exitus[lon-1] = 'o';
        plica_exitus = auto_exitus;
    }

    char plica_i_tmp[64];
    const char *plica_lex;
    if (est_c) {
        snprintf(
            plica_i_tmp, sizeof(plica_i_tmp),
            "/tmp/ccc0.%d.i", (int)getpid()
        );
        plica_i_tmp_gl = plica_i_tmp;
        char *iccc_via = quaere_iccc(argv[0]);
        exsecute_iccc(iccc_via, plica_fontis, plica_i_tmp, iccc_args, num_iccc_args);
        free(iccc_via);
        plica_lex = plica_i_tmp;
    } else {
        plica_lex = plica_fontis;
    }
    free(iccc_args);

    int longitudo;
    char *fons = lege_plicam(plica_lex, &longitudo);
    lex_initia(plica_lex, fons, longitudo);
    parse_initia();
    nodus_t *radix = parse_translatio();
    genera_initia();
    genera_translatio(radix, plica_exitus, 1 /* modus_objecti */);
    free(fons);

    if (est_c) {
        remove(plica_i_tmp);
        plica_i_tmp_gl = NULL;
    }
    return 0;
}
