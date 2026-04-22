/*
 * ic.c — praeprocessor C99 plenus
 *
 * Accipit plicam fontis .c, emittit plicam praeprocessatam .i.
 * Nullae dependentiae externae praeter bibliothecam C standardem.
 *
 * Directivae sustentae:
 *   #include, #define, #undef
 *   #if, #ifdef, #ifndef, #elif, #else, #endif
 *   #line, #error, #warning, #pragma
 *
 * Macrae:
 *   - object-like et function-like
 *   - variadicae cum __VA_ARGS__
 *   - stringificatio '#', concatenatio '##'
 *   - nomina praedefinita: __FILE__, __LINE__, __STDC__,
 *     __STDC_VERSION__, __STDC_HOSTED__, __DATE__, __TIME__
 *
 * Viae capitum:
 *   "nomen" — primum ex directorio includentis, deinde -I, deinde -S
 *   <nomen> — primum ex -I, deinde -S
 *
 * Usus:
 *   ic [-o plica.i] [-I via]* [-S via] [-Dnomen[=valor]]* plica.c
 */

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================
 * limites (ex C99 §5.2.4.1)
 * ================================================================ */

#define LIM_ACERVUS_INCLUD 64    /* profunditas maxima #include */
#define LIM_PARAMETRA      256
#define LIM_ARGUMENTA      256
#define LIM_VIA            4096
#define LIM_IDENT          1024

/* ================================================================
 * errata
 * ================================================================ */

static const char *nomen_programmi = "ic";
static const char *plica_currens   = NULL;
static int         linea_currens   = 0;
static const char *plica_exitus_gl = NULL;

static void erratum(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (plica_currens)
        fprintf(stderr, "%s:%d: erratum: ", plica_currens, linea_currens);
    else
        fprintf(stderr, "%s: erratum: ", nomen_programmi);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    if (plica_exitus_gl)
        remove(plica_exitus_gl);
    exit(1);
}

static void monitum(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (plica_currens)
        fprintf(stderr, "%s:%d: monitum: ", plica_currens, linea_currens);
    else
        fprintf(stderr, "%s: monitum: ", nomen_programmi);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static void *xmalloc(size_t n)
{
    void *p = malloc(n);
    if (!p)
        erratum("memoria exhausta");
    return p;
}

static void *xrealloc(void *p, size_t n)
{
    void *q = realloc(p, n);
    if (!q)
        erratum("memoria exhausta");
    return q;
}

static char *xstrdup(const char *s)
{
    size_t n = strlen(s);
    char     *r  = xmalloc(n + 1);
    memcpy(r, s, n + 1);
    return r;
}

static char *xstrndup(const char *s, int n)
{
    char *r = xmalloc(n + 1);
    memcpy(r, s, n);
    r[n] = 0;
    return r;
}

/* ================================================================
 * signa (tokens)
 * ================================================================ */

typedef enum {
    T_FIN = 0,    /* finis inputi */
    T_NOM,        /* identifier */
    T_PPN,        /* preprocessor number */
    T_STR,        /* chorda literalis "..." */
    T_CHR,        /* constans characteris '...' */
    T_PUN,        /* punctuator */
    T_LIN,        /* linea nova (in directiva) */
    T_LOC,        /* placemarker (pro ##) */
    T_ALI         /* alius character */
} genus_t;

typedef struct hs_s {
    char *nomen;
    struct hs_s *seq;
} hs_t;

typedef struct signum_s {
    genus_t genus;
    char *textus;
    int lon;
    int linea;
    const char *plica;
    int spatium_ante;
    int principium;      /* primum signum lineae */
    hs_t *hs;
    struct signum_s *seq;
} signum_t;

static signum_t *signum_crea(
    genus_t g, const char *text, int lon,
    int linea, const char *plica
) {
    signum_t *s     = xmalloc(sizeof(*s));
    s        ->genus        = g;
    s        ->textus       = xstrndup(text, lon);
    s        ->lon          = lon;
    s        ->linea        = linea;
    s        ->plica        = plica;
    s        ->spatium_ante = 0;
    s        ->principium   = 0;
    s        ->hs           = NULL;
    s        ->seq          = NULL;
    return s;
}

static signum_t *signum_copia(const signum_t *s)
{
    signum_t *c     = xmalloc(sizeof(*c));
    c        ->genus        = s->genus;
    c        ->textus       = xstrndup(s->textus, s->lon);
    c        ->lon          = s->lon;
    c        ->linea        = s->linea;
    c        ->plica        = s->plica;
    c        ->spatium_ante = s->spatium_ante;
    c        ->principium   = s->principium;
    c        ->hs           = s->hs;  /* shared */
    c        ->seq          = NULL;
    return c;
}

static int signum_eq(const signum_t *s, const char *text)
{
    int n = (int)strlen(text);
    return s->lon == n && memcmp(s->textus, text, n) == 0;
}

/* ================================================================
 * hide sets
 * ================================================================ */

static int hs_continet(hs_t *h, const char *nomen)
{
    for (; h; h = h->seq)
        if (strcmp(h->nomen, nomen) == 0)
            return 1;
    return 0;
}

static hs_t *hs_adde(hs_t *h, const char *nomen)
{
    if (hs_continet(h, nomen))
        return h;
    hs_t *n = xmalloc(sizeof(*n));
    n    ->nomen = xstrdup(nomen);
    n    ->seq   = h;
    return n;
}

static hs_t *hs_unio(hs_t *a, hs_t *b)
{
    hs_t *r = a;
    for (; b; b = b->seq)
        r = hs_adde(r, b->nomen);
    return r;
}

static hs_t *hs_intersectio(hs_t *a, hs_t *b)
{
    hs_t *r = NULL;
    for (; a; a = a->seq)
        if (hs_continet(b, a->nomen))
            r = hs_adde(r, a->nomen);
    return r;
}

/* ================================================================
 * macrae
 * ================================================================ */

typedef enum {
    SPEC_NIHIL = 0,
    SPEC_FILE,
    SPEC_LINE,
    SPEC_DATE,
    SPEC_TIME,
    SPEC_STDC,
    SPEC_STDC_VERSION,
    SPEC_STDC_HOSTED
} specialis_t;

typedef struct macra_s {
    char *nomen;
    int functionalis;       /* habet parentheses? */
    int variadica;          /* accipit ...? */
    int num_param;
    char **param;
    signum_t *corpus;       /* lista coniuncta */
    int ago;                /* in expansione: prohibe recursionem */
    specialis_t specialis;
    struct macra_s *seq;
} macra_t;

#define MACRA_HASH 1024
static macra_t *macra_tabula[MACRA_HASH];

static unsigned disper(const char *s, int n)
{
    unsigned h = 5381;
    for (int i = 0; i < n; i++)
        h = h * 33 + (unsigned char)s[i];
    return h;
}

static macra_t *macra_quaere(const char *nomen, int lon)
{
    unsigned h = disper(nomen, lon) % MACRA_HASH;
    macra_t    *m = macra_tabula[h];
    for (; m; m = m->seq)
        if ((int)strlen(m->nomen) == lon && memcmp(m->nomen, nomen, lon) == 0)
            return m;
    return NULL;
}

static void macra_pone(macra_t *m)
{
    unsigned h    = disper(m->nomen, (int)strlen(m->nomen)) % MACRA_HASH;
    m->seq        = macra_tabula[h];
    macra_tabula[h] = m;
}

static void macra_tolle(const char *nomen, int lon)
{
    unsigned h = disper(nomen, lon) % MACRA_HASH;
    macra_t    **pp  = &macra_tabula[h];
    while (*pp) {
        if (
            (int)strlen((*pp)->nomen) == lon &&
            memcmp((*pp)->nomen, nomen, lon) == 0
        ) {
            macra_t *d = *pp;
            pp[0]   = d->seq;
            /* macra ipsa non liberatur; memoria servatur ad finem processus */
            return;
        }
        pp = &(*pp)->seq;
    }
}

/* ================================================================
 * viae inclusionis
 * ================================================================ */

static char **viae_I     = NULL;
static int    num_viae_I = 0;
static int    cap_viae_I = 0;

static char  *via_S      = NULL;
#define VIA_S_DEFALTA "/opt/apotheca/var/ccc/capita"

static void via_I_adde(const char *via)
{
    if (num_viae_I >= cap_viae_I) {
        cap_viae_I = cap_viae_I ? cap_viae_I * 2 : 8;
        viae_I     = xrealloc(viae_I, cap_viae_I * sizeof(char *));
    }
    viae_I[num_viae_I++] = xstrdup(via);
}

/* via_directoria — directorium plicae (cum '/' ultimo), vel "./" si nullum */
static char *via_directoria(const char *via)
{
    const char *ult = strrchr(via, '/');
    if (!ult)
        return xstrdup("./");
    int n = (int)(ult - via) + 1;
    char  *r = xmalloc(n + 1);
    memcpy(r, via, n);
    r[n]     = 0;
    return r;
}

/* ================================================================
 * lectio plicae
 * ================================================================ */

static char *lege_plicam(const char *via, int *lon)
{
    FILE *fp = fopen(via, "rb");
    if (!fp)
        return NULL;
    fseek(fp, 0, SEEK_END);
    long mag = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char     *data = xmalloc(mag + 2);
    size_t r = fread(data, 1, mag, fp);
    (void)r;
    /* assecura novam lineam terminalem */
    if (mag > 0 && data[mag-1] != '\n')
        data[mag++] = '\n';
    data[mag] = 0;
    fclose(fp);
    if (lon)
        *lon = (int)mag;
    return data;
}

/* ================================================================
 * flumen (input stream)
 *
 * Stream singulum tractat unam plicam fontis.
 * Acervus flumen_acervus profunditatem #include custodit.
 * ================================================================ */

typedef struct flumen_s {
    char *plica;            /* via ad plicam (ownata) */
    char *data;             /* contentum plicae */
    int   lon;
    int   pos;              /* positus in data */
    int   linea;            /* linea currens */
    int   bol;              /* signum proximum in principio lineae? */
    struct flumen_s *sub;   /* antecedens in acervo */
} flumen_t;

static flumen_t *flumen_currens = NULL;
static int       flumen_altitudo = 0;

/* specta_char — redde characterem proximum sine consumendo, tractans '\<nl>' */
static int specta_char(flumen_t *f, int *npos, int *nlinea)
{
    int p = f->pos;
    int l = f->linea;
    while (p < f->lon) {
        unsigned char c = (unsigned char)f->data[p];
        if (c == '\\' && p + 1 < f->lon && f->data[p+1] == '\n') {
            p += 2;
            l++;
            continue;
        }
        if (
            c == '\\' && p + 2 < f->lon &&
            f->data[p+1] == '\r' && f->data[p+2] == '\n'
        ) {
            p += 3;
            l++;
            continue;
        }
        if (npos)
            *npos = p;
        if (nlinea)
            *nlinea = l;
        return c;
    }
    if (npos)
        *npos = f->lon;
    if (nlinea)
        *nlinea = l;
    return -1;
}

static int sume_char(flumen_t *f)
{
    int np, nl;
    int c = specta_char(f, &np, &nl);
    if (c < 0)
        return -1;
    f ->pos   = np + 1;
    f ->linea = nl + (c == '\n' ? 1 : 0);
    return c;
}

static int specta_unum(flumen_t *f)
{
    return specta_char(f, NULL, NULL);
}

/* specta_duo — secundus character */
static int specta_duo(flumen_t *f)
{
    int np, nl;
    int c = specta_char(f, &np, &nl);
    if (c < 0)
        return -1;
    /* advanceto post primum */
    int p = np + 1;
    (void)nl;
    while (p < f->lon) {
        unsigned char d = (unsigned char)f->data[p];
        if (d == '\\' && p + 1 < f->lon && f->data[p+1] == '\n') {
            p += 2;
            continue;
        }
        return d;
    }
    return -1;
}

/* ================================================================
 * lexator — producit signum proximum ex flumine
 * ================================================================ */

/* octeti >= 0x80 admittuntur in identificatoribus (sequentiae UTF-8) */
static int est_nondigit(int c) { return isalpha(c) || c == '_' || c >= 0x80; }
static int est_ident_char(int c) { return isalnum(c) || c == '_' || c >= 0x80; }

static int utf8_valida(const char *s, int n)
{
    const unsigned char *p   = (const unsigned char *)s;
    const unsigned char *fin = p + n;
    while (p < fin) {
        unsigned c = *p++;
        if (c < 0x80)
            continue;
        int extra;
        unsigned minimum;
        if ((c & 0xE0) == 0xC0) {
            if (c < 0xC2)
                return 0;
            extra   = 1;
            minimum = 0x80;
        } else if ((c & 0xF0) == 0xE0) {
            extra   = 2;
            minimum = 0x800;
        } else if ((c & 0xF8) == 0xF0) {
            if (c > 0xF4)
                return 0;
            extra   = 3;
            minimum = 0x10000;
        } else {
            return 0;
        }
        if (p + extra > fin)
            return 0;
        unsigned punctum = c & (0x7Fu >> extra);
        for (int i = 0; i < extra; i++) {
            unsigned cc = *p++;
            if ((cc & 0xC0) != 0x80)
                return 0;
            punctum = (punctum << 6) | (cc & 0x3F);
        }
        if (punctum < minimum)
            return 0;
        if (punctum >= 0xD800 && punctum <= 0xDFFF)
            return 0;
        if (punctum > 0x10FFFF)
            return 0;
    }
    return 1;
}

/* praetermitte spatia et commentaria; redde '\n' si invenitur, 0 si pervenit
 * ad signum, -1 si EOF. Pone *space = 1 si quod spatium transivit. */
static int praetermitte_spatia(flumen_t *f, int *space)
{
    for (;;) {
        int c = specta_unum(f);
        if (c < 0)
            return -1;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\v' || c == '\f') {
            sume_char(f);
            *space = 1;
            continue;
        }
        if (c == '/') {
            int d = specta_duo(f);
            if (d == '/') {
                /* commentarium lineae */
                sume_char(f);
                sume_char(f);
                while ((c = specta_unum(f)) >= 0 && c != '\n')
                    sume_char(f);
                *space = 1;
                continue;
            }
            if (d == '*') {
                /* commentarium blocki */
                sume_char(f);
                sume_char(f);
                int prev = 0;
                for (;;) {
                    c = sume_char(f);
                    if (c < 0)
                        erratum("commentarium non terminatum");
                    if (prev == '*' && c == '/')
                        break;
                    prev = c;
                }
                *space = 1;
                continue;
            }
        }
        return c;
    }
}

