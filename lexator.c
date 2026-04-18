/*
 * lexator.c — CCC lexator et praeprocessor
 *
 * Lexator C99 cum praeprocessore simplici.
 * Capita interna pro functionibus systematis (stdio, stdlib,
 * termios, etc.) inclusa sunt.
 */

#include "utilia.h"
#include "lexator.h"
#include "biblio.h"

/* ================================================================
 * vocabula clavis
 * ================================================================ */

static struct {
    const char *nomen;
    int genus;
} vocabula[] = {
    {"auto",      T_AUTO},     {"break",    T_BREAK},
    {"case",      T_CASE},     {"char",     T_CHAR},
    {"const",     T_CONST},    {"continue", T_CONTINUE},
    {"default",   T_DEFAULT},  {"do",       T_DO},
    {"double",    T_DOUBLE},   {"else",     T_ELSE},
    {"enum",      T_ENUM},     {"extern",   T_EXTERN},
    {"float",     T_FLOAT},    {"for",      T_FOR},
    {"goto",      T_GOTO},     {"if",       T_IF},
    {"int",       T_INT},      {"long",     T_LONG},
    {"register",  T_REGISTER}, {"return",   T_RETURN},
    {"short",     T_SHORT},    {"signed",   T_SIGNED},
    {"sizeof",    T_SIZEOF},   {"static",   T_STATIC},
    {"struct",    T_STRUCT},   {"switch",   T_SWITCH},
    {"typedef",   T_TYPEDEF},  {"union",    T_UNION},
    {"unsigned",  T_UNSIGNED}, {"void",     T_VOID},
    {"volatile",  T_VOLATILE}, {"while",    T_WHILE},
    /* §6.7.4: inline — indicium optimizationis */
    {"inline",    T_INLINE},
    /* §6.2.5: _Bool — typus Booleanus */
    {"_Bool",     T_BOOL},
    /* §6.7.3.1: restrict — qualificator indicis */
    {"restrict",  T_RESTRICT},
    {NULL, 0}
};

/* ================================================================
 * macrae (praeparatore)
 * ================================================================ */

typedef struct {
    char nomen[256];
    char valor[MAX_CHORDA];
    int activa;
    int est_functionalis;       /* 1 si macra functionalis */
    int est_variadica;          /* §6.10.3: 1 sī ... in parametrīs */
    int num_parametrorum;
    char parametri[MAX_PARAM_MACRAE][MAX_NOMEN_PARAM];
} macra_t;

static macra_t macrae[MAX_MACRAE];
static int num_macrarum = 0;

static macra_t *macra_quaere(const char *nomen)
{
    for (int i = 0; i < num_macrarum; i++)
        if (macrae[i].activa && strcmp(macrae[i].nomen, nomen) == 0)
            return &macrae[i];
    return NULL;
}

static macra_t *macra_defini(const char *nomen, const char *valor)
{
    macra_t *m = macra_quaere(nomen);
    if (m) {
        strncpy(m->valor, valor, MAX_CHORDA - 1);
        return m;
    }
    if (num_macrarum >= MAX_MACRAE)
        erratum("nimis multae macrae");
    m = &macrae[num_macrarum++];
    strncpy(m->nomen, nomen, 255);
    strncpy(m->valor, valor, MAX_CHORDA - 1);
    m->activa = 1;
    m->est_functionalis = 0;
    m->num_parametrorum = 0;
    return m;
}

static void macra_dele(const char *nomen)
{
    macra_t *m = macra_quaere(nomen);
    if (m)
        m->activa = 0;
}

/* ================================================================
 * typedef nomina (pro disambiguatione in parsore)
 * ================================================================ */

static char typedef_nomina[4096][256];
static int  num_typedef_nominum = 0;

int lex_est_typus(const char *nomen)
{
    for (int i = 0; i < num_typedef_nominum; i++)
        if (strcmp(typedef_nomina[i], nomen) == 0)
            return 1;
    return 0;
}

void lex_registra_typedef(const char *nomen)
{
    if (num_typedef_nominum >= 4096)
        erratum("nimis multa typedef nomina");
    strncpy(typedef_nomina[num_typedef_nominum++], nomen, 255);
}

/* ================================================================
 * acervus plicarum (pro #include)
 * ================================================================ */

typedef struct {
    char *nomen;
    const char *fons;
    int longitudo;
    int positus;
    int linea;
} plica_ctx_t;

static plica_ctx_t acervus_plicarum[MAX_PLICAE_ACERVUS];
static int vertex_plicarum = 0;

/* contextus currens */
static char *cur_nomen;
static const char *cur_fons;
static int cur_longitudo;
static int cur_positus;
static int cur_linea;

/* ================================================================
 * acervus expansionis macrorum
 * ================================================================ */

typedef struct {
    const char *fons;
    int longitudo;
    int positus;
} expansio_ctx_t;

static expansio_ctx_t acervus_expansionum[64];
static int vertex_expansionum = 0;

/* ================================================================
 * signum currens (exportatum)
 * ================================================================ */

signum_t sig;
int sig_linea;

/* ================================================================
 * characteres
 * ================================================================ */

static int lege_c(void)
{
    /* primum ex expansione macrorum */
    while (vertex_expansionum > 0) {
        expansio_ctx_t *e = &acervus_expansionum[vertex_expansionum - 1];
        if (e->positus < e->longitudo)
            return (unsigned char)e->fons[e->positus++];
        vertex_expansionum--;
    }

    for (;;) {
        if (cur_positus < cur_longitudo) {
            char c = cur_fons[cur_positus++];
            if (c == '\n')
                cur_linea++;
            /* continuatio lineae: \ ante novam lineam */
            if (c == '\\' && cur_positus < cur_longitudo && cur_fons[cur_positus] == '\n') {
                cur_positus++;
                cur_linea++;
                continue;
            }
            return (unsigned char)c;
        }
        /* fine plicae currentis — restitue ex acervo */
        if (vertex_plicarum <= 0)
            return -1;
        vertex_plicarum--;
        plica_ctx_t *p = &acervus_plicarum[vertex_plicarum];
        free(cur_nomen);
        cur_nomen     = p->nomen;
        cur_fons      = p->fons;
        cur_longitudo = p->longitudo;
        cur_positus   = p->positus;
        cur_linea     = p->linea;
    }
}

