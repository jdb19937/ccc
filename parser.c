/*
 * parser.c — CCC parser descendens recursivus
 *
 * Producit arborem syntaxis abstractam (AST) ex signis.
 * Tractat declarationes, sententias, expressiones C99.
 */

#include "utilia.h"
#include "parser.h"
#include "parser_intern.h"
#include "fluat.h"

#include <string.h>
#include <stdlib.h>

/* ================================================================
 * allocatores
 * ================================================================ */

static nodus_t         **nodi_area = NULL;
static int nodi_cap    = 0;
static int nodi_vertex = 0;


static symbolum_t         **symbola_area = NULL;
static int symbola_cap    = 0;
static int symbola_vertex = 0;

static ambitus_t **ambitus_area = NULL;
static int ambitus_cap = 0;
static int ambitus_vertex = 0;

ambitus_t *cur_ambitus = NULL;

nodus_t *nodus_novus(int genus)
{
    if (nodi_vertex >= nodi_cap) {
        int nova_cap = nodi_cap ? nodi_cap * 2 : 1024;
        nodus_t      **novus = realloc(nodi_area, nova_cap * sizeof(nodus_t *));
        if (!novus)
            erratum("memoria exhausta");
        nodi_area = novus;
        nodi_cap  = nova_cap;
    }
    nodus_t *n = calloc(1, sizeof(nodus_t));
    if (!n)
        erratum("memoria exhausta");
    nodi_area[nodi_vertex++] = n;
    n->genus       = genus;
    n->linea       = sig_linea;
    n->init_offset = -1; /* §6.7.8: nullus designator */
    return n;
}


/* ================================================================
 * ambitus (scope)
 * ================================================================ */

void ambitus_intra(void)
{
    if (ambitus_vertex >= ambitus_cap) {
        int nova_cap = ambitus_cap ? ambitus_cap * 2 : 64;
        ambitus_t    **novus = realloc(ambitus_area, nova_cap * sizeof(ambitus_t *));
        if (!novus)
            erratum("memoria exhausta");
        ambitus_area = novus;
        ambitus_cap  = nova_cap;
    }
    ambitus_t *a = calloc(1, sizeof(ambitus_t));
    if (!a)
        erratum("memoria exhausta");
    ambitus_area[ambitus_vertex++] = a;
    a->parens = cur_ambitus;
    a->proximus_offset = cur_ambitus ? cur_ambitus->proximus_offset : 0;
    cur_ambitus = a;
}

void ambitus_exi(void)
{
    if (!cur_ambitus)
        erratum("ambitus_exi sine ambitu");
    if (cur_ambitus->parens)
        cur_ambitus->parens->proximus_offset = cur_ambitus->proximus_offset;
    cur_ambitus = cur_ambitus->parens;
}

symbolum_t *ambitus_quaere(const char *nomen, int genus)
{
    for (ambitus_t *a = cur_ambitus; a; a = a->parens)
        for (symbolum_t *s = a->symbola; s; s = s->proximus)
            if (s->genus == genus && strcmp(s->nomen, nomen) == 0)
                return s;
    return NULL;
}

symbolum_t *ambitus_quaere_omnes(const char *nomen)
{
    for (ambitus_t *a = cur_ambitus; a; a = a->parens)
        for (symbolum_t *s = a->symbola; s; s = s->proximus)
            if (strcmp(s->nomen, nomen) == 0)
                return s;
    return NULL;
}

symbolum_t *ambitus_adde(const char *nomen, int genus)
{
    if (symbola_vertex >= symbola_cap) {
        int nova_cap = symbola_cap ? symbola_cap * 2 : 256;
        symbolum_t   **novus = realloc(symbola_area, nova_cap * sizeof(symbolum_t *));
        if (!novus)
            erratum("memoria exhausta");
        symbola_area = novus;
        symbola_cap  = nova_cap;
    }
    symbolum_t *s = calloc(1, sizeof(symbolum_t));
    if (!s)
        erratum("memoria exhausta");
    symbola_area[symbola_vertex++] = s;
    strncpy(s->nomen, nomen, 255);
    s->genus = genus;
    s->globalis_index = -1;
    s->proximus = cur_ambitus->symbola;
    cur_ambitus->symbola = s;
    return s;
}

ambitus_t *ambitus_currens(void) { return cur_ambitus; }

/* ================================================================
 * auxiliaria parsoris
 * ================================================================ */

void expecta(int genus)
{
    if (sig.genus != genus) {
        erratum_ad(
            sig_linea, "expectabatur %d, inventum %d ('%s')",
            genus, sig.genus, sig.chorda
        );
    }
    lex_proximum();
}

int congruet(int genus)
{
    if (sig.genus == genus) {
        lex_proximum();
        return 1;
    }
    return 0;
}

/* ================================================================
 * est hoc typus? (pro disambiguatione declaratio/expressio)
 * ================================================================ */

int est_specifier_typi(void)
{
    switch (sig.genus) {
    case T_VOID: case T_CHAR: case T_SHORT: case T_INT: case T_LONG:
    case T_FLOAT: case T_DOUBLE: case T_SIGNED: case T_UNSIGNED:
    case T_STRUCT: case T_UNION: case T_ENUM: case T_CONST:
    case T_VOLATILE: case T_STATIC: case T_EXTERN: case T_TYPEDEF:
    case T_REGISTER: case T_AUTO:
    case T_INLINE:   /* §6.7.4 */
    case T_BOOL:     /* §6.2.5 */
    case T_RESTRICT: /* §6.7.3.1 */
        return 1;
    case T_IDENT:
        return lex_est_typus(sig.chorda);
    default:
        return 0;
    }
}

/* ================================================================
 * dissectio typorum
 * ================================================================ */
/* ================================================================
 * translatio (unitas translationis — top level)
 * ================================================================ */

void parse_initia(void)
{
    for (int i = 0; i < nodi_vertex; i++)
        free(nodi_area[i]);
    for (int i = 0; i < symbola_vertex; i++)
        free(symbola_area[i]);
    for (int i = 0; i < ambitus_vertex; i++)
        free(ambitus_area[i]);
    nodi_vertex    = 0;
    symbola_vertex = 0;
    ambitus_vertex = 0;
    cur_ambitus    = NULL;

    typus_initia();
    fluat_initia();  /* §6.2.5¶10: ty_float, ty_double */
    ambitus_intra(); /* ambitus globalis */

    /* lege primum signum */
    lex_proximum();
}

nodus_t *parse_translatio(void)
{
    int cap   = 1024;
    nodus_t   **decls = calloc(cap, sizeof(nodus_t *));
    int ndecl = 0;

    while (sig.genus != T_EOF) {
        nodus_t *d = parse_declaratio(1);
        if (d && d->genus != N_NOP) {
            if (ndecl >= cap) {
                cap *= 2;
                decls = realloc(decls, cap * sizeof(nodus_t *));
            }
            decls[ndecl++] = d;
        }
    }

    nodus_t *radix = nodus_novus(N_BLOCK);
    radix   ->membra  = calloc(ndecl, sizeof(nodus_t *));
    memcpy(radix->membra, decls, ndecl * sizeof(nodus_t *));
    radix->num_membrorum = ndecl;
    return radix;
}