static signum_t *lex_proximum(flumen_t *f)
{
    int space = 0;
    int c     = praetermitte_spatia(f, &space);

    if (c < 0) {
        signum_t *s = signum_crea(T_FIN, "", 0, f->linea, f->plica);
        s        ->principium = f->bol;
        f        ->bol = 1;
        return s;
    }

    if (c == '\n') {
        int linea = f->linea;
        sume_char(f);
        signum_t *s   = signum_crea(T_LIN, "\n", 1, linea, f->plica);
        s        ->spatium_ante = space;
        s        ->principium = f->bol;
        f        ->bol        = 1;
        return s;
    }

    int bol        = f->bol;
    f->bol         = 0;
    int linea_ini = f->linea;

    /* ident */
    if (est_nondigit(c)) {
        char buf[LIM_IDENT];
        int n = 0;
        while ((c = specta_unum(f)) >= 0 && est_ident_char(c)) {
            if (n >= LIM_IDENT - 1)
                erratum("nomen nimis longum");
            buf[n++] = sume_char(f);
        }
        buf[n] = 0;
        if (!utf8_valida(buf, n))
            erratum("identificator cum UTF-8 invalida: '%s'", buf);
        signum_t *s = signum_crea(T_NOM, buf, n, linea_ini, f->plica);
        s        ->spatium_ante = space;
        s        ->principium   = bol;
        return s;
    }

    /* pp-number: digit, vel '.' + digit */
    if (isdigit(c) || (c == '.' && isdigit(specta_duo(f)))) {
        char buf[LIM_IDENT];
        int n = 0;
        buf[n ++] = sume_char(f);
        for (;;) {
            c = specta_unum(f);
            if (c < 0)
                break;
            if (c == 'e' || c == 'E' || c == 'p' || c == 'P') {
                int d = specta_duo(f);
                if (d == '+' || d == '-') {
                    if (n + 2 >= LIM_IDENT)
                        erratum("numerus nimis longus");
                    buf[n ++] = sume_char(f);
                    buf[n ++] = sume_char(f);
                    continue;
                }
            }
            if (est_ident_char(c) || c == '.') {
                if (n >= LIM_IDENT - 1)
                    erratum("numerus nimis longus");
                buf[n++] = sume_char(f);
                continue;
            }
            break;
        }
        signum_t *s = signum_crea(T_PPN, buf, n, linea_ini, f->plica);
        s        ->spatium_ante = space;
        s        ->principium   = bol;
        return s;
    }

    /* chorda literalis: prefiguri u8, u, U, L accipiuntur qua ident sequi
     * permitteretur. Hic formam nudam tractamus; prefix iam qua ident
     * lectus est. */
    if (c == '"') {
        char    *buf = xmalloc(256);
        int cap = 256;
        int n   = 0;
        buf[n   ++]  = sume_char(f);
        for (;;) {
            c = sume_char(f);
            if (c < 0 || c == '\n')
                erratum("chorda non terminata");
            if (n + 2 >= cap) {
                cap *= 2;
                buf = xrealloc(buf, cap);
            }
            buf[n++] = c;
            if (c == '\\') {
                int esc = sume_char(f);
                if (esc < 0)
                    erratum("escape non terminatum in chorda");
                buf[n++] = esc;
            } else if (c == '"') {
                break;
            }
        }
        signum_t *s   = signum_crea(T_STR, buf, n, linea_ini, f->plica);
        s        ->spatium_ante = space;
        s        ->principium   = bol;
        free(buf);
        return s;
    }

    if (c == '\'') {
        char buf[256];
        int n = 0;
        buf[n ++] = sume_char(f);
        for (;;) {
            c = sume_char(f);
            if (c < 0 || c == '\n')
                erratum("constans characteris non terminata");
            buf[n++] = c;
            if (c == '\\') {
                int esc = sume_char(f);
                if (esc < 0)
                    erratum("escape non terminatum in constante");
                buf[n++] = esc;
            } else if (c == '\'') {
                break;
            }
            if (n >= 255)
                erratum("constans characteris nimis longa");
        }
        signum_t *s   = signum_crea(T_CHR, buf, n, linea_ini, f->plica);
        s        ->spatium_ante = space;
        s        ->principium   = bol;
        return s;
    }

    /* punctuator — proba longiores primum */
    char pun[4] = {0};
    pun[0]  = sume_char(f);
    int lon = 1;
    int c2  = specta_unum(f);
    int c3  = specta_duo(f);
    /* tres characteres: <<= >>= ... */
    if (
        (pun[0] == '<' && c2 == '<' && c3 == '=') ||
        (pun[0] == '>' && c2 == '>' && c3 == '=') ||
        (pun[0] == '.' && c2 == '.' && c3 == '.')
    ) {
        pun[1] = sume_char(f);
        pun[2] = sume_char(f);
        lon    = 3;
    } else if (
        /* duo characteres */
        (pun[0] == '+' && (c2 == '+' || c2 == '=')) ||
        (pun[0] == '-' && (c2 == '-' || c2 == '=' || c2 == '>')) ||
        (pun[0] == '*' && c2 == '=') ||
        (pun[0] == '/' && c2 == '=') ||
        (pun[0] == '%' && c2 == '=') ||
        (pun[0] == '&' && (c2 == '&' || c2 == '=')) ||
        (pun[0] == '|' && (c2 == '|' || c2 == '=')) ||
        (pun[0] == '^' && c2 == '=') ||
        (pun[0] == '<' && (c2 == '=' || c2 == '<')) ||
        (pun[0] == '>' && (c2 == '=' || c2 == '>')) ||
        (pun[0] == '=' && c2 == '=') ||
        (pun[0] == '!' && c2 == '=') ||
        (pun[0] == '#' && c2 == '#')
    ) {
        pun[1] = sume_char(f);
        lon    = 2;
    }

    signum_t *s   = signum_crea(T_PUN, pun, lon, linea_ini, f->plica);
    s        ->spatium_ante = space;
    s        ->principium   = bol;
    return s;
}