static void repone_c(void)
{
    /* repone unum characterem */
    if (vertex_expansionum > 0) {
        acervus_expansionum[vertex_expansionum - 1].positus--;
        return;
    }
    if (cur_positus > 0) {
        cur_positus--;
        if (cur_positus < cur_longitudo && cur_fons[cur_positus] == '\n')
            cur_linea--;
    }
}

static void pelle_expansionem(const char *fons, int lon)
{
    if (vertex_expansionum >= 64)
        erratum("expansio macrorum nimis profunda");
    expansio_ctx_t *e = &acervus_expansionum[vertex_expansionum++];
    e->fons = fons;
    e->longitudo = lon;
    e->positus = 0;
}

/* ================================================================
 * praetermissio spatiorum et commentariorum
 * ================================================================ */

static void praetermitte_spatia(void)
{
    for (;;) {
        int c = lege_c();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v')
            continue;
        if (c == '/') {
            int c2 = lege_c();
            if (c2 == '/') {
                /* commentarium lineae */
                while ((c = lege_c()) != -1 && c != '\n') {}
                continue;
            } else if (c2 == '*') {
                /* commentarium blocki */
                int prec = 0;
                while ((c = lege_c()) != -1) {
                    if (prec == '*' && c == '/')
                        break;
                    prec = c;
                }
                continue;
            }
            repone_c();
        }
        if (c != -1)
            repone_c();
        return;
    }
}

/* ================================================================
 * conditiones praeprocessoris (#if/#ifdef acervus)
 * ================================================================ */

static int cond_acervus[64];
static int cond_vertex = 0;
/* 1 = emittimus, 0 = suppressi, 2 = in ramo else suppressi */

static int cond_activa(void)
{
    for (int i = 0; i < cond_vertex; i++)
        if (cond_acervus[i] != 1)
            return 0;
    return 1;
}

/* ================================================================
 * praeprocessor — directiva
 * ================================================================ */

static void lege_nomen_pp(char *alveus, int max)
{
    int i = 0;
    int c;
    praetermitte_spatia();
    while ((c = lege_c()) != -1) {
        if (
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_'
        ) {
            if (i < max - 1)
                alveus[i++] = c;
        } else {
            repone_c();
            break;
        }
    }
    alveus[i] = '\0';
}

static void lege_residuum_lineae(char *alveus, int max)
{
    int i = 0;
    int c;
    /* praetermitte spatia initialia */
    while ((c = lege_c()) != -1 && (c == ' ' || c == '\t')) {}
    if (c != -1 && c != '\n') {
        alveus[i++] = c;
        while ((c = lege_c()) != -1 && c != '\n') {
            if (i < max - 1)
                alveus[i++] = c;
        }
    }
    /* tolle spatia terminalia */
    while (i > 0 && (alveus[i-1] == ' ' || alveus[i-1] == '\t' || alveus[i-1] == '\r'))
        i--;
    alveus[i] = '\0';
}

static void praetermitte_lineam(void)
{
    int c;
    while ((c = lege_c()) != -1 && c != '\n') {}
}

static void pelle_plicam(char *nomen, const char *fons, int lon)
{
    if (vertex_plicarum >= MAX_PLICAE_ACERVUS)
        erratum("nimis multae plicae inclusae");
    plica_ctx_t *p = &acervus_plicarum[vertex_plicarum++];
    p->nomen       = cur_nomen;
    p->fons        = cur_fons;
    p->longitudo   = cur_longitudo;
    p->positus     = cur_positus;
    p->linea       = cur_linea;

    cur_nomen     = nomen;
    cur_fons      = fons;
    cur_longitudo = lon;
    cur_positus   = 0;
    cur_linea     = 1;
}