/* ================================================================
 * acervus plicarum
 * ================================================================ */

static flumen_t *acervus_trude(const char *via)
{
    if (flumen_altitudo >= LIM_ACERVUS_INCLUD)
        erratum(
            "nimis profunda inclusio (#include acervus > %d)",
            LIM_ACERVUS_INCLUD
        );
    int lon;
    char *data = lege_plicam(via, &lon);
    if (!data)
        erratum("non possum aperire '%s': %s", via, strerror(errno));
    flumen_t *f = xmalloc(sizeof(*f));
    f->plica    = xstrdup(via);
    f->data     = data;
    f->lon      = lon;
    f->pos      = 0;
    f->linea    = 1;
    f->bol      = 1;
    f->sub      = flumen_currens;
    flumen_currens = f;
    flumen_altitudo++;
    return f;
}

static int acervus_tolle(void)
{
    if (!flumen_currens)
        return 0;
    flumen_t       *f = flumen_currens;
    flumen_currens = f->sub;
    flumen_altitudo--;
    free(f->data);
    /* plica non liberatur — signa possunt servare puntatorem */
    free(f);
    return flumen_currens != NULL;
}

/* ================================================================
 * signa pendentia (pushback)
 *
 * Expansio macrae signa in extremum listae pendentium trudit;
 * driver principalis ea primum sumit. Inde signa ex fluminibus petuntur.
 * ================================================================ */

static signum_t *pendentia = NULL;

static void pendentia_trude(signum_t *lista)
{
    /* append ante pendentia currentia */
    if (!lista)
        return;
    signum_t *ult = lista;
    while (ult->seq)
        ult = ult->seq;
    ult       ->seq = pendentia;
    pendentia = lista;
}

static signum_t *signum_proximum_raw(void)
{
    if (pendentia) {
        signum_t  *s = pendentia;
        pendentia = s->seq;
        s         ->seq      = NULL;
        return s;
    }
    while (flumen_currens) {
        signum_t *s = lex_proximum(flumen_currens);
        if (s->genus == T_FIN) {
            if (!acervus_tolle())
                return s;
            free(s->textus);
            free(s);
            continue;
        }
        return s;
    }
    return signum_crea(T_FIN, "", 0, 0, NULL);
}

/* ================================================================
 * emissio exitus
 * ================================================================ */

static FILE       *exitus         = NULL;
static const char *exit_plica    = NULL;
static int         exit_linea    = 0;
static int         exit_col      = 0;
static int         exit_in_bol   = 1;

static void emit_linea_nunc(const char *plica, int linea, int flag)
{
    if (!exit_in_bol) {
        fputc('\n', exitus);
        exit_in_bol = 1;
    }
    if (flag)
        fprintf(exitus, "# %d \"%s\" %d\n", linea, plica, flag);
    else
        fprintf(exitus, "# %d \"%s\"\n", linea, plica);
    exit_plica = plica;
    exit_linea = linea;
    exit_col   = 0;
}

static void emit_signum(signum_t *s)
{
    if (s->genus == T_FIN || s->genus == T_LIN)
        return;
    if (s->genus == T_LOC)
        return;

    /* congrue plicam et lineam */
    if (s->plica && exit_plica != s->plica) {
        emit_linea_nunc(s->plica, s->linea, 0);
    } else if (s->linea != exit_linea) {
        if (s->linea > exit_linea && s->linea - exit_linea < 8) {
            while (exit_linea < s->linea) {
                fputc('\n', exitus);
                exit_linea++;
            }
            exit_col    = 0;
            exit_in_bol = 1;
        } else {
            emit_linea_nunc(s->plica ? s->plica : "<nihil>", s->linea, 0);
        }
    }

    if (s->spatium_ante && !exit_in_bol) {
        fputc(' ', exitus);
        exit_col++;
    }
    fwrite(s->textus, 1, s->lon, exitus);
    exit_col += s->lon;
    exit_in_bol = 0;
}

/* ================================================================
 * directivae — praefatio
 * ================================================================ */

static void tractat_directivam(void);
static void expande_et_emitte(signum_t *s);

/* ================================================================
 * acervus conditionum (#if stack)
 * ================================================================ */

typedef struct cond_s {
    int verum;          /* ramus currens capi debet? */
    int captum;         /* ramus iam captus in hoc grege? */
    int in_else;        /* vidimus #else? */
    int pater_verus;    /* grex parentis captus? */
    struct cond_s *sub;
} cond_t;

static cond_t *cond_stack = NULL;

static int skip_activum(void)
{
    /* saltamus si quae conditio non vera est */
    for (cond_t *c = cond_stack; c; c = c->sub)
        if (!c->verum)
            return 1;
    return 0;
}

static void cond_trude(int valor, int pater)
{
    cond_t *c      = xmalloc(sizeof(*c));
    c->verum       = valor && pater;
    c->captum      = c->verum;
    c->in_else     = 0;
    c->pater_verus = pater;
    c->sub         = cond_stack;
    cond_stack     = c;
}

static void cond_tolle(void)
{
    if (!cond_stack)
        erratum("#endif sine #if");
    cond_t     *c  = cond_stack;
    cond_stack = c->sub;
    free(c);
}

/* ================================================================
 * collecta signorum lineae directivae
 * ================================================================ */

static signum_t *collige_lineam(void)
{
    signum_t *caput = NULL;
    signum_t *ult   = NULL;
    for (;;) {
        signum_t *s = signum_proximum_raw();
        if (s->genus == T_LIN || s->genus == T_FIN) {
            /* reponamus T_FIN si pervenimus, sed T_LIN consumatur */
            if (s->genus == T_FIN) {
                signum_t *lin = signum_crea(T_LIN, "\n", 1, s->linea, s->plica);
                pendentia_trude(s);
                (void)lin;
            } else {
                free(s->textus);
                free(s);
            }
            break;
        }
        if (!caput)
            caput = s;
        if (ult)
            ult->seq = s;
        ult = s;
    }
    return caput;
}

/* ================================================================
 * expansio macrae — hide-set algorithmus
 * ================================================================ */

static signum_t *expande_lista(signum_t *ts);
/* flag: si >0, expande_lista non legit signa ex flumine pro lookahead.
 * usatur cum expande_lista vocatur pro argumentum expandendum. */
static int expande_clausus = 0;

/* signum_ad_chordam — stringificatio ad formam "..." */
static signum_t *signum_ad_chordam(signum_t *lista, int linea, const char *plica)
{
    int cap    = 256;
    char       *buf = xmalloc(cap);
    int n      = 0;
    buf[n      ++] = '"';
    int primum = 1;
    for (signum_t *s = lista; s; s = s->seq) {
        if (s->genus == T_LOC)
            continue;
        if (!primum && s->spatium_ante) {
            if (n + 2 >= cap) {
                cap *= 2;
                buf = xrealloc(buf, cap);
            }
            buf[n++] = ' ';
        }
        primum = 0;
        for (int i = 0; i < s->lon; i++) {
            char c = s->textus[i];
            if (n + 4 >= cap) {
                cap *= 2;
                buf = xrealloc(buf, cap);
            }
            if (s->genus == T_STR || s->genus == T_CHR) {
                if (c == '\\' || c == '"')
                    buf[n++] = '\\';
            }
            buf[n++] = c;
        }
    }
    if (n + 2 >= cap) {
        cap *= 2;
        buf = xrealloc(buf, cap);
    }
    buf[n    ++] = '"';
    signum_t *r = signum_crea(T_STR, buf, n, linea, plica);
    free(buf);
    return r;
}

/* concat_signa — concatena duo signa in unum */
static signum_t *concat_signa(signum_t *a, signum_t *b)
{
    if (a->genus == T_LOC)
        return signum_copia(b);
    if (b->genus == T_LOC)
        return signum_copia(a);

    int n = a->lon + b->lon;
    char  *buf = xmalloc(n + 1);
    memcpy(buf, a->textus, a->lon);
    memcpy(buf + a->lon, b->textus, b->lon);
    buf[n] = 0;

    /* determina genus ex primo charactere */
    genus_t g;
    int c = (unsigned char)buf[0];
    if (est_nondigit(c))
        g = T_NOM;
    else if (isdigit(c))
        g = T_PPN;
    else
        g = T_PUN;

    signum_t *r     = signum_crea(g, buf, n, a->linea, a->plica);
    r        ->spatium_ante = a->spatium_ante;
    r        ->hs           = hs_intersectio(a->hs, b->hs);
    free(buf);
    return r;
}

/* quaere_param — indice parametri macro, -1 si non inventus.
 * pone *va = 1 si est __VA_ARGS__ implicitum */
static int quaere_param(macra_t *m, const signum_t *s, int *va)
{
    *va = 0;
    if (m->variadica && signum_eq(s, "__VA_ARGS__")) {
        *va = 1;
        return m->num_param;
    }
    for (int i = 0; i < m->num_param; i++) {
        if (signum_eq(s, m->param[i]))
            return i;
    }
    return -1;
}

/* sume_argumenta — lege argumenta inter '(' et ')'.
 * '(' iam consumptum esse oportet.
 * reddit listam listarum, *num ponit numerum argumentorum actualium. */
static signum_t **sume_argumenta(macra_t *m, int *num_out)
{
    signum_t **args = xmalloc(LIM_ARGUMENTA * sizeof(signum_t *));
    for (int i = 0; i < LIM_ARGUMENTA; i++)
        args[i] = NULL;
    int num  = 0;
    int prof = 1;

    signum_t *arg_caput = NULL;
    signum_t *arg_ult   = NULL;

    int max_arg = m->variadica ? LIM_ARGUMENTA : m->num_param;
    if (max_arg < 1)
        max_arg = 1;

    for (;;) {
        signum_t *s = signum_proximum_raw();
        if (s->genus == T_FIN)
            erratum("EOF in argumentis macrae '%s'", m->nomen);
        if (s->genus == T_LIN) {
            free(s->textus);
            free(s);
            continue;
        }

        if (s->genus == T_PUN && s->lon == 1) {
            if (s->textus[0] == '(')
                prof++;
            else if (s->textus[0] == ')') {
                prof--;
                if (prof == 0) {
                    if (arg_caput || num > 0) {
                        args[num++] = arg_caput;
                    } else if (m->num_param == 0 && !m->variadica) {
                        /* nullum argumentum si macra nullos parametros habet */
                    } else {
                        args[num++] = arg_caput;
                    }
                    free(s->textus);
                    free(s);
                    break;
                }
            } else if (s->textus[0] == ',' && prof == 1) {
                int in_va = m->variadica && (num >= m->num_param);
                if (!in_va) {
                    args[num  ++] = arg_caput;
                    arg_caput = arg_ult = NULL;
                    free(s->textus);
                    free(s);
                    if (num >= max_arg && !m->variadica)
                        erratum("nimis multa argumenta ad '%s'", m->nomen);
                    continue;
                }
            }
        }

        if (!arg_caput)
            arg_caput = s;
        if (arg_ult)
            arg_ult->seq = s;
        arg_ult = s;
    }

    *num_out = num;
    return args;
}

/* copia_lista — copia listam signorum */
static signum_t *copia_lista(signum_t *src)
{
    signum_t *caput = NULL;
    signum_t *ult   = NULL;
    for (; src; src = src->seq) {
        signum_t *c = signum_copia(src);
        if (!caput)
            caput = c;
        if (ult)
            ult->seq = c;
        ult = c;
    }
    return caput;
}

/* substitue — pars algorithmi C99 §6.10.3.5 */
static signum_t *substitue(
    macra_t *m, signum_t **args, int num_args,
    hs_t *hs, int linea, const char *plica
) {
    signum_t *rslt_caput = NULL;
    signum_t *rslt_ult   = NULL;

    (void)num_args;

    for (signum_t *ip = m->corpus; ip; ip = ip->seq) {
        /* # parametrum */
        if (ip->genus == T_PUN && ip->lon == 1 && ip->textus[0] == '#') {
            signum_t *next = ip->seq;
            if (!next)
                erratum("'#' sine parametro in macra '%s'", m->nomen);
            int va;
            int idx = quaere_param(m, next, &va);
            if (idx < 0)
                erratum("'#' ante non-parametrum '%s'", next->textus);
            signum_t *arg = (idx < LIM_ARGUMENTA) ? args[idx] : NULL;
            signum_t *str = signum_ad_chordam(arg, linea, plica);
            str      ->spatium_ante = ip->spatium_ante;
            if (!rslt_caput)
                rslt_caput = str;
            if (rslt_ult)
                rslt_ult->seq = str;
            rslt_ult = str;
            ip       = next;
            continue;
        }

        /* ## — concatenatio */
        if (
            ip->genus == T_PUN && ip->lon == 2 && ip->textus[0] == '#' &&
            ip->textus[1] == '#'
        ) {
            signum_t *next = ip->seq;
            if (!next || !rslt_ult)
                erratum("## in loco invalido");
            /* sume operandum dextrum */
            signum_t *rhs = NULL;
            int va;
            int idx = (next->genus == T_NOM) ? quaere_param(m, next, &va) : -1;
            if (idx >= 0) {
                signum_t *arg = (idx < LIM_ARGUMENTA) ? args[idx] : NULL;
                rhs      = copia_lista(arg);
                if (!rhs) {
                    rhs = signum_crea(T_LOC, "", 0, linea, plica);
                }
            } else {
                rhs = signum_copia(next);
                rhs ->seq = NULL;
            }
            /* concat rslt_ult et primum signum rhs */
            signum_t *lhs = rslt_ult;
            /* trova praecedentem */
            signum_t *pre = NULL;
            if (rslt_caput == lhs)
                pre = NULL;
            else
                for (pre = rslt_caput; pre->seq != lhs; pre = pre->seq) {}
            signum_t *combo = concat_signa(lhs, rhs);
            combo    ->spatium_ante = lhs->spatium_ante;
            if (pre)
                pre->seq = combo;
            else
                rslt_caput = combo;
            combo->seq = rhs->seq;
            /* libera lhs et primum rhs (modo logice) */
            if (rhs->seq) {
                rslt_ult = rhs->seq;
                while (rslt_ult->seq)
                    rslt_ult = rslt_ult->seq;
            } else {
                rslt_ult = combo;
            }
            ip = next;
            continue;
        }

        /* parametrum nudum */
        int va;
        int idx = (ip->genus == T_NOM) ? quaere_param(m, ip, &va) : -1;
        if (idx >= 0) {
            signum_t *arg = (idx < LIM_ARGUMENTA) ? args[idx] : NULL;
            /* praeverifica: si signum sequens est ##, non expande; alias expande */
            signum_t *nx = ip->seq;
            int sub_concat = (
                nx && nx->genus == T_PUN && nx->lon == 2 &&
                nx->textus[0] == '#' && nx->textus[1] == '#'
            );
            int sub_hash   = 0; /* iam supra tractatum */
            (void)sub_hash;

            signum_t *inst;
            if (sub_concat) {
                inst = copia_lista(arg);
                if (!inst) {
                    inst = signum_crea(T_LOC, "", 0, linea, plica);
                }
            } else {
                signum_t *copia = copia_lista(arg);
                expande_clausus++;
                inst = expande_lista(copia);
                expande_clausus--;
                if (!inst) {
                    inst = signum_crea(T_LOC, "", 0, linea, plica);
                    inst ->spatium_ante = ip->spatium_ante;
                }
            }
            /* prima spatium ex ip */
            if (inst)
                inst->spatium_ante = ip->spatium_ante;
            if (!rslt_caput)
                rslt_caput = inst;
            if (rslt_ult)
                rslt_ult->seq = inst;
            if (inst) {
                rslt_ult = inst;
                while (rslt_ult->seq)
                    rslt_ult = rslt_ult->seq;
            }
            continue;
        }

        /* signum ordinarium */
        signum_t *c = signum_copia(ip);
        if (!rslt_caput)
            rslt_caput = c;
        if (rslt_ult)
            rslt_ult->seq = c;
        rslt_ult = c;
    }

    /* statue hide set; pone lineam et plicam ad locum callsitis */
    for (signum_t *p = rslt_caput; p; p = p->seq) {
        p ->hs    = hs_unio(p->hs, hs);
        p ->linea = linea;
        p ->plica = plica;
    }
    /* tolle T_LOC placemarkers */
    signum_t *p = rslt_caput;
    signum_t *prev = NULL;
    while (p) {
        if (p->genus == T_LOC) {
            signum_t *next = p->seq;
            if (prev)
                prev->seq = next;
            else
                rslt_caput = next;
            free(p->textus);
            free(p);
            p = next;
        } else {
            prev = p;
            p    = p->seq;
        }
    }
    return rslt_caput;
}

/* macra_specialis_expande — reddit lista pro __FILE__, __LINE__, etc. */
static signum_t *macra_specialis_expande(macra_t *m, int linea, const char *plica)
{
    char buf[64];
    signum_t *s = NULL;
    switch (m->specialis) {
    case SPEC_LINE:
        snprintf(buf, sizeof(buf), "%d", linea);
        s = signum_crea(T_PPN, buf, (int)strlen(buf), linea, plica);
        break;
    case SPEC_FILE: {
            int n = (int)strlen(plica ? plica : "<nihil>");
            char  *q = xmalloc(n + 3);
            q[0]  = '"';
            memcpy(q + 1, plica ? plica : "<nihil>", n);
            q[n +1] = '"';
            q[n +2] = 0;
            s   = signum_crea(T_STR, q, n + 2, linea, plica);
            free(q);
            break;
        }
    case SPEC_DATE: {
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            static const char *menses[] = {
                "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
            };
            snprintf(
                buf, sizeof(buf), "\"%s %2d %d\"",
                menses[tm->tm_mon], tm->tm_mday, tm->tm_year + 1900
            );
            s = signum_crea(T_STR, buf, (int)strlen(buf), linea, plica);
            break;
        }
    case SPEC_TIME: {
            time_t t = time(NULL);
            struct tm *tm = localtime(&t);
            snprintf(
                buf, sizeof(buf), "\"%02d:%02d:%02d\"",
                tm->tm_hour, tm->tm_min, tm->tm_sec
            );
            s = signum_crea(T_STR, buf, (int)strlen(buf), linea, plica);
            break;
        }
    case SPEC_STDC:
        s = signum_crea(T_PPN, "1", 1, linea, plica);
        break;
    case SPEC_STDC_VERSION:
        s = signum_crea(T_PPN, "199901L", 7, linea, plica);
        break;
    case SPEC_STDC_HOSTED:
        s = signum_crea(T_PPN, "1", 1, linea, plica);
        break;
    default:
        return NULL;
    }
    return s;
}