static int tracta_directivam(void)
{
    /* iamiam post '#' */
    char directiva[64];
    lege_nomen_pp(directiva, sizeof(directiva));

    if (strcmp(directiva, "include") == 0) {
        if (!cond_activa()) {
            praetermitte_lineam();
            return 1;
        }
        praetermitte_spatia();
        int c = lege_c();
        char via[512];
        int i = 0;
        if (c == '"') {
            while ((c = lege_c()) != -1 && c != '"' && c != '\n')
                if (i < 511)
                    via[i++] = c;
            via[i] = '\0';
            praetermitte_lineam();

            /* proba plicam localem (relativa ad plicam currentem) */
            char *cur_dir = via_directoria(cur_nomen);
            char via_plena[1024];
            snprintf(via_plena, sizeof(via_plena), "%s%s", cur_dir, via);
            free(cur_dir);
            FILE *fp_proba = fopen(via_plena, "rb");
            if (fp_proba) {
                fclose(fp_proba);
                int lon;
                char *fons = lege_plicam(via_plena, &lon);
                pelle_plicam(strdup(via_plena), fons, lon);
            } else {
                /* quaere per vias -I */
                int lon;
                char via_i[1024];
                char *fons = includ_quaere(via, &lon, via_i, sizeof(via_i));
                if (fons)
                    pelle_plicam(strdup(via_i), fons, lon);
                else
                    erratum("caput non inventum: \"%s\"", via);
            }
        } else if (c == '<') {
            while ((c = lege_c()) != -1 && c != '>' && c != '\n')
                if (i < 511)
                    via[i++] = c;
            via[i] = '\0';
            praetermitte_lineam();

            /* proba vias -I primum */
            int lon_i;
            char via_i[1024];
            char *fons_i = includ_quaere(via, &lon_i, via_i, sizeof(via_i));
            if (fons_i) {
                pelle_plicam(strdup(via_i), fons_i, lon_i);
            } else {
                erratum("caput non inventum: <%s>", via);
            }
        }
        return 1;
    }

    if (strcmp(directiva, "define") == 0) {
        if (!cond_activa()) {
            praetermitte_lineam();
            return 1;
        }
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        /* proba si macra functionalis: '(' sine spatio post nomen */
        int c_peek = lege_c();
        if (c_peek == '(') {
            /* macra functionalis — lege parametros */
            char param_nomina[MAX_PARAM_MACRAE][MAX_NOMEN_PARAM];
            int nparam = 0;
            for (;;) {
                char pn[128];
                int pi = 0;
                /* praetermitte spatia */
                int pc;
                while ((pc = lege_c()) == ' ' || pc == '\t') {}
                if (pc == ')')
                    break;
                if (pc == ',' )
                    continue;
                /* lege nomen parametri */
                pn[pi++] = pc;
                while ((pc = lege_c()) != -1 && pc != ',' && pc != ')' && pc != ' ' && pc != '\t') {
                    if (pi < 127)
                        pn[pi++] = pc;
                }
                pn[pi] = '\0';
                if (nparam >= MAX_PARAM_MACRAE)
                    erratum_ad(
                        cur_linea,
                        "nimis multi parametri in macra"
                    );
                strncpy(param_nomina[nparam++], pn, MAX_NOMEN_PARAM - 1);
                if (pc == ')')
                    break;
            }
            /* §6.10.3: dēlige ... ex ultimō parametrō */
            int est_var = 0;
            if (
                nparam > 0
                && strcmp(param_nomina[nparam - 1], "...") == 0
            ) {
                nparam--;
                est_var = 1;
            }
            char valor[MAX_CHORDA];
            lege_residuum_lineae(valor, sizeof(valor));
            macra_t *m = macra_defini(nomen, valor);
            m->est_functionalis = 1;
            m->est_variadica    = est_var;
            m->num_parametrorum = nparam;
            for (int i = 0; i < nparam; i++)
                strncpy(m->parametri[i], param_nomina[i], MAX_NOMEN_PARAM - 1);
        } else {
            repone_c();
            char valor[MAX_CHORDA];
            lege_residuum_lineae(valor, sizeof(valor));
            macra_defini(nomen, valor);
        }
        return 1;
    }

    if (strcmp(directiva, "undef") == 0) {
        if (!cond_activa()) {
            praetermitte_lineam();
            return 1;
        }
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        macra_dele(nomen);
        praetermitte_lineam();
        return 1;
    }

    if (strcmp(directiva, "ifdef") == 0) {
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        praetermitte_lineam();
        if (cond_vertex >= 64)
            erratum("#ifdef nimis profundum");
        if (!cond_activa()) {
            cond_acervus[cond_vertex++] = 0;
        } else {
            cond_acervus[cond_vertex++] = macra_quaere(nomen) ? 1 : 0;
        }
        return 1;
    }

    if (strcmp(directiva, "ifndef") == 0) {
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        praetermitte_lineam();
        if (cond_vertex >= 64)
            erratum("#ifndef nimis profundum");
        if (!cond_activa()) {
            cond_acervus[cond_vertex++] = 0;
        } else {
            cond_acervus[cond_vertex++] = macra_quaere(nomen) ? 0 : 1;
        }
        return 1;
    }

    if (strcmp(directiva, "if") == 0) {
        if (cond_vertex >= 64)
            erratum("#if nimis profundum");
        if (!cond_activa()) {
            cond_acervus[cond_vertex++] = 0;
            praetermitte_lineam();
            return 1;
        }
        /* evaluatio simplex: supportamus '0', '1', 'defined(X)' */
        char expr[MAX_CHORDA];
        lege_residuum_lineae(expr, sizeof(expr));
        /* §6.10.1: macrae expanduntur ante evaluationem #if */
        {
            char expandata[MAX_CHORDA];
            int ei = 0;
            for (int ci = 0; expr[ci] && ei < MAX_CHORDA - 1; ) {
                if (
                    (expr[ci] >= 'a' && expr[ci] <= 'z') ||
                    (expr[ci] >= 'A' && expr[ci] <= 'Z') || expr[ci] == '_'
                ) {
                    char tok[256];
                    int ti = 0;
                    while (
                        expr[ci] && (
                            (expr[ci] >= 'a' && expr[ci] <= 'z') ||
                            (expr[ci] >= 'A' && expr[ci] <= 'Z') ||
                            (expr[ci] >= '0' && expr[ci] <= '9') || expr[ci] == '_'
                        )
                    )
                        if (ti < 255)
                            tok[ti++] = expr[ci++];
                    else
                        ci++;
                    tok[ti]         = '\0';
                    macra_t *m      = macra_quaere(tok);
                    const char *rep = (m && !m->est_functionalis) ? m->valor : tok;
                    while (*rep && ei < MAX_CHORDA - 1)
                        expandata[ei++] = *rep++;
                } else {
                    expandata[ei++] = expr[ci++];
                }
            }
            expandata[ei] = '\0';
            strncpy(expr, expandata, MAX_CHORDA - 1);
        }
        int val = 0;
        if (strstr(expr, "defined")) {
            /* extrahre nomen */
            char *p = strstr(expr, "defined");
            p += 7;
            while (*p == ' ' || *p == '(')
                p++;
            char nomen[256];
            int i = 0;
            while (*p && *p != ')' && *p != ' ' && i < 255)
                nomen[i++] = *p++;
            nomen[i] = '\0';
            val      = macra_quaere(nomen) ? 1 : 0;
        } else {
            /* evaluatio simplex expressionum: N > M, N == M, etc. */
            char *gt = strstr(expr, ">=");
            char *lt = strstr(expr, "<=");
            char *eq = strstr(expr, "==");
            char *ne = strstr(expr, "!=");
            char *g  = !gt ? strchr(expr, '>') : NULL;
            char *l  = !lt ? strchr(expr, '<') : NULL;
            if (eq)
                val = atoi(expr) == atoi(eq + 2);
            else if (ne)
                val = atoi(expr) != atoi(ne + 2);
            else if (gt)
                val = atoi(expr) >= atoi(gt + 2);
            else if (lt)
                val = atoi(expr) <= atoi(lt + 2);
            else if (g)
                val = atoi(expr) >  atoi(g + 1);
            else if (l)
                val = atoi(expr) <  atoi(l + 1);
            else
                val = atoi(expr);
        }
        cond_acervus[cond_vertex++] = val ? 1 : 0;
        return 1;
    }

    if (strcmp(directiva, "elif") == 0) {
        praetermitte_lineam();
        if (cond_vertex <= 0)
            erratum("#elif sine #if");
        if (cond_acervus[cond_vertex - 1] == 1) {
            cond_acervus[cond_vertex - 1] = 2; /* iam satisfactum */
        } else if (cond_acervus[cond_vertex - 1] == 0) {
            /* proba novam conditionem — simpliciter ponamus 0 */
            cond_acervus[cond_vertex - 1] = 0;
        }
        return 1;
    }

    if (strcmp(directiva, "else") == 0) {
        praetermitte_lineam();
        if (cond_vertex <= 0)
            erratum("#else sine #if");
        if (cond_acervus[cond_vertex - 1] == 1)
            cond_acervus[cond_vertex - 1] = 2;
        else if (cond_acervus[cond_vertex - 1] == 0)
            cond_acervus[cond_vertex - 1] = 1;
        return 1;
    }

    if (strcmp(directiva, "endif") == 0) {
        praetermitte_lineam();
        if (cond_vertex <= 0)
            erratum("#endif sine #if");
        cond_vertex--;
        return 1;
    }

    /* §6.10.5: #error — nuntium diagnosticum et cessatio */
    if (strcmp(directiva, "error") == 0) {
        if (cond_activa()) {
            char nuntium[MAX_CHORDA];
            lege_residuum_lineae(nuntium, sizeof(nuntium));
            erratum_ad(cur_linea, "#error %s", nuntium);
        }
        praetermitte_lineam();
        return 1;
    }

    if (
        strcmp(directiva, "pragma") == 0 ||
        strcmp(directiva, "warning") == 0 ||
        strcmp(directiva, "line") == 0 ||
        directiva[0] == '\0'
    ) {
        praetermitte_lineam();
        return 1;
    }

    /* §6.10: directiva ignota */
    if (cond_activa())
        erratum_ad(cur_linea, "directiva ignota: #%s", directiva);
    praetermitte_lineam();
    return 1;
}