/* expande_lista — rescan et expande listam signorum */
static signum_t *expande_lista(signum_t *ts)
{
    signum_t *exitus_caput = NULL;
    signum_t *exitus_ult   = NULL;

    while (ts) {
        signum_t *s = ts;
        ts       = ts->seq;
        s        ->seq = NULL;

        macra_t *m = NULL;
        if (s->genus == T_NOM && !hs_continet(s->hs, s->textus))
            m = macra_quaere(s->textus, s->lon);

        if (!m) {
            if (!exitus_caput)
                exitus_caput = s;
            if (exitus_ult)
                exitus_ult->seq = s;
            exitus_ult = s;
            continue;
        }

        /* macra specialis */
        if (m->specialis != SPEC_NIHIL) {
            signum_t *sp = macra_specialis_expande(m, s->linea, s->plica);
            sp       ->spatium_ante = s->spatium_ante;
            sp       ->hs = hs_adde(s->hs, m->nomen);
            free(s->textus);
            free(s);
            if (!exitus_caput)
                exitus_caput = sp;
            if (exitus_ult)
                exitus_ult->seq = sp;
            exitus_ult = sp;
            continue;
        }

        if (!m->functionalis) {
            hs_t     *new_hs = hs_adde(s->hs, m->nomen);
            signum_t *sub = substitue(m, NULL, 0, new_hs, s->linea, s->plica);
            if (sub)
                sub->spatium_ante = s->spatium_ante;
            free(s->textus);
            free(s);
            /* praepende ad ts */
            if (sub) {
                signum_t *u = sub;
                while (u->seq)
                    u = u->seq;
                u  ->seq = ts;
                ts = sub;
            }
            continue;
        }

        /* functionalis: specta '(' */
        /* prima perquire in ts; si non, in pendentia/flumine.
         * sed si clausus, non legere ex flumine — emitte nomen invariatum. */
        signum_t *peek = ts;
        if (!peek && !expande_clausus)
            peek = signum_proximum_raw();
        if (!peek) {
            /* nihil sequitur — emitte s invariatum */
            if (!exitus_caput)
                exitus_caput = s;
            if (exitus_ult)
                exitus_ult->seq = s;
            exitus_ult = s;
            continue;
        } else {
            /* usa peek ex ts */
        }
        int found_paren = 0;
        signum_t        *praeter = NULL;   /* signa saltata interim */
        signum_t        *praeter_ult = NULL;

        /* saltet spatia et linear-lines; nil sic quia jam signa */
        signum_t *p = peek;
        while (p && p->genus == T_LIN) {
            signum_t *nx = p->seq;
            p        ->seq = NULL;
            if (!praeter)
                praeter = p;
            if (praeter_ult)
                praeter_ult->seq = p;
            praeter_ult = p;
            p = nx;
        }
        if (p && p->genus == T_PUN && p->lon == 1 && p->textus[0] == '(') {
            found_paren = 1;
            /* consume ex ts vel ex pendentibus — hic subtile. */
            if (peek == ts) {
                /* peek venit ex ts; iam elementum primum est p post saltata */
                /* nunc consume p ex ts */
                if (praeter) {
                    /* reponamus saltata in front de ts */
                    /* sed omnia erant T_LIN; ignorantur */
                    for (signum_t *q = praeter; q; ) {
                        signum_t *nx = q->seq;
                        free(q->textus);
                        free(q);
                        q = nx;
                    }
                }
                /* p est in ts: sed ts puntat ad primum saltatum vel p */
                /* reposi ts ad p->seq */
                ts = p->seq;
                free(p->textus);
                free(p);
            } else {
                /* peek venit ex flumine; si praeter contenebat res, restituamus */
                for (signum_t *q = praeter; q; ) {
                    signum_t *nx = q->seq;
                    free(q->textus);
                    free(q);
                    q = nx;
                }
                /* p iam consumptum */
                free(p->textus);
                free(p);
            }
        } else {
            /* nullum '(' — emitte nomen invariatum; reponamus peek */
            if (peek != ts) {
                /* peek venit ex flumine: reponamus */
                if (p) {
                    p  ->seq = ts;
                    ts = p;
                }
                if (praeter_ult) {
                    praeter_ult->seq = ts;
                    ts = praeter;
                }
            }
            if (!exitus_caput)
                exitus_caput = s;
            if (exitus_ult)
                exitus_ult->seq = s;
            exitus_ult = s;
            continue;
        }

        if (!found_paren) {
            if (!exitus_caput)
                exitus_caput = s;
            if (exitus_ult)
                exitus_ult->seq = s;
            exitus_ult = s;
            continue;
        }

        /* lege argumenta. Subtile: argumenta possunt venire ex ts aut ex flumine.
         * Implementatio simplex: si ts est vacua, sume ex flumine. Alias
         * converte ts ad listam temporariam in pendentia et sume. */
        /* Trudit ts ad pendentia */
        signum_t   *old_pend_saved = NULL;
        int had_ts = 0;
        if (ts) {
            had_ts         = 1;
            old_pend_saved = pendentia;
            pendentia      = ts;
            signum_t       *u = ts;
            while (u->seq)
                u = u->seq;
            u  ->seq = old_pend_saved;
            ts = NULL;
        }
        int num;
        signum_t **args = sume_argumenta(m, &num);
        /* post sume_argumenta: pendentia potest continere residuum ts
         * sequitum per old_pend_saved. Separemus: reditum residuum ts ad ts,
         * et restitue old_pend_saved ad pendentia. */
        if (had_ts) {
            if (old_pend_saved == NULL) {
                ts        = pendentia;
                pendentia = NULL;
            } else {
                /* quaere nodum primum old_pend_saved in pendentia */
                signum_t **pp = &pendentia;
                while (*pp && *pp != old_pend_saved)
                    pp = &(*pp)->seq;
                if (*pp == old_pend_saved) {
                    signum_t *ts_rem = (pendentia == old_pend_saved) ? NULL : pendentia;

                    pp[0]     = NULL;
                    ts        = ts_rem;
                    pendentia = old_pend_saved;
                }
                /* alias: old_pend_saved consumptum — nihil separandum */
            }
        }

        /* argumenta expecta */
        int expectata = m->num_param;
        if (m->variadica) {
            if (num < expectata)
                erratum(
                    "macra '%s' accipit saltem %d argumenta, %d data",
                    m->nomen, expectata, num
                );
        } else {
            if (num != expectata) {
                /* casus specialis: macra sine param vocata sine arg:
                 * num=0 expectata=0 ok */
                if (!(num == 1 && expectata == 0 && args[0] == NULL))
                    erratum(
                        "macra '%s' accipit %d argumenta, %d data",
                        m->nomen, expectata, num
                    );
                if (num == 1 && args[0] == NULL)
                    num = 0;
            }
        }

        /* colliga __VA_ARGS__ si variadica */
        signum_t *va_caput = NULL;
        signum_t *va_ult = NULL;
        if (m->variadica) {
            for (int i = m->num_param; i < num; i++) {
                if (i > m->num_param) {
                    signum_t *virg = signum_crea(T_PUN, ",", 1, s->linea, s->plica);
                    if (va_ult)
                        va_ult->seq = virg;
                    else
                        va_caput = virg;
                    va_ult = virg;
                }
                signum_t *pc = copia_lista(args[i]);
                if (pc) {
                    if (va_ult)
                        va_ult->seq = pc;
                    else
                        va_caput = pc;
                    while (pc->seq)
                        pc = pc->seq;
                    va_ult = pc;
                }
            }
            args[m->num_param] = va_caput;
        }

        /* hide set: intersectio(hs(nomen), hs()) ∪ {nomen} */
        /* approximatio: hs(nomen) ∪ {nomen} */
        hs_t *new_hs = hs_adde(s->hs, m->nomen);

        signum_t *sub = substitue(m, args, num, new_hs, s->linea, s->plica);
        if (sub)
            sub->spatium_ante = s->spatium_ante;

        /* praepende sub ad ts */
        if (sub) {
            signum_t *u = sub;
            while (u->seq)
                u = u->seq;
            u  ->seq = ts;
            ts = sub;
        }

        free(args);
        free(s->textus);
        free(s);
    }
    return exitus_caput;
}

/* ================================================================
 * aestimator expressionis #if
 * ================================================================ */

typedef struct {
    signum_t *cursor;
} aest_t;

static long long aest_expressio(aest_t *a);
static long long aest_cond(aest_t *a);

static signum_t *aest_curr(aest_t *a) { return a->cursor; }

static void aest_advance(aest_t *a)
{
    if (a->cursor)
        a->cursor = a->cursor->seq;
}

static int aest_est_pun(signum_t *s, const char *p)
{
    return s && s->genus == T_PUN && signum_eq(s, p);
}

/* parse numerum literalem (integrum) ex pp-number */
static long long aest_numerus(const char *s, int lon)
{
    long long val = 0;
    int i         = 0;
    int base      = 10;
    if (lon >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        i    = 2;
    } else if (lon >= 1 && s[0] == '0') {
        base = 8;
    }
    for (; i < lon; i++) {
        char c = s[i];
        int d;
        if (c >= '0' && c <= '9')
            d = c - '0';
        else if (c >= 'a' && c <= 'f')
            d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            d = c - 'A' + 10;
        else
            break;
        if (d >= base)
            erratum("digitus invalidus in numero: '%c'", c);
        val = val * base + d;
    }
    /* suffixa ignoramus (u, l, ll) */
    return val;
}

/* parse char literal */
static long long aest_characteris(const char *s, int lon)
{
    /* skip prefix L/u/U */
    int i = 0;
    if (i < lon && (s[i] == 'L' || s[i] == 'u' || s[i] == 'U'))
        i++;
    if (i >= lon || s[i] != '\'')
        erratum("constans invalida");
    i++;
    long long val = 0;
    while (i < lon && s[i] != '\'') {
        int c;
        if (s[i] == '\\') {
            i++;
            if (i >= lon)
                erratum("escape truncatum");
            switch (s[i]) {
            case 'n': c = '\n';
                i++;
                break;
            case 't': c = '\t';
                i++;
                break;
            case 'r': c = '\r';
                i++;
                break;
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7': {
                    c     = 0;
                    int k = 0;
                    while (k < 3 && i < lon && s[i] >= '0' && s[i] <= '7') {
                        c = c * 8 + s[i] - '0';
                        i++;
                        k++;
                    }
                    break;
                }
            case 'x': {
                    i++;
                    c = 0;
                    while (i < lon && isxdigit((unsigned char)s[i])) {
                        int d = isdigit((unsigned char)s[i]) ? s[i] - '0' :
                            (tolower((unsigned char)s[i]) - 'a' + 10);
                        c = c * 16 + d;
                        i++;
                    }
                    break;
                }
            case '\\': c = '\\';
                i++;
                break;
            case '\'': c = '\'';
                i++;
                break;
            case '"':  c = '"';
                i++;
                break;
            case 'a':  c = '\a';
                i++;
                break;
            case 'b':  c = '\b';
                i++;
                break;
            case 'f':  c = '\f';
                i++;
                break;
            case 'v':  c = '\v';
                i++;
                break;
            case '?':  c = '?';
                i++;
                break;
            default:   c = s[i];
                i++;
                break;
            }
        } else {
            c = (unsigned char)s[i++];
        }
        val = (val << 8) | (c & 0xff);
    }
    return val;
}

static long long aest_primaria(aest_t *a)
{
    signum_t *s = aest_curr(a);
    if (!s)
        erratum("expressio trunca in #if");

    if (s->genus == T_PUN && signum_eq(s, "(")) {
        aest_advance(a);
        long long v = aest_expressio(a);
        s = aest_curr(a);
        if (!aest_est_pun(s, ")"))
            erratum("expectavi ')' in expressione #if");
        aest_advance(a);
        return v;
    }
    if (s->genus == T_PPN) {
        long long v = aest_numerus(s->textus, s->lon);
        aest_advance(a);
        return v;
    }
    if (s->genus == T_CHR) {
        long long v = aest_characteris(s->textus, s->lon);
        aest_advance(a);
        return v;
    }
    if (s->genus == T_NOM) {
        /* identifier non expansus: fit 0 */
        aest_advance(a);
        return 0;
    }
    erratum("signum invalidum in #if: %.*s", s->lon, s->textus);
    return 0;
}

static long long aest_unaria(aest_t *a)
{
    signum_t *s = aest_curr(a);
    if (aest_est_pun(s, "+")) {
        aest_advance(a);
        return aest_unaria(a);
    }
    if (aest_est_pun(s, "-")) {
        aest_advance(a);
        return -aest_unaria(a);
    }
    if (aest_est_pun(s, "!")) {
        aest_advance(a);
        return !aest_unaria(a);
    }
    if (aest_est_pun(s, "~")) {
        aest_advance(a);
        return ~aest_unaria(a);
    }
    return aest_primaria(a);
}

static long long aest_muldiv(aest_t *a)
{
    long long v = aest_unaria(a);
    for (;;) {
        signum_t *s = aest_curr(a);
        if (aest_est_pun(s, "*")) {
            aest_advance(a);
            v = v * aest_unaria(a);
        }else if (aest_est_pun(s, "/")) {
            aest_advance(a);
            long long r = aest_unaria(a);
            if (r == 0)
                erratum("divisio per 0 in #if");
            v = v / r;
        } else if (aest_est_pun(s, "%")) {
            aest_advance(a);
            long long r = aest_unaria(a);
            if (r == 0)
                erratum("modulo 0 in #if");
            v = v % r;
        } else
            break;
    }
    return v;
}

static long long aest_addsub(aest_t *a)
{
    long long v = aest_muldiv(a);
    for (;;) {
        signum_t *s = aest_curr(a);
        if (aest_est_pun(s, "+")) {
            aest_advance(a);
            v = v + aest_muldiv(a);
        }else if (aest_est_pun(s, "-")) {
            aest_advance(a);
            v = v - aest_muldiv(a);
        }else
            break;
    }
    return v;
}

static long long aest_shift(aest_t *a)
{
    long long v = aest_addsub(a);
    for (;;) {
        signum_t *s = aest_curr(a);
        if (aest_est_pun(s, "<<")) {
            aest_advance(a);
            v = v << aest_addsub(a);
        }else if (aest_est_pun(s, ">>")) {
            aest_advance(a);
            v = v >> aest_addsub(a);
        }else
            break;
    }
    return v;
}

static long long aest_rel(aest_t *a)
{
    long long v = aest_shift(a);
    for (;;) {
        signum_t *s = aest_curr(a);
        if (aest_est_pun(s, "<"))  {
            aest_advance(a);
            v = (v <  aest_shift(a));
        }else if (aest_est_pun(s, ">"))  {
            aest_advance(a);
            v = (v >  aest_shift(a));
        }else if (aest_est_pun(s, "<=")) {
            aest_advance(a);
            v = (v <= aest_shift(a));
        }else if (aest_est_pun(s, ">=")) {
            aest_advance(a);
            v = (v >= aest_shift(a));
        }else
            break;
    }
    return v;
}

static long long aest_eq(aest_t *a)
{
    long long v = aest_rel(a);
    for (;;) {
        signum_t *s = aest_curr(a);
        if (aest_est_pun(s, "==")) {
            aest_advance(a);
            v = (v == aest_rel(a));
        }else if (aest_est_pun(s, "!=")) {
            aest_advance(a);
            v = (v != aest_rel(a));
        }else
            break;
    }
    return v;
}

static long long aest_bit_and(aest_t *a)
{
    long long v = aest_eq(a);
    while (aest_est_pun(aest_curr(a), "&")) {
        aest_advance(a);
        v = v & aest_eq(a);
    }
    return v;
}

static long long aest_bit_xor(aest_t *a)
{
    long long v = aest_bit_and(a);
    while (aest_est_pun(aest_curr(a), "^")) {
        aest_advance(a);
        v = v ^ aest_bit_and(a);
    }
    return v;
}

static long long aest_bit_or(aest_t *a)
{
    long long v = aest_bit_xor(a);
    while (aest_est_pun(aest_curr(a), "|")) {
        aest_advance(a);
        v = v | aest_bit_xor(a);
    }
    return v;
}

static long long aest_log_and(aest_t *a)
{
    long long v = aest_bit_or(a);
    while (aest_est_pun(aest_curr(a), "&&")) {
        aest_advance(a);
        long long r = aest_bit_or(a);
        v = v && r;
    }
    return v;
}

static long long aest_log_or(aest_t *a)
{
    long long v = aest_log_and(a);
    while (aest_est_pun(aest_curr(a), "||")) {
        aest_advance(a);
        long long r = aest_log_and(a);
        v = v || r;
    }
    return v;
}

static long long aest_cond(aest_t *a)
{
    long long v = aest_log_or(a);
    if (aest_est_pun(aest_curr(a), "?")) {
        aest_advance(a);
        long long b = aest_expressio(a);
        signum_t    *s = aest_curr(a);
        if (!aest_est_pun(s, ":"))
            erratum("expectavi ':' in ?: de #if");
        aest_advance(a);
        long long c = aest_cond(a);
        return v ? b : c;
    }
    return v;
}

static long long aest_expressio(aest_t *a)
{
    return aest_cond(a);
}

/* aestima_conditionem — signa conditionalia jam praeparata sunt:
 * 'defined X' et 'defined(X)' iam conversa in 0/1;
 * nomina cetera expansa; identifiers residui sunt 0. */
static int aestima_conditionem(signum_t *signa)
{
    aest_t a;
    a.cursor = signa;
    long long v = aest_expressio(&a);
    if (a.cursor)
        erratum("signa extra post expressionem #if");
    return v != 0;
}

/* praeprocessa conditionem: tracta 'defined', deinde expande */
static signum_t *praepara_conditionem(signum_t *signa)
{
    /* Phase 1: tracta 'defined' */
    signum_t *caput = NULL;
    signum_t *ult   = NULL;
    signum_t *p     = signa;
    while (p) {
        if (p->genus == T_NOM && signum_eq(p, "defined")) {
            signum_t       *n = p->seq;
            signum_t       *nomen_sig = NULL;
            int consumenda = 1;
            if (n && n->genus == T_PUN && signum_eq(n, "(")) {
                nomen_sig = n->seq;
                if (!nomen_sig || nomen_sig->genus != T_NOM)
                    erratum("defined( expectat nomen");
                signum_t *rp = nomen_sig->seq;
                if (!rp || !(rp->genus == T_PUN && signum_eq(rp, ")")))
                    erratum("defined( sine ')'");
                consumenda = 4; /* defined ( nomen ) */
                (void)rp;
            } else if (n && n->genus == T_NOM) {
                nomen_sig  = n;
                consumenda = 2;
            } else {
                erratum("defined sine nomine");
            }
            int def = macra_quaere(nomen_sig->textus, nomen_sig->lon) != NULL;
            signum_t *val = signum_crea(
                T_PPN, def ? "1" : "0", 1,
                p->linea, p->plica
            );
            val->spatium_ante = p->spatium_ante;
            if (!caput)
                caput = val;
            if (ult)
                ult->seq = val;
            ult = val;
            /* salta consumenda signa */
            signum_t *q = p;
            for (int k = 0; k < consumenda && q; k++)
                q = q->seq;
            p = q;
            continue;
        }
        signum_t *c = signum_copia(p);
        if (!caput)
            caput = c;
        if (ult)
            ult->seq = c;
        ult = c;
        p   = p->seq;
    }
    /* Phase 2: expande macras */
    signum_t *expansa = expande_lista(caput);
    return expansa;
}

/* ================================================================
 * directivae
 * ================================================================ */

static void directiva_define(signum_t *linea);
static void directiva_undef(signum_t *linea);
static void directiva_include(signum_t *linea);
static void directiva_if(signum_t *linea, int neg_ifdef, int is_ifdef, int is_if);
static void directiva_elif(signum_t *linea);
static void directiva_else(void);
static void directiva_endif(void);
static void directiva_line(signum_t *linea);
static void directiva_error(signum_t *linea, int est_monitum);
static void directiva_pragma(signum_t *linea);

/* signa_libera — libera listam */
static void signa_libera(signum_t *s)
{
    while (s) {
        signum_t *n = s->seq;
        free(s->textus);
        free(s);
        s = n;
    }
}

static void tractat_directivam(void)
{
    /* '#' iam consumptum. Lege nomen directivae */
    signum_t *cap = collige_lineam();
    signum_t *p   = cap;
    /* salta T_LIN quae nulla, primum signum est nomen directivae */

    /* si nullum, null directive (#\n). */
    if (!p || p->genus == T_LIN || p->genus == T_FIN) {
        signa_libera(cap);
        return;
    }

    if (skip_activum()) {
        /* solum conditionales tractandae sunt */
        if (p->genus == T_NOM) {
            if (
                signum_eq(p, "if") || signum_eq(p, "ifdef") ||
                signum_eq(p, "ifndef")
            ) {
                /* in skip modo omnia conditiones interiores falsa */
                cond_trude(0, 0);
                signa_libera(cap);
                return;
            }
            if (signum_eq(p, "else")) {
                signa_libera(cap);
                if (!cond_stack)
                    erratum("#else sine #if");
                if (cond_stack->in_else)
                    erratum("#else post #else");
                cond_stack->in_else = 1;
                if (!cond_stack->pater_verus) {
                    cond_stack->verum = 0;
                } else if (cond_stack->captum) {
                    cond_stack->verum = 0;
                } else {
                    cond_stack ->verum = 1;
                    cond_stack ->captum = 1;
                }
                return;
            }
            if (signum_eq(p, "elif")) {
                directiva_elif(cap);
                return;
            }
            if (signum_eq(p, "endif")) {
                signa_libera(cap);
                directiva_endif();
                return;
            }
        }
        /* cetera directivae ignorantur in skip modo */
        signa_libera(cap);
        return;
    }

    if (p->genus != T_NOM) {
        erratum("directiva '#' sine nomine");
    }

    if (signum_eq(p, "define")) {
        directiva_define(p->seq);
    } else if (signum_eq(p, "undef")) {
        directiva_undef(p->seq);
    } else if (signum_eq(p, "include")) {
        directiva_include(p->seq);
    } else if (signum_eq(p, "if")) {
        directiva_if(p->seq, 0, 0, 1);
    } else if (signum_eq(p, "ifdef")) {
        directiva_if(p->seq, 0, 1, 0);
    } else if (signum_eq(p, "ifndef")) {
        directiva_if(p->seq, 1, 1, 0);
    } else if (signum_eq(p, "elif")) {
        directiva_elif(cap);
        return;
    } else if (signum_eq(p, "else")) {
        directiva_else();
    } else if (signum_eq(p, "endif")) {
        directiva_endif();
    } else if (signum_eq(p, "line")) {
        directiva_line(p->seq);
    } else if (signum_eq(p, "error")) {
        directiva_error(p->seq, 0);
    } else if (signum_eq(p, "warning")) {
        directiva_error(p->seq, 1);
    } else if (signum_eq(p, "pragma")) {
        directiva_pragma(p->seq);
    } else {
        erratum("directiva ignota: #%.*s", p->lon, p->textus);
    }

    signa_libera(cap);
}