/* ================================================================
 * character effugiens in chordis
 * ================================================================ */

static int lege_effugium(void)
{
    int c = lege_c();
    switch (c) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"':  return '"';
    case '0':  return '\0';
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'f':  return '\f';
    case 'v':  return '\v';
    case 'x': {
            int val = 0;
            for (int i = 0; i < 2; i++) {
                c = lege_c();
                if (c >= '0' && c <= '9')
                    val = val * 16 + (c - '0');
                else if (c >= 'a' && c <= 'f')
                    val = val * 16 + (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    val = val * 16 + (c - 'A' + 10);
                else {
                    repone_c();
                    break;
                }
            }
            return val;
        }
    default:
        if (c >= '0' && c <= '7') {
            int val = c - '0';
            for (int i = 0; i < 2; i++) {
                c = lege_c();
                if (c >= '0' && c <= '7')
                    val = val * 8 + (c - '0');
                else {
                    repone_c();
                    break;
                }
            }
            return val;
        }
        return c;
    }
}

/* ================================================================
 * signum spectans (peek) — declarationes ante lexatorem
 * ================================================================ */

static signum_t sig_spectans;
static int habet_spectantem = 0;
static const char *plica_spectans = NULL;

/* ================================================================
 * lexator principalis
 * ================================================================ */

static int lege_signum_internum(void)
{
inicio:
    praetermitte_spatia();
    sig.linea = cur_linea;
    sig_linea = cur_linea;
    plica_currentis = cur_nomen;

    int c = lege_c();
    if (c == -1) {
        sig.genus = T_EOF;
        return T_EOF;
    }

    /* nova linea — potentialiter directiva */
    if (c == '\n')
        goto inicio;

    /* directiva praeprocessoris */
    if (c == '#') {
        if (tracta_directivam())
            goto inicio;
        sig.genus = T_HASH;
        return T_HASH;
    }

    /* lineas suppressas ignora */
    if (!cond_activa()) {
        /* praetermitte usque ad novam lineam */
        while (c != '\n' && c != -1)
            c = lege_c();
        goto inicio;
    }

    /* numerus — §6.4.4.1 (integer), §6.4.4.2 (fluitans) */
    if (c >= '0' && c <= '9') {
        /* collige characteres numeri in alveo pro strtod si fluitans */
        char num_buf[256];
        int num_len      = 0;
        int est_fluitans = 0;

        long val = 0;
        num_buf[num_len++] = (char)c;
        if (c == '0') {
            c = lege_c();
            if (c == 'x' || c == 'X') {
                num_buf[num_len++] = (char)c;
                while ((c = lege_c()) != -1) {
                    if (c >= '0' && c <= '9')
                        val = val * 16 + (c - '0');
                    else if (c >= 'a' && c <= 'f')
                        val = val * 16 + (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F')
                        val = val * 16 + (c - 'A' + 10);
                    else
                        break;
                    num_buf[num_len++] = (char)c;
                }
            } else if (c >= '0' && c <= '7' && c != '.') {
                val = c - '0';
                num_buf[num_len++] = (char)c;
                while ((c = lege_c()) != -1 && c >= '0' && c <= '7') {
                    val = val * 8 + (c - '0');
                    num_buf[num_len++] = (char)c;
                }
            } else {
                /* erat solum 0 (vel 0. sequitur) */
            }
        } else {
            val = c - '0';
            while ((c = lege_c()) != -1 && c >= '0' && c <= '9') {
                val = val * 10 + (c - '0');
                num_buf[num_len++] = (char)c;
            }
        }
        /* §6.4.4.2: punctum decimale → constans fluitans */
        if (c == '.') {
            est_fluitans       = 1;
            num_buf[num_len++] = '.';
            while ((c = lege_c()) != -1 && c >= '0' && c <= '9')
                num_buf[num_len++] = (char)c;
        }
        /* §6.4.4.2: exponent e/E → constans fluitans */
        if (c == 'e' || c == 'E') {
            est_fluitans = 1;
            num_buf[num_len++] = (char)c;
            c = lege_c();
            if (c == '+' || c == '-') {
                num_buf[num_len++] = (char)c;
                c = lege_c();
            }
            while (c >= '0' && c <= '9') {
                num_buf[num_len++] = (char)c;
                c = lege_c();
            }
        }
        /* suffixum — §6.4.4.1: U, L pro integris; §6.4.4.2: f, F, l, L pro fluitantibus */
        if (est_fluitans) {
            if (c == 'f' || c == 'F') {
                c = lege_c(); /* suffixum float — consumitur */
            } else if (c == 'l' || c == 'L') {
                c = lege_c(); /* suffixum long double — tractatur ut double */
            }
        } else {
            while (
                c == 'u' || c == 'U' || c == 'l' || c == 'L' ||
                c == 'f' || c == 'F'
            ) {
                if (c == 'f' || c == 'F')
                    est_fluitans = 1;
                c = lege_c();
            }
        }
        if (c != -1)
            repone_c();

        if (est_fluitans) {
            /* §6.4.4.2: converte per strtod */
            num_buf[num_len] = '\0';
            sig.genus        = T_NUM_FLUAT;
            sig.valor_f      = strtod(num_buf, NULL);
            sig.valor        = 0;
            return T_NUM_FLUAT;
        }
        sig.genus = T_NUM;
        sig.valor = val;
        return T_NUM;
    }

    /* chorda — §6.4.5 */
    if (c == '"') {
        int i = 0;
        while ((c = lege_c()) != -1 && c != '"') {
            if (c == '\\')
                c = lege_effugium();
            if (i >= MAX_CHORDA - 1)
                erratum_ad(
                    cur_linea,
                    "chorda litteralis nimis longa (max %d)", MAX_CHORDA - 1
                );
            sig.chorda[i++] = c;
        }
        sig.chorda[i]   = '\0';
        sig.lon_chordae = i;
        sig.genus       = T_STR;

        /* §6.4.5p4: concatenatio chordarum litteralium adiacentium
         * — etiam per macrōs expandendōs */
        for (;;) {
            praetermitte_spatia();
            c = lege_c();
            /* praetermitte novas lineas inter chordas */
            while (c == '\n') {
                praetermitte_spatia();
                c = lege_c();
            }
            if (c == '"') {
                while ((c = lege_c()) != -1 && c != '"') {
                    if (c == '\\')
                        c = lege_effugium();
                    if (i >= MAX_CHORDA - 1)
                        erratum_ad(
                            cur_linea,
                            "chorda litteralis nimis longa (max %d)",
                            MAX_CHORDA - 1
                        );
                    sig.chorda[i++] = c;
                }
                sig.chorda[i]   = '\0';
                sig.lon_chordae = i;
            } else if (
                (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
            ) {
                /* §6.4.5: macra potentiālis quae ad chordam expanditur */
                char mac_nom[256];
                int mi        = 0;
                mac_nom[mi++] = c;
                while (
                    (c = lege_c()) != -1 && (
                        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_'
                    )
                ) {
                    if (mi < 255)
                        mac_nom[mi++] = c;
                }
                mac_nom[mi] = '\0';
                if (c != -1)
                    repone_c();
                macra_t *mac = macra_quaere(mac_nom);
                if (
                    mac && !mac->est_functionalis
                    && mac->valor[0] == '"'
                ) {
                    /* macra simplex expanditur ad chordam — catenā */
                    const char *p = mac->valor + 1;
                    while (*p && *p != '"') {
                        if (i >= MAX_CHORDA - 1)
                            break;
                        sig.chorda[i++] = *p++;
                    }
                    sig.chorda[i]   = '\0';
                    sig.lon_chordae = i;
                } else if (mac) {
                    /* macra (potentiāliter functiōnālis) — expande et
                     * continuā catenātiōnem; expansiō forte chordam generat */
                    pelle_expansionem(mac_nom, mi);
                    /* lege signum ex expansiōne; sī est chorda, catenā */
                    signum_t sig_salva;
                    memcpy(&sig_salva, &sig, sizeof(signum_t));
                    int lin_salva = sig_linea;
                    const char *plica_salva = plica_currentis;
                    int gen       = lege_signum_internum();
                    if (gen == T_STR) {
                        /* macra ad chordam expandēbātur — catenā */
                        for (int si = 0; si < sig.lon_chordae; si++) {
                            if (i >= MAX_CHORDA - 1)
                                break;
                            sig_salva.chorda[i++] = sig.chorda[si];
                        }
                        sig_salva.chorda[i]   = '\0';
                        sig_salva.lon_chordae = i;
                        memcpy(&sig, &sig_salva, sizeof(signum_t));
                        sig_linea = lin_salva;
                        plica_currentis = plica_salva;
                    } else {
                        /* nōn est chorda — servā signum ut spectantem */
                        memcpy(&sig_spectans, &sig, sizeof(signum_t));
                        plica_spectans = plica_currentis;
                        habet_spectantem = 1;
                        memcpy(&sig, &sig_salva, sizeof(signum_t));
                        sig_linea = lin_salva;
                        plica_currentis = plica_salva;
                        break;
                    }
                } else {
                    /* nōn est macra — repōne et exī */
                    pelle_expansionem(mac_nom, mi);
                    break;
                }
            } else {
                if (c != -1)
                    repone_c();
                break;
            }
        }
        return T_STR;
    }

    /* character — §6.4.4.4 */
    if (c == '\'') {
        c = lege_c();
        if (c == '\\')
            c = lege_effugium();
        sig.valor = c;
        c         = lege_c();
        if (c != '\'')
            erratum_ad(
                cur_linea,
                "character litteralis non clausum: expectabatur '"
            );
        sig.genus = T_CHARLIT;
        return T_CHARLIT;
    }

    /* identificator vel vocabulum clavis */
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        int i = 0;
        sig.chorda[i++] = c;
        while ((c = lege_c()) != -1) {
            if (
                (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_'
            ) {
                if (i >= MAX_CHORDA - 1)
                    erratum_ad(cur_linea, "identificator nimis longus");
                sig.chorda[i++] = c;
            } else {
                repone_c();
                break;
            }
        }
        sig.chorda[i] = '\0';

        /* est vocabulum clavis? */
        for (int j = 0; vocabula[j].nomen; j++) {
            if (strcmp(sig.chorda, vocabula[j].nomen) == 0) {
                sig.genus = vocabula[j].genus;
                return sig.genus;
            }
        }

        /* est macra? */
        macra_t *m = macra_quaere(sig.chorda);
        if (m && m->est_functionalis) {
            /* macra functionalis — lege argumenta */
            praetermitte_spatia();
            int pc = lege_c();
            if (pc == '\n') {
                praetermitte_spatia();
                pc = lege_c();
            }
            if (pc == '(') {
                /* alveus planus pro argumentis — acervus pro casu communi,
                 * malloc si excedit */
                char arg_buf_acervus[MAX_CHORDA];
                char *arg_buf   = arg_buf_acervus;
                int arg_buf_cap = MAX_CHORDA;
                int arg_buf_pos = 0;
                int arg_initia[MAX_ARG_MACRAE];
                int nargs   = 0;
                int profund = 0;

                arg_initia[0] = 0;
                int in_chorda = 0;  /* intra chordam litterālem */
                for (;;) {
                    pc = lege_c();
                    if (pc == -1)
                        break;
                    /* §6.10.3: chorda intra argūmenta — commata nōn dēlimitant */
                    if (pc == '"' && !in_chorda) {
                        in_chorda = 1;
                        if (arg_buf_pos < arg_buf_cap)
                            arg_buf[arg_buf_pos++] = pc;
                        while ((pc = lege_c()) != -1) {
                            if (arg_buf_pos < arg_buf_cap)
                                arg_buf[arg_buf_pos++] = pc;
                            if (pc == '\\') {
                                pc = lege_c();
                                if (pc != -1 && arg_buf_pos < arg_buf_cap)
                                    arg_buf[arg_buf_pos++] = pc;
                            } else if (pc == '"') {
                                break;
                            }
                        }
                        in_chorda = 0;
                        continue;
                    }
                    if (pc == '\'' && !in_chorda) {
                        /* character litterālis — praetermitte */
                        if (arg_buf_pos < arg_buf_cap)
                            arg_buf[arg_buf_pos++] = pc;
                        pc = lege_c();
                        if (pc == '\\') {
                            if (arg_buf_pos < arg_buf_cap)
                                arg_buf[arg_buf_pos++] = pc;
                            pc = lege_c();
                        }
                        if (pc != -1 && arg_buf_pos < arg_buf_cap)
                            arg_buf[arg_buf_pos++] = pc;
                        pc = lege_c();
                        if (pc != -1 && arg_buf_pos < arg_buf_cap)
                            arg_buf[arg_buf_pos++] = pc;
                        continue;
                    }
                    if (pc == '(') {
                        profund++;
                    } else if (pc == ')') {
                        if (profund == 0) {
                            if (arg_buf_pos >= arg_buf_cap)
                                erratum_ad(
                                    cur_linea,
                                    "argumenta macrae nimis longa"
                                );
                            arg_buf[arg_buf_pos++] = '\0';
                            if ((arg_buf_pos - 1) > arg_initia[nargs] || nargs > 0) {
                                if (nargs >= MAX_ARG_MACRAE)
                                    erratum_ad(
                                        cur_linea,
                                        "nimis multa argumenta in macra"
                                    );
                                nargs++;
                            }
                            break;
                        }
                        profund--;
                    } else if (
                        pc == ',' && profund == 0
                        && !(
                            m->est_variadica
                            && nargs >= m->num_parametrorum
                        )
                    ) {
                        /* §6.10.3: prō macrā variadicā, commata post ultimum
                         * parametrum inclūduntur in __VA_ARGS__ */
                        if (arg_buf_pos >= arg_buf_cap)
                            erratum_ad(
                                cur_linea,
                                "argumenta macrae nimis longa"
                            );
                        arg_buf[arg_buf_pos++] = '\0';
                        if (nargs >= MAX_ARG_MACRAE - 1)
                            erratum_ad(
                                cur_linea,
                                "nimis multa argumenta in macra"
                            );
                        nargs++;
                        arg_initia[nargs] = arg_buf_pos;
                        continue;
                    }
                    /* cresc alveum si opus est */
                    if (arg_buf_pos >= arg_buf_cap) {
                        int nova_cap = arg_buf_cap * 2;
                        char *novus  = malloc(nova_cap);
                        if (!novus)
                            erratum("memoria exhausta");
                        memcpy(novus, arg_buf, arg_buf_pos);
                        if (arg_buf != arg_buf_acervus)
                            free(arg_buf);
                        arg_buf     = novus;
                        arg_buf_cap = nova_cap;
                    }
                    arg_buf[arg_buf_pos++] = pc;
                }
                /* substitue parametros in corpore macrae */
                static char expansio_alvei[8][MAX_CHORDA * 2];
                static int expansio_vertex = 0;
                char *expansio = expansio_alvei[expansio_vertex % 8];
                expansio_vertex++;
                int ei = 0;
                const char *corpus = m->valor;
                int clon = (int)strlen(corpus);
                for (int ci = 0; ci < clon; ) {
                    /* §6.10.3.3: ## — coniūnctiō signōrum */
                    if (
                        corpus[ci] == '#' && ci + 1 < clon
                        && corpus[ci + 1] == '#'
                    ) {
                        /* tolle spatia prōcēdentia ex expansiōne */
                        while (
                            ei > 0 && (
                                expansio[ei-1] == ' '
                                || expansio[ei-1] == '\t'
                            )
                        )
                            ei--;
                        ci += 2;
                        /* praetermitte spatia post ## */
                        while (
                            ci < clon && (
                                corpus[ci] == ' '
                                || corpus[ci] == '\t'
                            )
                        )
                            ci++;
                        continue;
                    }
                    /* §6.10.3.2: # — stringificātiō */
                    if (corpus[ci] == '#') {
                        ci++;
                        while (
                            ci < clon && (
                                corpus[ci] == ' '
                                || corpus[ci] == '\t'
                            )
                        )
                            ci++;
                        /* lege nōmen parametrī */
                        char stok[256];
                        int sti = 0;
                        while (
                            ci < clon && (
                                (corpus[ci] >= 'a' && corpus[ci] <= 'z') ||
                                (corpus[ci] >= 'A' && corpus[ci] <= 'Z') ||
                                (corpus[ci] >= '0' && corpus[ci] <= '9') ||
                                corpus[ci] == '_'
                            )
                        ) {
                            if (sti < 255)
                                stok[sti++] = corpus[ci];
                            ci++;
                        }
                        stok[sti] = '\0';
                        /* quaere parametrum et stringificā */
                        const char *str_val = stok;
                        for (int pi = 0; pi < m->num_parametrorum; pi++) {
                            if (
                                strcmp(stok, m->parametri[pi]) == 0
                                && pi < nargs
                            ) {
                                str_val = arg_buf + arg_initia[pi];
                                break;
                            }
                        }
                        if (ei < MAX_CHORDA * 2 - 1)
                            expansio[ei++] = '"';
                        while (*str_val && ei < MAX_CHORDA * 2 - 1)
                            expansio[ei++] = *str_val++;
                        if (ei < MAX_CHORDA * 2 - 1)
                            expansio[ei++] = '"';
                        continue;
                    }
                    /* proba si est nomen parametri */
                    if (
                        (corpus[ci] >= 'a' && corpus[ci] <= 'z') ||
                        (corpus[ci] >= 'A' && corpus[ci] <= 'Z') ||
                        corpus[ci] == '_'
                    ) {
                        char tok[256];
                        int ti = 0;
                        while (
                            ci < clon && (
                                (corpus[ci] >= 'a' && corpus[ci] <= 'z') ||
                                (corpus[ci] >= 'A' && corpus[ci] <= 'Z') ||
                                (corpus[ci] >= '0' && corpus[ci] <= '9') ||
                                corpus[ci] == '_'
                            )
                        ) {
                            if (ti < 255)
                                tok[ti++] = corpus[ci];
                            ci++;
                        }
                        tok[ti] = '\0';
                        /* §6.10.3.1: __VA_ARGS__ prō macrīs variadicīs */
                        int invenit = 0;
                        if (
                            m->est_variadica
                            && strcmp(tok, "__VA_ARGS__") == 0
                            && nargs > m->num_parametrorum
                        ) {
                            const char *arg =
                                arg_buf + arg_initia[m->num_parametrorum];
                            while (*arg == ' ' || *arg == '\t')
                                arg++;
                            while (*arg && ei < MAX_CHORDA * 2 - 1)
                                expansio[ei++] = *arg++;
                            invenit = 1;
                        }
                        /* quaere in parametris */
                        if (!invenit)
                            for (int pi = 0; pi < m->num_parametrorum; pi++) {
                            if (strcmp(tok, m->parametri[pi]) == 0 && pi < nargs) {
                                /* substitue argumentum — praetermitte spatia */
                                const char *arg = arg_buf + arg_initia[pi];
                                while (*arg == ' ' || *arg == '\t')
                                    arg++;
                                while (*arg && ei < MAX_CHORDA * 2 - 1)
                                    expansio[ei++] = *arg++;
                                invenit = 1;
                                break;
                            }
                        }
                        if (!invenit) {
                            for (int ki = 0; ki < ti && ei < MAX_CHORDA * 2 - 1; ki++)
                                expansio[ei++] = tok[ki];
                        }
                    } else {
                        if (ei < MAX_CHORDA * 2 - 1)
                            expansio[ei++] = corpus[ci];
                        ci++;
                    }
                }
                expansio[ei] = '\0';
                if (arg_buf != arg_buf_acervus)
                    free(arg_buf);
                pelle_expansionem(expansio, ei);
                goto inicio;
            } else {
                repone_c();
            }
        } else if (m && m->valor[0] != '\0') {
            /* expande macram simplicem — pelle textum expansionis */
            pelle_expansionem(m->valor, strlen(m->valor));
            goto inicio;
        }

        sig.genus = T_IDENT;
        return T_IDENT;
    }

    /* operatores */
    switch (c) {
    case '+': {
            int c2 = lege_c();
            if (c2 == '+') {
                sig.genus = T_PLUSPLUS;
                return T_PLUSPLUS;
            }
            if (c2 == '=') {
                sig.genus = T_PLUSEQ;
                return T_PLUSEQ;
            }
            repone_c();
            sig.genus = T_PLUS;
            return T_PLUS;
        }
    case '-': {
            int c2 = lege_c();
            if (c2 == '-') {
                sig.genus = T_MINUSMINUS;
                return T_MINUSMINUS;
            }
            if (c2 == '=') {
                sig.genus = T_MINUSEQ;
                return T_MINUSEQ;
            }
            if (c2 == '>') {
                sig.genus = T_ARROW;
                return T_ARROW;
            }
            repone_c();
            sig.genus = T_MINUS;
            return T_MINUS;
        }
    case '*': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_STAREQ;
                return T_STAREQ;
            }
            repone_c();
            sig.genus = T_STAR;
            return T_STAR;
        }
    case '/': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_SLASHEQ;
                return T_SLASHEQ;
            }
            repone_c();
            sig.genus = T_SLASH;
            return T_SLASH;
        }
    case '%': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_PERCENTEQ;
                return T_PERCENTEQ;
            }
            repone_c();
            sig.genus = T_PERCENT;
            return T_PERCENT;
        }
    case '&': {
            int c2 = lege_c();
            if (c2 == '&') {
                sig.genus = T_AMPAMP;
                return T_AMPAMP;
            }
            if (c2 == '=') {
                sig.genus = T_AMPEQ;
                return T_AMPEQ;
            }
            repone_c();
            sig.genus = T_AMP;
            return T_AMP;
        }
    case '|': {
            int c2 = lege_c();
            if (c2 == '|') {
                sig.genus = T_PIPEPIPE;
                return T_PIPEPIPE;
            }
            if (c2 == '=') {
                sig.genus = T_PIPEEQ;
                return T_PIPEEQ;
            }
            repone_c();
            sig.genus = T_PIPE;
            return T_PIPE;
        }
    case '^': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_CARETEQ;
                return T_CARETEQ;
            }
            repone_c();
            sig.genus = T_CARET;
            return T_CARET;
        }
    case '<': {
            int c2 = lege_c();
            if (c2 == '<') {
                int c3 = lege_c();
                if (c3 == '=') {
                    sig.genus = T_LTLTEQ;
                    return T_LTLTEQ;
                }
                repone_c();
                sig.genus = T_LTLT;
                return T_LTLT;
            }
            if (c2 == '=') {
                sig.genus = T_LTEQ;
                return T_LTEQ;
            }
            repone_c();
            sig.genus = T_LT;
            return T_LT;
        }
    case '>': {
            int c2 = lege_c();
            if (c2 == '>') {
                int c3 = lege_c();
                if (c3 == '=') {
                    sig.genus = T_GTGTEQ;
                    return T_GTGTEQ;
                }
                repone_c();
                sig.genus = T_GTGT;
                return T_GTGT;
            }
            if (c2 == '=') {
                sig.genus = T_GTEQ;
                return T_GTEQ;
            }
            repone_c();
            sig.genus = T_GT;
            return T_GT;
        }
    case '=': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_EQEQ;
                return T_EQEQ;
            }
            repone_c();
            sig.genus = T_ASSIGN;
            return T_ASSIGN;
        }
    case '!': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_BANGEQ;
                return T_BANGEQ;
            }
            repone_c();
            sig.genus = T_BANG;
            return T_BANG;
        }
    case '~': sig.genus = T_TILDE; return T_TILDE;
    case '?': sig.genus = T_QUESTION; return T_QUESTION;
    case ':': sig.genus = T_COLON; return T_COLON;
    case ';': sig.genus = T_SEMICOLON; return T_SEMICOLON;
    case ',': sig.genus = T_COMMA; return T_COMMA;
    case '(': sig.genus = T_LPAREN; return T_LPAREN;
    case ')': sig.genus = T_RPAREN; return T_RPAREN;
    case '[': sig.genus = T_LBRACKET; return T_LBRACKET;
    case ']': sig.genus = T_RBRACKET; return T_RBRACKET;
    case '{': sig.genus = T_LBRACE; return T_LBRACE;
    case '}': sig.genus = T_RBRACE; return T_RBRACE;
    case '.': {
            int c2 = lege_c();
            if (c2 == '.') {
                int c3 = lege_c();
                if (c3 == '.') {
                    sig.genus = T_ELLIPSIS;
                    return T_ELLIPSIS;
                }
                repone_c();
            }
            repone_c();
            sig.genus = T_DOT;
            return T_DOT;
        }
    }

    erratum_ad(cur_linea, "character ignotus: '%c' (%d)", c, c);
    return T_EOF;
}