/* --- #define --- */

static void directiva_define(signum_t *r)
{
    if (!r || r->genus != T_NOM)
        erratum("#define sine nomine");
    signum_t *nomen_sig = r;
    macra_t  *m = xmalloc(sizeof(*m));
    memset(m, 0, sizeof(*m));
    m->nomen = xstrndup(nomen_sig->textus, nomen_sig->lon);

    signum_t *after = nomen_sig->seq;

    /* function-like: '(' sine spatio post nomen */
    if (
        after && after->genus == T_PUN && signum_eq(after, "(") &&
        !after->spatium_ante
    ) {
        m->functionalis = 1;
        m->param        = xmalloc(LIM_PARAMETRA * sizeof(char *));
        signum_t *q     = after->seq;
        int primum = 1;
        for (;;) {
            if (!q)
                erratum("#define functionalis sine ')'");
            if (q->genus == T_PUN && signum_eq(q, ")")) {
                q = q->seq;
                break;
            }
            if (!primum) {
                if (!(q->genus == T_PUN && signum_eq(q, ",")))
                    erratum("expectavi ',' in parametris");
                q = q->seq;
                if (!q)
                    erratum("parametrum post ',' exspectatum");
            }
            primum = 0;
            if (q->genus == T_PUN && signum_eq(q, "...")) {
                m ->variadica = 1;
                q = q->seq;
                if (!q || !(q->genus == T_PUN && signum_eq(q, ")")))
                    erratum("'...' debet esse ultimum parametrum");
                q = q->seq;
                break;
            }
            if (q->genus != T_NOM)
                erratum("nomen parametri expectatum");
            if (m->num_param >= LIM_PARAMETRA)
                erratum("nimis multi parametri in macra");
            m ->param[m->num_param++] = xstrndup(q->textus, q->lon);
            q = q->seq;
        }
        after = q;
    }

    /* corpus: signa restantia */
    signum_t *corpus_caput = NULL;
    signum_t *corpus_ult   = NULL;
    for (
        signum_t *q = after;
        q && q->genus != T_LIN && q->genus != T_FIN;
        q = q->seq
    ) {
        signum_t *c = signum_copia(q);
        if (!corpus_caput)
            corpus_caput = c;
        if (corpus_ult)
            corpus_ult->seq = c;
        corpus_ult = c;
    }
    /* primum signum corporis non habet spatium ante quantum ad substitutionem */
    m->corpus = corpus_caput;

    /* redefinitio: si macra exstat, nova praeterit silens si idem */
    macra_t *exs = macra_quaere(m->nomen, (int)strlen(m->nomen));
    if (exs) {
        macra_tolle(m->nomen, (int)strlen(m->nomen));
    }
    macra_pone(m);
}

/* --- #undef --- */

static void directiva_undef(signum_t *r)
{
    if (!r || r->genus != T_NOM)
        erratum("#undef sine nomine");
    macra_tolle(r->textus, r->lon);
}

/* --- #include --- */

/* resolve_viam — quaere plicam, reddit viam plenam in buf */
static int resolve_viam(const char *nomen, int quaestivum, char *buf, int buf_mag)
{
    /* quaestivum: 1 si "...", 0 si <...> */
    FILE *fp;
    if (quaestivum && flumen_currens && flumen_currens->plica) {
        char *dir = via_directoria(flumen_currens->plica);
        snprintf(buf, buf_mag, "%s%s", dir, nomen);
        free(dir);
        fp = fopen(buf, "rb");
        if (fp) {
            fclose(fp);
            return 1;
        }
    }
    for (int i = 0; i < num_viae_I; i++) {
        snprintf(buf, buf_mag, "%s/%s", viae_I[i], nomen);
        fp = fopen(buf, "rb");
        if (fp) {
            fclose(fp);
            return 1;
        }
    }
    if (via_S) {
        snprintf(buf, buf_mag, "%s/%s", via_S, nomen);
        fp = fopen(buf, "rb");
        if (fp) {
            fclose(fp);
            return 1;
        }
    }
    return 0;
}

static void directiva_include(signum_t *r)
{
    /* Simplex: si primum signum est T_STR vel '<', tracta directe.
     * Alias, expande et recursively tracta. */
    if (!r || (r->genus != T_STR && !(r->genus == T_PUN && signum_eq(r, "<")))) {
        /* expande */
        signum_t *copia = copia_lista(r);
        signum_t *exp   = expande_lista(copia);
        r        = exp;
        if (!r)
            erratum("#include sine plica");
    }

    char nomen[LIM_VIA];
    int quaestivum;

    if (r->genus == T_STR) {
        if (r->lon < 2 || r->textus[0] != '"')
            erratum("#include \"...\" invalidum");
        int n = r->lon - 2;
        if (n >= LIM_VIA)
            erratum("via nimis longa");
        memcpy(nomen, r->textus + 1, n);
        nomen[n]   = 0;
        quaestivum = 1;
    } else if (r->genus == T_PUN && signum_eq(r, "<")) {
        /* post expansionem: < foo . h > */
        int n    = 0;
        signum_t *q = r->seq;
        while (q && !(q->genus == T_PUN && signum_eq(q, ">"))) {
            if (q->spatium_ante && n > 0) {
                if (n >= LIM_VIA - 1)
                    erratum("via nimis longa");
                nomen[n++] = ' ';
            }
            if (n + q->lon >= LIM_VIA)
                erratum("via nimis longa");
            memcpy(nomen + n, q->textus, q->lon);
            n += q->lon;
            q = q->seq;
        }
        nomen[n]   = 0;
        quaestivum = 0;
    } else {
        erratum("#include expectat \"...\" vel <...>");
        return;
    }

    char via_plena[LIM_VIA + 256];
    if (!resolve_viam(nomen, quaestivum, via_plena, sizeof(via_plena)))
        erratum("plica '%s' non inventa", nomen);

    /* aperi et trude */
    flumen_t *f = acervus_trude(via_plena);
    /* emitte marker #line pro novae plicae */
    emit_linea_nunc(f->plica, 1, 1);
}

/* --- #if / #ifdef / #ifndef --- */

static void directiva_if(signum_t *r, int neg_ifdef, int is_ifdef, int is_if)
{
    int pater = cond_stack ? cond_stack->verum : 1;
    int valor = 0;
    if (is_ifdef) {
        if (!r || r->genus != T_NOM)
            erratum("#ifdef/#ifndef sine nomine");
        int def = macra_quaere(r->textus, r->lon) != NULL;
        valor   = neg_ifdef ? !def : def;
    } else if (is_if) {
        signum_t *copia = copia_lista(r);
        signum_t *prep  = praepara_conditionem(copia);
        valor    = aestima_conditionem(prep);
    }
    cond_trude(valor, pater);
}

/* --- #elif --- */

static void directiva_elif(signum_t *linea)
{
    if (!cond_stack)
        erratum("#elif sine #if");
    if (cond_stack->in_else)
        erratum("#elif post #else");
    /* salta 'elif' token ad initium */
    signum_t *p = linea;
    if (p && p->genus == T_NOM && signum_eq(p, "elif"))
        p = p->seq;

    if (!cond_stack->pater_verus) {
        cond_stack->verum = 0;
        return;
    }
    if (cond_stack->captum) {
        cond_stack->verum = 0;
        return;
    }
    signum_t   *copia = copia_lista(p);
    signum_t   *prep  = praepara_conditionem(copia);
    int valor  = aestima_conditionem(prep);
    cond_stack ->verum  = valor;
    cond_stack ->captum = valor ? 1 : cond_stack->captum;
}

/* --- #else --- */

static void directiva_else(void)
{
    if (!cond_stack)
        erratum("#else sine #if");
    if (cond_stack->in_else)
        erratum("#else post #else");
    cond_stack->in_else = 1;
    if (!cond_stack->pater_verus) {
        cond_stack->verum = 0;
        return;
    }
    if (cond_stack->captum) {
        cond_stack->verum = 0;
    } else {
        cond_stack ->verum = 1;
        cond_stack ->captum = 1;
    }
}

/* --- #endif --- */

static void directiva_endif(void)
{
    cond_tolle();
}

/* --- #line --- */

static void directiva_line(signum_t *r)
{
    signum_t *copia = copia_lista(r);
    signum_t *exp   = expande_lista(copia);
    if (!exp || exp->genus != T_PPN)
        erratum("#line expectat numerum");
    long long n = aest_numerus(exp->textus, exp->lon);
    if (n <= 0)
        erratum("#line numerus invalidus");
    signum_t   *q = exp->seq;
    const char *plica_nova = NULL;
    if (q && q->genus == T_STR) {
        /* exordine " ... " */
        int lon = q->lon - 2;
        if (lon < 0)
            lon = 0;
        char       *s = xstrndup(q->textus + 1, lon);
        plica_nova = s;
    }
    if (flumen_currens) {
        flumen_currens->linea = (int)n;
        if (plica_nova) {
            /* non liberamus veterem; servatur ad signa vetera */
            flumen_currens->plica = (char *)plica_nova;
        }
    }
    emit_linea_nunc(flumen_currens->plica, (int)n, 0);
}

/* --- #error / #warning --- */

static void directiva_error(signum_t *r, int est_monitum)
{
    char buf[1024];
    int n = 0;
    for (
        signum_t *p = r;
        p && p->genus != T_LIN && p->genus != T_FIN;
        p = p->seq
    ) {
        if (p->spatium_ante && n > 0 && n < (int)sizeof(buf) - 1)
            buf[n++] = ' ';
        int t = p->lon;
        if (n + t >= (int)sizeof(buf))
            t = sizeof(buf) - 1 - n;
        memcpy(buf + n, p->textus, t);
        n += t;
    }
    buf[n] = 0;
    if (est_monitum)
        monitum("#warning: %s", buf);
    else
        erratum("#error: %s", buf);
}

/* --- #pragma --- */

static void directiva_pragma(signum_t *r)
{
    /* emitte directivam verbatim ad exitum */
    if (!exit_in_bol) {
        fputc('\n', exitus);
        exit_in_bol = 1;
    }
    fprintf(exitus, "#pragma");
    for (
        signum_t *p = r;
        p && p->genus != T_LIN && p->genus != T_FIN;
        p = p->seq
    ) {
        fputc(' ', exitus);
        fwrite(p->textus, 1, p->lon, exitus);
    }
    fputc('\n', exitus);
    exit_in_bol = 1;
    exit_linea++;
}

/* ================================================================
 * driver principalis
 * ================================================================ */

static void expande_et_emitte(signum_t *s)
{
    /* s est una lista (potest una signorum expansionem producere) */
    signum_t *exp = expande_lista(s);
    for (signum_t *p = exp; p; p = p->seq)
        emit_signum(p);
    /* non liberamus — accepta */
}

static void praedefini_macras(void);

static void agita(const char *plica_ingressus)
{
    acervus_trude(plica_ingressus);
    emit_linea_nunc(flumen_currens->plica, 1, 0);

    for (;;) {
        signum_t *s = signum_proximum_raw();
        if (s->genus == T_FIN)
            break;

        if (skip_activum()) {
            /* in salto: solum tracta directivas, ignora alias signa */
            if (
                s->genus == T_PUN && s->lon == 1 && s->textus[0] == '#' &&
                s->principium
            ) {
                plica_currens = s->plica;
                linea_currens = s->linea;
                free(s->textus);
                free(s);
                tractat_directivam();
                continue;
            }
            if (s->genus == T_LIN) {
                free(s->textus);
                free(s);
                continue;
            }
            /* signa ordinaria in salto: deiice */
            free(s->textus);
            free(s);
            continue;
        }

        if (s->genus == T_LIN) {
            /* emitte si exitus non recenter emissus habuit lineam novam */
            /* gere via emit per linea tracking — non explicite emitte */
            free(s->textus);
            free(s);
            continue;
        }

        if (
            s->genus == T_PUN && s->lon == 1 && s->textus[0] == '#' &&
            s->principium
        ) {
            plica_currens = s->plica;
            linea_currens = s->linea;
            free(s->textus);
            free(s);
            tractat_directivam();
            continue;
        }

        plica_currens = s->plica;
        linea_currens = s->linea;
        expande_et_emitte(s);
    }
    if (cond_stack)
        erratum("#if non terminatum ante EOF");
}

/* macras praedefinitas pone */
static void praedefini_macras(void)
{
    static struct {
        const char *nomen;
        specialis_t sp;
    } praed[] = {
        { "__FILE__",         SPEC_FILE },
        { "__LINE__",         SPEC_LINE },
        { "__DATE__",         SPEC_DATE },
        { "__TIME__",         SPEC_TIME },
        { "__STDC__",         SPEC_STDC },
        { "__STDC_VERSION__", SPEC_STDC_VERSION },
        { "__STDC_HOSTED__",  SPEC_STDC_HOSTED },
    };
    for (size_t i = 0; i < sizeof(praed)/sizeof(praed[0]); i++) {
        macra_t *m  = xmalloc(sizeof(*m));
        memset(m, 0, sizeof(*m));
        m ->nomen     = xstrdup(praed[i].nomen);
        m ->specialis = praed[i].sp;
        macra_pone(m);
    }
}

/* defini_macram_arg — tracta -DNOMEN[=valor] */
static void defini_macram_arg(const char *s)
{
    const char *eq = strchr(s, '=');
    char nomen[LIM_IDENT];
    const char *valor = "1";
    if (eq) {
        int n = (int)(eq - s);
        if (n >= LIM_IDENT)
            erratum("nomen -D nimis longum");
        memcpy(nomen, s, n);
        nomen[n] = 0;
        valor    = eq + 1;
    } else {
        snprintf(nomen, sizeof(nomen), "%s", s);
    }

    macra_t *m = xmalloc(sizeof(*m));
    memset(m, 0, sizeof(*m));
    m->nomen = xstrdup(nomen);

    /* parse valor ut lista signorum — simplex: tracta ut unum PPN si numerus,
     * sin identifier, alias chorda */
    if (valor[0]) {
        int vn = (int)strlen(valor);
        signum_t *cs;
        /* tracta quodlibet valorem ut PPN si digit, T_STR si " ", alias NOM */
        if (isdigit((unsigned char)valor[0]))
            cs = signum_crea(T_PPN, valor, vn, 0, "<-D>");
        else if (valor[0] == '"')
            cs = signum_crea(T_STR, valor, vn, 0, "<-D>");
        else
            cs = signum_crea(T_NOM, valor, vn, 0, "<-D>");
        m->corpus = cs;
    }
    macra_pone(m);
}

/* ================================================================
 * principale
 * ================================================================ */

static void usus(void)
{
    fprintf(
        stderr,
        "usus: ic [-o plica.i] [-I via]* [-S via] [-Dnomen[=valor]]* plica.c\n"
        "\n"
        "optiones:\n"
        "  -o <plica>   plica exitus (defalta: plica.i)\n"
        "  -I <via>     adde viam inclusionis\n"
        "  -S <via>     via capitum systematis\n"
        "               (defalta: " VIA_S_DEFALTA ")\n"
        "  -D<nomen>    defini nomen (valor: 1 si non datus)\n"
        "  -U<nomen>    exde nomen\n"
        "  -h, --help   monstra hoc\n"
    );
    exit(1);
}

static int finit_in_c(const char *s)
{
    int n = (int)strlen(s);
    return n > 2 && s[n-2] == '.' && s[n-1] == 'c';
}

int main(int argc, char *argv[])
{
    const char *plica_fontis = NULL;
    const char *plica_exitus = NULL;

    /* -D/-U colligenda priusquam plicam fontis tractamus */
    typedef struct {
        int D_non_U;
        const char *arg;
    }opt_macra_t;
    opt_macra_t opt_macrae[256];
    int num_opt = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (++i >= argc)
                usus();
            plica_exitus = argv[i];
        } else if (strncmp(argv[i], "-I", 2) == 0) {
            const char *v = argv[i][2] ? argv[i] + 2 :
                (++i < argc ? argv[i] : NULL);
            if (!v)
                usus();
            via_I_adde(v);
        } else if (strncmp(argv[i], "-S", 2) == 0) {
            const char *v = argv[i][2] ? argv[i] + 2 :
                (++i < argc ? argv[i] : NULL);
            if (!v)
                usus();
            via_S = xstrdup(v);
        } else if (strncmp(argv[i], "-D", 2) == 0) {
            const char *v = argv[i][2] ? argv[i] + 2 :
                (++i < argc ? argv[i] : NULL);
            if (!v)
                usus();
            if (num_opt >= 256)
                erratum("nimis multae -D optiones");
            opt_macrae[num_opt] .D_non_U = 1;
            opt_macrae[num_opt] .arg     = v;
            num_opt++;
        } else if (strncmp(argv[i], "-U", 2) == 0) {
            const char *v = argv[i][2] ? argv[i] + 2 :
                (++i < argc ? argv[i] : NULL);
            if (!v)
                usus();
            if (num_opt >= 256)
                erratum("nimis multae optiones -U");
            opt_macrae[num_opt] .D_non_U = 0;
            opt_macrae[num_opt] .arg     = v;
            num_opt++;
        } else if (
            strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--help") == 0
        ) {
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

    if (!finit_in_c(plica_fontis))
        erratum("plica '%s' non desinit in .c", plica_fontis);

    if (!via_S)
        via_S = xstrdup(VIA_S_DEFALTA);

    /* computa nomen exitus */
    char auto_exitus[LIM_VIA];
    if (!plica_exitus) {
        int n = (int)strlen(plica_fontis);
        if (n + 1 >= LIM_VIA)
            erratum("nomen plicae nimis longum");
        memcpy(auto_exitus, plica_fontis, n);
        auto_exitus[n  -1] = 'i';
        auto_exitus[n] = 0;
        plica_exitus   = auto_exitus;
    }

    /* aperi exitum in plica temporaria — renominabitur post successum */
    char plica_tmp[LIM_VIA + 8];
    int nx = snprintf(plica_tmp, sizeof(plica_tmp), "%s.tmp", plica_exitus);
    if (nx < 0 || nx >= (int)sizeof(plica_tmp))
        erratum("nomen plicae nimis longum");
    exitus = fopen(plica_tmp, "wb");
    if (!exitus)
        erratum(
            "non possum scribere '%s': %s",
            plica_tmp, strerror(errno)
        );
    plica_exitus_gl = plica_tmp;

    praedefini_macras();

    for (int i = 0; i < num_opt; i++) {
        if (opt_macrae[i].D_non_U)
            defini_macram_arg(opt_macrae[i].arg);
        else
            macra_tolle(opt_macrae[i].arg, (int)strlen(opt_macrae[i].arg));
    }

    /* agita */
    agita(plica_fontis);

    /* linea nova finalis si necessaria */
    if (!exit_in_bol)
        fputc('\n', exitus);
    fclose(exitus);
    if (rename(plica_tmp, plica_exitus) != 0) {
        remove(plica_tmp);
        erratum(
            "non possum renominare '%s' ad '%s': %s",
            plica_tmp, plica_exitus, strerror(errno)
        );
    }
    plica_exitus_gl = NULL;
    return 0;
}