/* ================================================================
 * signum spectans (peek)
 * ================================================================ */

void lex_initia(const char *nomen, const char *fons, int longitudo)
{
    cur_nomen     = strdup(nomen);
    cur_fons      = fons;
    cur_longitudo = longitudo;
    cur_positus   = 0;
    cur_linea     = 1;
    plica_currentis = cur_nomen;
    vertex_plicarum = 0;
    vertex_expansionum = 0;
    habet_spectantem = 0;
    cond_vertex = 0;
    num_macrarum = 0;
    num_typedef_nominum = 0;
}

void lex_proximum(void)
{
    if (habet_spectantem) {
        memcpy(&sig, &sig_spectans, sizeof(signum_t));
        sig_linea        = sig.linea;
        plica_currentis  = plica_spectans;
        habet_spectantem = 0;
        return;
    }
    lege_signum_internum();
}

int lex_specta(void)
{
    if (habet_spectantem)
        return sig_spectans.genus;
    signum_t salva;
    memcpy(&salva, &sig, sizeof(signum_t));
    int salva_linea = sig_linea;
    const char *salva_plica = plica_currentis;
    lege_signum_internum();
    memcpy(&sig_spectans, &sig, sizeof(signum_t));
    plica_spectans = plica_currentis;
    memcpy(&sig, &salva, sizeof(signum_t));
    sig_linea        = salva_linea;
    plica_currentis  = salva_plica;
    habet_spectantem = 1;
    return sig_spectans.genus;
}
