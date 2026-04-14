/*
 * parser.c — CCC parser descendens recursivus
 *
 * Producit arborem syntaxis abstractam (AST) ex signis.
 * Tractat declarationes, sententias, expressiones C99.
 */

#include "ccc.h"

/* ================================================================
 * allocatores
 * ================================================================ */

static nodus_t nodi_area[MAX_NODI];
static int nodi_vertex = 0;

static typus_t typi_area[MAX_TYPI];
static int typi_vertex = 0;

static symbolum_t symbola_area[MAX_SYMBOLA];
static int symbola_vertex = 0;

static ambitus_t ambitus_area[MAX_AMBITUS];
static int ambitus_vertex = 0;

static ambitus_t *cur_ambitus = NULL;

nodus_t *nodus_novus(int genus)
{
    if (nodi_vertex >= MAX_NODI)
        erratum("nimis multi nodi");
    nodus_t *n = &nodi_area[nodi_vertex++];
    memset(n, 0, sizeof(nodus_t));
    n->genus = genus;
    n->linea = sig_linea;
    return n;
}

typus_t *typus_novus(int genus)
{
    if (typi_vertex >= MAX_TYPI)
        erratum("nimis multi typi");
    typus_t *t = &typi_area[typi_vertex++];
    memset(t, 0, sizeof(typus_t));
    t->genus = genus;
    t->num_elementorum = -1;
    return t;
}

/* ================================================================
 * typi praefiniti
 * ================================================================ */

typus_t *ty_void;
typus_t *ty_char;
typus_t *ty_uchar;
typus_t *ty_short;
typus_t *ty_ushort;
typus_t *ty_int;
typus_t *ty_uint;
typus_t *ty_long;
typus_t *ty_ulong;

static void initia_typos(void)
{
    ty_void  = typus_novus(TY_VOID);
    ty_void->magnitudo = 0;
    ty_void->colineatio = 1;

    ty_char  = typus_novus(TY_CHAR);
    ty_char->magnitudo = 1;
    ty_char->colineatio = 1;

    ty_uchar = typus_novus(TY_UCHAR);
    ty_uchar->magnitudo = 1;
    ty_uchar->colineatio = 1;
    ty_uchar->est_sine_signo = 1;

    ty_short = typus_novus(TY_SHORT);
    ty_short->magnitudo = 2;
    ty_short->colineatio = 2;

    ty_ushort = typus_novus(TY_USHORT);
    ty_ushort->magnitudo = 2;
    ty_ushort->colineatio = 2;
    ty_ushort->est_sine_signo = 1;

    ty_int   = typus_novus(TY_INT);
    ty_int->magnitudo = 4;
    ty_int->colineatio = 4;

    ty_uint  = typus_novus(TY_UINT);
    ty_uint->magnitudo = 4;
    ty_uint->colineatio = 4;
    ty_uint->est_sine_signo = 1;

    ty_long  = typus_novus(TY_LONG);
    ty_long->magnitudo = 8;
    ty_long->colineatio = 8;

    ty_ulong = typus_novus(TY_ULONG);
    ty_ulong->magnitudo = 8;
    ty_ulong->colineatio = 8;
    ty_ulong->est_sine_signo = 1;
}

/* ================================================================
 * utilia typorum
 * ================================================================ */

typus_t *typus_indicem(typus_t *basis)
{
    typus_t *t    = typus_novus(TY_PTR);
    t->basis      = basis;
    t->magnitudo  = 8;
    t->colineatio = 8;
    return t;
}

typus_t *typus_tabulam(typus_t *basis, int num)
{
    typus_t *t = typus_novus(TY_ARRAY);
    t->basis = basis;
    t->num_elementorum = num;
    t->colineatio = basis->colineatio;
    t->magnitudo = basis->magnitudo * num;
    return t;
}

int typus_magnitudo(typus_t *t)
{
    if (!t)
        return 0;
    return t->magnitudo;
}

int typus_colineatio(typus_t *t)
{
    if (!t)
        return 1;
    return t->colineatio ? t->colineatio : 1;
}

int typus_est_integer(typus_t *t)
{
    if (!t)
        return 0;
    return t->genus >= TY_CHAR && t->genus <= TY_ULLONG;
}

int typus_est_index(typus_t *t)
{
    if (!t)
        return 0;
    return t->genus == TY_PTR || t->genus == TY_ARRAY;
}

int typus_est_arithmeticus(typus_t *t)
{
    return typus_est_integer(t);
}

typus_t *typus_basis_indicis(typus_t *t)
{
    if (!t)
        return ty_void;
    if (t->genus == TY_PTR)
        return t->basis ? t->basis : ty_void;
    if (t->genus == TY_ARRAY)
        return t->basis ? t->basis : ty_void;
    return ty_void;
}

/* ================================================================
 * ambitus (scope)
 * ================================================================ */

void ambitus_intra(void)
{
    if (ambitus_vertex >= MAX_AMBITUS)
        erratum("nimis multi ambitus");
    ambitus_t *a = &ambitus_area[ambitus_vertex++];
    memset(a, 0, sizeof(ambitus_t));
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
    if (symbola_vertex >= MAX_SYMBOLA)
        erratum("nimis multa symbola");
    symbolum_t *s = &symbola_area[symbola_vertex++];
    memset(s, 0, sizeof(symbolum_t));
    strncpy(s->nomen, nomen, 255);
    s->genus = genus;
    s->got_index = -1;
    s->globalis_index = -1;
    s->proximus = cur_ambitus->symbola;
    cur_ambitus->symbola = s;
    return s;
}

ambitus_t *ambitus_currens(void) { return cur_ambitus; }

/* ================================================================
 * auxiliaria parsoris
 * ================================================================ */

static void expecta(int genus)
{
    if (sig.genus != genus) {
        erratum_ad(
            sig_linea, "expectabatur %d, inventum %d ('%s')",
            genus, sig.genus, sig.chorda
        );
    }
    lex_proximum();
}

static int congruet(int genus)
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

static int est_specifier_typi(void)
{
    switch (sig.genus) {
    case T_VOID: case T_CHAR: case T_SHORT: case T_INT: case T_LONG:
    case T_FLOAT: case T_DOUBLE: case T_SIGNED: case T_UNSIGNED:
    case T_STRUCT: case T_UNION: case T_ENUM: case T_CONST:
    case T_VOLATILE: case T_STATIC: case T_EXTERN: case T_TYPEDEF:
    case T_REGISTER: case T_AUTO:
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

static typus_t *parse_struct_vel_union(void);
static typus_t *parse_enum(void);
static nodus_t *parse_expr(void);
static nodus_t *parse_expr_assign(void);
static nodus_t *parse_sententia(void);
static nodus_t *parse_blocum(void);
static nodus_t *parse_declaratio(int est_globalis);

/* parse specifiers basicos (int, long, unsigned, struct, etc.) */
static typus_t *parse_specifiers(
    int *est_staticus, int *est_externus,
    int *est_typedef
) {
    int s_stat     = 0, s_ext = 0, s_td = 0;
    int s_unsigned = 0;
    int s_short    = 0, s_long = 0, s_longlong = 0;
    int s_char     = 0, s_int = 0, s_void = 0;
    int s_const    = 0;
    typus_t *t     = NULL;

    for (;;) {
        switch (sig.genus) {
        case T_STATIC:   s_stat = 1;
            lex_proximum();
            continue;
        case T_EXTERN:   s_ext = 1;
            lex_proximum();
            continue;
        case T_TYPEDEF:  s_td = 1;
            lex_proximum();
            continue;
        case T_CONST:    s_const = 1;
            lex_proximum();
            continue;
        case T_VOLATILE: lex_proximum(); continue;
        case T_REGISTER: lex_proximum(); continue;
        case T_AUTO:     lex_proximum(); continue;
        case T_SIGNED:   lex_proximum(); continue;
        case T_UNSIGNED: s_unsigned = 1;
            lex_proximum();
            continue;
        case T_SHORT:    s_short = 1;
            lex_proximum();
            continue;
        case T_LONG:
            if (s_long)
                s_longlong = 1;
            s_long = 1;
            lex_proximum();
            continue;
        case T_CHAR:     s_char = 1;
            lex_proximum();
            continue;
        case T_INT:      s_int = 1;
            lex_proximum();
            continue;
        case T_VOID:     s_void = 1;
            lex_proximum();
            continue;
        case T_FLOAT:    lex_proximum();
            t = ty_int;
            continue;
            /* fictus */
        case T_DOUBLE:   lex_proximum();
            t = ty_long;
            continue;
            /* fictus */
        case T_STRUCT:
        case T_UNION:
            t = parse_struct_vel_union();
            continue;
        case T_ENUM:
            t = parse_enum();
            continue;
        case T_IDENT:
            if (lex_est_typus(sig.chorda)) {
                symbolum_t *sym = ambitus_quaere(sig.chorda, SYM_TYPEDEF);
                if (sym && sym->typus)
                    t = sym->typus;
                else
                    t = ty_int;
                lex_proximum();
                continue;
            }
            goto finis;
        default:
            goto finis;
        }
    }
finis:
    if (est_staticus)
        *est_staticus = s_stat;
    if (est_externus)
        *est_externus = s_ext;
    if (est_typedef)
        *est_typedef = s_td;

    if (t) {
        if (s_const) {
            typus_t *tc = typus_novus(t->genus);
            *tc = *t;
            tc->est_constans = 1;
            return tc;
        }
        return t;
    }

    /* construere typum ex specifiers */
    if (s_void)
        return ty_void;
    if (s_char)
        return s_unsigned ? ty_uchar : ty_char;
    if (s_short)
        return s_unsigned ? ty_ushort : ty_short;
    if (s_longlong || (s_long && s_int) || s_long)
        return s_unsigned ? ty_ulong : ty_long;

    /* defalta: int */
    return s_unsigned ? ty_uint : ty_int;
}

/* parse struct vel union */
static typus_t *parse_struct_vel_union(void)
{
    int est_struct = (sig.genus == T_STRUCT);
    lex_proximum(); /* consume struct/union */
    (void)est_struct;

    char nomen_tag[128] = {0};
    if (sig.genus == T_IDENT) {
        strncpy(nomen_tag, sig.chorda, 127);
        lex_proximum();
    }

    /* si iam definitum, quaere */
    if (nomen_tag[0] && sig.genus != T_LBRACE) {
        symbolum_t *s = ambitus_quaere(nomen_tag, SYM_STRUCT_TAG);
        if (s && s->typus)
            return s->typus;
        /* declaratio ante — crea typum incompletum */
        typus_t *t = typus_novus(TY_STRUCT);
        strncpy(t->nomen_tag, nomen_tag, 127);
        symbolum_t *ns = ambitus_adde(nomen_tag, SYM_STRUCT_TAG);
        ns->typus      = t;
        return t;
    }

    typus_t *t = NULL;
    if (nomen_tag[0]) {
        symbolum_t *s = ambitus_quaere(nomen_tag, SYM_STRUCT_TAG);
        if (s && s->typus) {
            t = s->typus;
        }
    }
    if (!t) {
        t = typus_novus(TY_STRUCT);
        strncpy(t->nomen_tag, nomen_tag, 127);
        if (nomen_tag[0]) {
            symbolum_t *ns = ambitus_adde(nomen_tag, SYM_STRUCT_TAG);
            ns->typus      = t;
        }
    }

    if (sig.genus == T_LBRACE) {
        lex_proximum();
        t->membra        = calloc(MAX_MEMBRA, sizeof(membrum_t));
        t->num_membrorum = 0;
        int offset       = 0;

        while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
            int s_stat         = 0, s_ext = 0;
            typus_t *typ_basis = parse_specifiers(&s_stat, &s_ext, NULL);

            while (sig.genus != T_SEMICOLON && sig.genus != T_EOF) {
                typus_t *typ_mem = typ_basis;
                /* indicis */
                while (sig.genus == T_STAR) {
                    lex_proximum();
                    typ_mem = typus_indicem(typ_mem);
                }

                char nom_mem[128] = {0};
                if (sig.genus == T_IDENT) {
                    strncpy(nom_mem, sig.chorda, 127);
                    lex_proximum();
                }

                /* tabulae */
                while (sig.genus == T_LBRACKET) {
                    lex_proximum();
                    int num = 0;
                    if (sig.genus == T_NUM) {
                        num = (int)sig.valor;
                        lex_proximum();
                    }
                    expecta(T_RBRACKET);
                    typ_mem = typus_tabulam(typ_mem, num);
                }

                /* colineatio */
                int col = typus_colineatio(typ_mem);
                if (col > 0)
                    offset = (offset + col - 1) & ~(col - 1);

                if (t->num_membrorum < MAX_MEMBRA) {
                    membrum_t *mem = &t->membra[t->num_membrorum++];
                    strncpy(mem->nomen, nom_mem, 127);
                    mem->typus  = typ_mem;
                    mem->offset = offset;
                }
                offset += typus_magnitudo(typ_mem);

                if (sig.genus == T_COMMA)
                    lex_proximum();
                else
                    break;
            }
            expecta(T_SEMICOLON);
        }
        expecta(T_RBRACE);

        int max_col = 1;
        for (int i = 0; i < t->num_membrorum; i++) {
            int c = typus_colineatio(t->membra[i].typus);
            if (c > max_col)
                max_col = c;
        }
        t->colineatio    = max_col;
        t->magnitudo     = (offset + max_col - 1) & ~(max_col - 1);
        t->est_perfectum = 1;
    }

    return t;
}

/* parse enum */
static typus_t *parse_enum(void)
{
    lex_proximum(); /* consume 'enum' */

    char nomen_tag[128] = {0};
    if (sig.genus == T_IDENT) {
        strncpy(nomen_tag, sig.chorda, 127);
        lex_proximum();
    }

    typus_t *t    = typus_novus(TY_ENUM);
    t->magnitudo  = 4;
    t->colineatio = 4;
    strncpy(t->nomen_tag, nomen_tag, 127);

    if (nomen_tag[0]) {
        symbolum_t *es = ambitus_quaere(nomen_tag, SYM_ENUM_TAG);
        if (!es) {
            es        = ambitus_adde(nomen_tag, SYM_ENUM_TAG);
            es->typus = t;
        } else {
            t = es->typus;
        }
    }

    if (sig.genus == T_LBRACE) {
        lex_proximum();
        int val = 0;
        while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
            char nomen[256];
            strncpy(nomen, sig.chorda, 255);
            expecta(T_IDENT);

            if (congruet(T_ASSIGN)) {
                /* expressio constans simplex */
                int negat = 0;
                if (sig.genus == T_MINUS) {
                    negat = 1;
                    lex_proximum();
                }
                if (sig.genus == T_NUM) {
                    val = (int)sig.valor;
                    if (negat)
                        val = -val;
                    lex_proximum();
                } else if (sig.genus == T_IDENT) {
                    symbolum_t *cs = ambitus_quaere(sig.chorda, SYM_ENUM_CONST);
                    if (cs)
                        val = cs->valor_enum;
                    lex_proximum();
                    /* supporta expressiones simplices: IDENT | NUM */
                    if (sig.genus == T_PIPE) {
                        lex_proximum();
                        if (sig.genus == T_NUM) {
                            val |= (int)sig.valor;
                            lex_proximum();
                        }
                    }
                }
            }

            symbolum_t *s   = ambitus_adde(nomen, SYM_ENUM_CONST);
            s->typus        = ty_int;
            s->valor_enum   = val;
            s->est_globalis = 1;
            val++;

            if (sig.genus == T_COMMA)
                lex_proximum();
        }
        expecta(T_RBRACE);
    }

    return t;
}

/* ================================================================
 * parse declarator (nomen + indicis + tabulae + functio)
 * ================================================================ */

static nodus_t *ultima_vla_expr; /* §6.7.5.2: VLA expressio magnitudinis */

static typus_t *parse_declarator(typus_t *basis, char *nomen, int max_nomen)
{
    typus_t *t = basis;

    /* indicis: * * * */
    int num_stellarum = 0;
    while (sig.genus == T_STAR) {
        num_stellarum++;
        lex_proximum();
        /* praetermitte const/volatile post * */
        while (sig.genus == T_CONST || sig.genus == T_VOLATILE)
            lex_proximum();
    }

    /* nomen (optionale) */
    nomen[0] = '\0';
    if (sig.genus == T_LPAREN && lex_specta() == T_STAR) {
        /* indicis ad functionem: non supportatum plene */
        lex_proximum(); /* ( */
        lex_proximum(); /* * */
        if (sig.genus == T_IDENT) {
            strncpy(nomen, sig.chorda, max_nomen - 1);
            lex_proximum();
        }
        expecta(T_RPAREN);
        /* parametri functionis */
        if (sig.genus == T_LPAREN) {
            lex_proximum();
            while (sig.genus != T_RPAREN && sig.genus != T_EOF)
                lex_proximum();
            expecta(T_RPAREN);
        }
        /* typus est index ad functionem — simpliciter pone ut index ad void */
        for (int i = 0; i < num_stellarum; i++)
            t = typus_indicem(t);
        t = typus_indicem(t);
        return t;
    }

    if (sig.genus == T_IDENT) {
        strncpy(nomen, sig.chorda, max_nomen - 1);
        lex_proximum();
    }

    /* applica indicis */
    for (int i = 0; i < num_stellarum; i++)
        t = typus_indicem(t);

    /* tabulae: [N] [M] ... */
    /* §6.7.5.2: collige dimensiones, applica in ordine inverso
     * ut int a[2][3] fiat TY_ARRAY(TY_ARRAY(int,3),2) */
    {
        int dims[64];
        nodus_t *vla_expr[64];
        int ndims = 0;
        while (sig.genus == T_LBRACKET) {
            lex_proximum();
            int num     = 0;
            nodus_t *ve = NULL;
            if (sig.genus == T_RBRACKET) {
                /* [] — dimensio ignota */
            } else {
                nodus_t *e = parse_expr_assign();
                if (e && e->genus == N_NUM)
                    num = (int)e->valor;
                else
                    ve = e; /* VLA — expressio non constans */
            }
            expecta(T_RBRACKET);
            if (ndims < 64) {
                dims[ndims]     = num;
                vla_expr[ndims] = ve;
                ndims++;
            }
        }
        for (int i = ndims - 1; i >= 0; i--)
            t = typus_tabulam(t, dims[i]);
        /* serva primam VLA expressionem in variabile globali */
        ultima_vla_expr = NULL;
        for (int i = 0; i < ndims; i++)
            if (vla_expr[i]) {
            ultima_vla_expr = vla_expr[i];
            break;
        }
    }

    return t;
}

/* ================================================================
 * parse parametros functionis
 * ================================================================ */

static typus_t *parse_parametros(
    typus_t *reditus,
    symbolum_t ***param_sym, int *num_param
) {
    typus_t *tf = typus_novus(TY_FUNC);
    tf->reditus = reditus;
    tf->magnitudo = 8;
    tf->colineatio = 8;
    tf->parametri = calloc(MAX_PARAM, sizeof(typus_t *));
    tf->nomina_param = calloc(MAX_PARAM, sizeof(char *));
    tf->num_parametrorum = 0;
    tf->est_variadicus = 0;

    expecta(T_LPAREN);

    /* void */
    if (sig.genus == T_VOID && lex_specta() == T_RPAREN) {
        lex_proximum();
        expecta(T_RPAREN);
        return tf;
    }

    if (sig.genus == T_RPAREN) {
        lex_proximum();
        return tf;
    }

    static symbolum_t *param_syms[MAX_PARAM];
    int np = 0;

    while (sig.genus != T_RPAREN && sig.genus != T_EOF) {
        if (sig.genus == T_ELLIPSIS) {
            tf->est_variadicus = 1;
            lex_proximum();
            break;
        }

        int s_stat  = 0, s_ext = 0;
        typus_t *tb = parse_specifiers(&s_stat, &s_ext, NULL);
        char nomen[256] = {0};
        typus_t *tp = parse_declarator(tb, nomen, 256);

        /* tabulae in parametris -> indicis */
        if (tp->genus == TY_ARRAY)
            tp = typus_indicem(tp->basis);

        if (tf->num_parametrorum < MAX_PARAM) {
            tf->parametri[tf->num_parametrorum]    = tp;
            tf->nomina_param[tf->num_parametrorum] = nomen[0] ? strdup(nomen) : NULL;

            if (nomen[0]) {
                symbolum_t *ps = ambitus_adde(nomen, SYM_VAR);
                ps->typus = tp;
                ps->est_parametrus = 1;
                param_syms[np++] = ps;
            }
            tf->num_parametrorum++;
        }

        if (sig.genus == T_COMMA)
            lex_proximum();
        else
            break;
    }
    expecta(T_RPAREN);

    if (param_sym) {
        *param_sym = calloc(np, sizeof(symbolum_t *));
        memcpy(*param_sym, param_syms, np * sizeof(symbolum_t *));
    }
    if (num_param)
        *num_param = np;

    return tf;
}

/* ================================================================
 * expressiones
 * ================================================================ */

static nodus_t *parse_expr_unaria(void);
static nodus_t *parse_expr_postfixa(void);
static nodus_t *parse_expr_primaria(void);
static nodus_t *parse_expr_conditio(void);

/* primaria: numerus, chorda, ident, (expr), sizeof */
static nodus_t *parse_expr_primaria(void)
{
    nodus_t *n;

    switch (sig.genus) {
    case T_NUM:
        n        = nodus_novus(N_NUM);
        n->valor = sig.valor;
        n->typus = ty_int;
        if (sig.valor > 0x7FFFFFFF || sig.valor < -0x7FFFFFFF)
            n->typus = ty_long;
        lex_proximum();
        return n;

    case T_CHARLIT:
        n        = nodus_novus(N_NUM);
        n->valor = sig.valor;
        n->typus = ty_int;
        lex_proximum();
        return n;

    case T_STR:
        n         = nodus_novus(N_STR);
        n->chorda = malloc(sig.lon_chordae + 1);
        memcpy(n->chorda, sig.chorda, sig.lon_chordae + 1);
        n->lon_chordae = sig.lon_chordae;
        n->typus       = typus_indicem(ty_char);
        lex_proximum();
        return n;

    case T_IDENT: {
            char nomen[256];
            strncpy(nomen, sig.chorda, 255);
            nomen[255] = '\0';
            lex_proximum();

            n        = nodus_novus(N_IDENT);
            n->nomen = strdup(nomen);

        /* quaere symbolum et salva in nodo */
            symbolum_t *s = ambitus_quaere_omnes(nomen);
            if (s) {
                if (s->genus == SYM_ENUM_CONST) {
                    n->genus = N_NUM;
                    n->valor = s->valor_enum;
                    n->typus = ty_int;
                } else {
                    n->typus = s->typus;
                    n->sym   = s;
                }
            } else {
            /* functio non declarata — praesume int f(...) */
                n->typus = ty_int;
            }
            return n;
        }

    case T_LPAREN: {
            lex_proximum();
        /* est cast? */
            if (est_specifier_typi()) {
                int s_stat  = 0, s_ext = 0;
                typus_t *tb = parse_specifiers(&s_stat, &s_ext, NULL);
                char nom[256] = {0};
                typus_t *ct = parse_declarator(tb, nom, 256);
                expecta(T_RPAREN);

                /* compound literal: (typename){ ... } */
                if (sig.genus == T_LBRACE) {
                    lex_proximum();
                    /* parse initializer list ut N_BLOCK cum elementis */
                    nodus_t *elems[256];
                    int nel = 0;
                    while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
                        /* praetermitte designatores */
                        if (sig.genus == T_DOT) {
                            lex_proximum();
                            if (sig.genus == T_IDENT)
                                lex_proximum();
                            if (sig.genus == T_ASSIGN)
                                lex_proximum();
                        }
                        if (sig.genus == T_LBRACKET) {
                            lex_proximum();
                            if (sig.genus == T_NUM)
                                lex_proximum();
                            if (sig.genus == T_RBRACKET)
                                lex_proximum();
                            if (sig.genus == T_ASSIGN)
                                lex_proximum();
                        }
                        if (sig.genus == T_LBRACE) {
                            /* sub-init */
                            lex_proximum();
                            while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
                                elems[nel++] = parse_expr_assign();
                                if (!congruet(T_COMMA))
                                    break;
                            }
                            expecta(T_RBRACE);
                        } else {
                            elems[nel++] = parse_expr_assign();
                        }
                        if (!congruet(T_COMMA))
                            break;
                    }
                    expecta(T_RBRACE);
                    /* crea nodum N_BLOCK cum elementis */
                    n         = nodus_novus(N_BLOCK);
                    n->typus  = ct;
                    n->membra = calloc(nel, sizeof(nodus_t *));
                    memcpy(n->membra, elems, nel * sizeof(nodus_t *));
                    n->num_membrorum = nel;
                    return n;
                }

                n = nodus_novus(N_CAST);
                n->typus_decl = ct;
                n->typus = ct;
                n->sinister = parse_expr_unaria();
                return n;
            }
            n = parse_expr();
            expecta(T_RPAREN);
            return n;
        }

    case T_SIZEOF: {
            lex_proximum();
            if (sig.genus == T_LPAREN) {
                /* sizeof( — consume parenthesim et inspice an typus sequatur */
                lex_proximum(); /* consume '(' */
                if (est_specifier_typi()) {
                    int s_stat  = 0;
                    int s_ext   = 0;
                    typus_t *tb = parse_specifiers(&s_stat, &s_ext, NULL);
                    char nom[256] = {0};
                    typus_t *st   = parse_declarator(tb, nom, 256);

                    expecta(T_RPAREN);
                    n        = nodus_novus(N_NUM);
                    n->valor = typus_magnitudo(st);
                    n->typus = ty_long;
                    return n;
                }

                /* sizeof(expr) */
                nodus_t *e = parse_expr();

                expecta(T_RPAREN);
                n        = nodus_novus(N_NUM);
                n->valor = e->typus ? typus_magnitudo(e->typus) : 0;
                n->typus = ty_long;
                return n;
            }

            /* sizeof expr */
            nodus_t *e = parse_expr_unaria();
            n = nodus_novus(N_NUM);
            n->valor = e->typus ? typus_magnitudo(e->typus) : 0;
            n->typus = ty_long;
            return n;
        }

    default:
        erratum_ad(
            sig_linea, "expressio inexpectata: %d '%s'",
            sig.genus, sig.chorda
        );
        return nodus_novus(N_NOP);
    }
}

/* postfixa: a[i], a.b, a->b, a++, a--, f(args) */
static nodus_t *parse_expr_postfixa(void)
{
    nodus_t *n = parse_expr_primaria();

    for (;;) {
        if (sig.genus == T_LBRACKET) {
            lex_proximum();
            nodus_t *idx  = nodus_novus(N_INDEX);
            idx->sinister = n;
            idx->dexter   = parse_expr();
            expecta(T_RBRACKET);
            if (n->typus)
                idx->typus = typus_basis_indicis(n->typus);
            n = idx;
        } else if (sig.genus == T_DOT) {
            lex_proximum();
            nodus_t *mem  = nodus_novus(N_MEMBER);
            mem->sinister = n;
            mem->nomen    = strdup(sig.chorda);
            expecta(T_IDENT);
            /* typus membri */
            if (n->typus && n->typus->genus == TY_STRUCT) {
                for (int i = 0; i < n->typus->num_membrorum; i++) {
                    if (strcmp(n->typus->membra[i].nomen, mem->nomen) == 0) {
                        mem->typus = n->typus->membra[i].typus;
                        break;
                    }
                }
            }
            n = mem;
        } else if (sig.genus == T_ARROW) {
            lex_proximum();
            nodus_t *mem  = nodus_novus(N_ARROW);
            mem->sinister = n;
            mem->nomen    = strdup(sig.chorda);
            expecta(T_IDENT);
            if (
                n->typus && n->typus->genus == TY_PTR && n->typus->basis &&
                n->typus->basis->genus == TY_STRUCT
            ) {
                typus_t *st = n->typus->basis;
                for (int i = 0; i < st->num_membrorum; i++) {
                    if (strcmp(st->membra[i].nomen, mem->nomen) == 0) {
                        mem->typus = st->membra[i].typus;
                        break;
                    }
                }
            }
            n = mem;
        } else if (sig.genus == T_LPAREN) {
            /* vocatio functionis */
            lex_proximum();
            nodus_t *vocatio  = nodus_novus(N_CALL);
            vocatio->sinister = n;

            nodus_t *args[MAX_PARAM];
            int nargs = 0;
            while (sig.genus != T_RPAREN && sig.genus != T_EOF) {
                if (nargs > 0)
                    expecta(T_COMMA);
                args[nargs++] = parse_expr_assign();
            }
            expecta(T_RPAREN);

            vocatio->membra = calloc(nargs, sizeof(nodus_t *));
            memcpy(vocatio->membra, args, nargs * sizeof(nodus_t *));
            vocatio->num_membrorum = nargs;

            /* typus reditus */
            if (n->typus && n->typus->genus == TY_FUNC)
                vocatio->typus = n->typus->reditus;
            else if (
                n->typus && n->typus->genus == TY_PTR &&
                n->typus->basis && n->typus->basis->genus == TY_FUNC
            )
                vocatio->typus = n->typus->basis->reditus;
            else
                vocatio->typus = ty_int;
            n = vocatio;
        } else if (sig.genus == T_PLUSPLUS) {
            lex_proximum();
            nodus_t *post = nodus_novus(N_POSTOP);
            post->op = T_PLUSPLUS;
            post->sinister = n;
            post->typus = n->typus;
            n = post;
        } else if (sig.genus == T_MINUSMINUS) {
            lex_proximum();
            nodus_t *post = nodus_novus(N_POSTOP);
            post->op = T_MINUSMINUS;
            post->sinister = n;
            post->typus = n->typus;
            n = post;
        } else {
            break;
        }
    }
    return n;
}

/* unaria: -x, ~x, !x, *x, &x, ++x, --x, (cast)x */
static nodus_t *parse_expr_unaria(void)
{
    switch (sig.genus) {
    case T_MINUS: case T_TILDE: case T_BANG: {
            int op = sig.genus;
            lex_proximum();
            nodus_t *n  = nodus_novus(N_UNOP);
            n->op       = op;
            n->sinister = parse_expr_unaria();
            n->typus    = n->sinister->typus;
            return n;
        }
    case T_STAR: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_DEREF);
            n->sinister = parse_expr_unaria();
            if (n->sinister->typus)
                n->typus = typus_basis_indicis(n->sinister->typus);
            else
                n->typus = ty_int;
            return n;
        }
    case T_AMP: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_ADDR);
            n->sinister = parse_expr_unaria();
            n->typus    = typus_indicem(n->sinister->typus ? n->sinister->typus : ty_int);
            return n;
        }
    case T_PLUSPLUS: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_UNOP);
            n->op       = T_PLUSPLUS;
            n->sinister = parse_expr_unaria();
            n->typus    = n->sinister->typus;
            return n;
        }
    case T_MINUSMINUS: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_UNOP);
            n->op       = T_MINUSMINUS;
            n->sinister = parse_expr_unaria();
            n->typus    = n->sinister->typus;
            return n;
        }
    default:
        return parse_expr_postfixa();
    }
}

/* binaria cum praecedentia (precedence climbing) */
static int praecedentia(int op)
{
    switch (op) {
    case T_STAR: case T_SLASH: case T_PERCENT: return 13;
    case T_PLUS: case T_MINUS: return 12;
    case T_LTLT: case T_GTGT: return 11;
    case T_LT: case T_GT: case T_LTEQ: case T_GTEQ: return 10;
    case T_EQEQ: case T_BANGEQ: return 9;
    case T_AMP: return 8;
    case T_CARET: return 7;
    case T_PIPE: return 6;
    case T_AMPAMP: return 5;
    case T_PIPEPIPE: return 4;
    default: return -1;
    }
}

static nodus_t *parse_expr_binaria(int min_prec)
{
    nodus_t *sinister = parse_expr_unaria();

    for (;;) {
        int op   = sig.genus;
        int prec = praecedentia(op);
        if (prec < min_prec)
            break;

        lex_proximum();
        nodus_t *dexter = parse_expr_binaria(prec + 1);

        nodus_t *n  = nodus_novus(N_BINOP);
        n->op       = op;
        n->sinister = sinister;
        n->dexter   = dexter;

        /* determinatio typi */
        if (typus_est_index(sinister->typus) || typus_est_index(dexter->typus)) {
            if (typus_est_index(sinister->typus))
                n->typus = sinister->typus;
            else
                n->typus = dexter->typus;
            /* comparatio -> int */
            if (
                op == T_EQEQ || op == T_BANGEQ || op == T_LT || op == T_GT ||
                op == T_LTEQ || op == T_GTEQ || op == T_AMPAMP || op == T_PIPEPIPE
            )
                n->typus = ty_int;
        } else if (sinister->typus && sinister->typus->magnitudo >= 8)
            n->typus = sinister->typus;
        else if (dexter->typus && dexter->typus->magnitudo >= 8)
            n->typus = dexter->typus;
        else
            n->typus = ty_int;

        /* operatores relationis -> int */
        if (
            op == T_EQEQ || op == T_BANGEQ || op == T_LT || op == T_GT ||
            op == T_LTEQ || op == T_GTEQ || op == T_AMPAMP || op == T_PIPEPIPE
        )
            n->typus = ty_int;

        sinister = n;
    }
    return sinister;
}

/* ternaria: a ? b : c */
static nodus_t *parse_expr_conditio(void)
{
    nodus_t *n = parse_expr_binaria(4); /* supra || */

    /* tracta || hic etiam */
    while (sig.genus == T_PIPEPIPE) {
        lex_proximum();
        nodus_t *dex = parse_expr_binaria(4);
        nodus_t *bin = nodus_novus(N_BINOP);
        bin->op = T_PIPEPIPE;
        bin->sinister = n;
        bin->dexter = dex;
        bin->typus = ty_int;
        n = bin;
    }

    if (sig.genus == T_QUESTION) {
        lex_proximum();
        nodus_t *ter  = nodus_novus(N_TERNARY);
        ter->sinister = n;
        ter->dexter   = parse_expr();
        expecta(T_COLON);
        ter->tertius = parse_expr_conditio();
        ter->typus   = ter->dexter->typus;
        return ter;
    }
    return n;
}

/* assignatio: a = b, a += b, etc. */
nodus_t *parse_expr_assign(void)
{
    nodus_t *n = parse_expr_conditio();

    int op = sig.genus;
    if (op == T_ASSIGN) {
        lex_proximum();
        nodus_t *a  = nodus_novus(N_ASSIGN);
        a->sinister = n;
        a->dexter   = parse_expr_assign();
        a->typus    = n->typus;
        return a;
    }
    if (
        op == T_PLUSEQ || op == T_MINUSEQ || op == T_STAREQ ||
        op == T_SLASHEQ || op == T_PERCENTEQ || op == T_AMPEQ ||
        op == T_PIPEEQ || op == T_CARETEQ || op == T_LTLTEQ ||
        op == T_GTGTEQ
    ) {
        lex_proximum();
        nodus_t *a  = nodus_novus(N_OPASSIGN);
        a->op       = op;
        a->sinister = n;
        a->dexter   = parse_expr_assign();
        a->typus    = n->typus;
        return a;
    }
    return n;
}

/* expressio plena (cum ,) */
static nodus_t *parse_expr(void)
{
    nodus_t *n = parse_expr_assign();
    while (sig.genus == T_COMMA) {
        lex_proximum();
        nodus_t *c = nodus_novus(N_COMMA_EXPR);
        c->sinister = n;
        c->dexter = parse_expr_assign();
        c->typus = c->dexter->typus;
        n = c;
    }
    return n;
}

/* ================================================================
 * sententiae (statements)
 * ================================================================ */

static nodus_t *parse_sententia(void)
{
    switch (sig.genus) {
    case T_LBRACE:
        return parse_blocum();

    case T_IF: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_IF);
            expecta(T_LPAREN);
            n->sinister = parse_expr();
            expecta(T_RPAREN);
            n->dexter = parse_sententia();
            if (congruet(T_ELSE))
                n->tertius = parse_sententia();
            return n;
        }

    case T_WHILE: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_WHILE);
            expecta(T_LPAREN);
            n->sinister = parse_expr();
            expecta(T_RPAREN);
            n->dexter = parse_sententia();
            return n;
        }

    case T_DO: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_DOWHILE);
            n->dexter  = parse_sententia();
            expecta(T_WHILE);
            expecta(T_LPAREN);
            n->sinister = parse_expr();
            expecta(T_RPAREN);
            expecta(T_SEMICOLON);
            return n;
        }

    case T_FOR: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_FOR);
            expecta(T_LPAREN);

            ambitus_intra();

        /* initium */
            if (sig.genus == T_SEMICOLON) {
                lex_proximum();
                n->sinister = NULL;
            } else if (est_specifier_typi()) {
                n->sinister = parse_declaratio(0);
            } else {
                n->sinister = nodus_novus(N_EXPR_STMT);
                n->sinister->sinister = parse_expr();
                expecta(T_SEMICOLON);
            }

        /* conditio */
            if (sig.genus == T_SEMICOLON) {
                n->dexter = NULL;
            } else {
                n->dexter = parse_expr();
            }
            expecta(T_SEMICOLON);

        /* incrementum */
            if (sig.genus == T_RPAREN) {
                n->tertius = NULL;
            } else {
                n->tertius = parse_expr();
            }
            expecta(T_RPAREN);

            n->quartus = parse_sententia();
            ambitus_exi();
            return n;
        }

    case T_SWITCH: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_SWITCH);
            expecta(T_LPAREN);
            n->sinister = parse_expr();
            expecta(T_RPAREN);
            n->dexter = parse_sententia();
            return n;
        }

    case T_CASE: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_CASE);
        /* valor constans */
            int negat = 0;
            if (sig.genus == T_MINUS) {
                negat = 1;
                lex_proximum();
            }
            if (sig.genus == T_NUM) {
                n->valor = sig.valor;
                if (negat)
                    n->valor = -n->valor;
                lex_proximum();
            } else if (sig.genus == T_CHARLIT) {
                n->valor = sig.valor;
                lex_proximum();
            } else if (sig.genus == T_IDENT) {
                symbolum_t *s = ambitus_quaere(sig.chorda, SYM_ENUM_CONST);
                if (s)
                    n->valor = s->valor_enum;
                lex_proximum();
            }
            expecta(T_COLON);
        /* non recursive — si proximus est case/default, dexter = NOP */
            if (
                sig.genus == T_CASE || sig.genus == T_DEFAULT ||
                sig.genus == T_RBRACE
            )
                n->dexter = nodus_novus(N_NOP);
            else
                n->dexter = parse_sententia();
            return n;
        }

    case T_DEFAULT: {
            lex_proximum();
            expecta(T_COLON);
            nodus_t *n = nodus_novus(N_DEFAULT);
            if (
                sig.genus == T_CASE || sig.genus == T_DEFAULT ||
                sig.genus == T_RBRACE
            )
                n->dexter = nodus_novus(N_NOP);
            else
                n->dexter = parse_sententia();
            return n;
        }

    case T_RETURN: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_RETURN);
            if (sig.genus != T_SEMICOLON)
                n->sinister = parse_expr();
            expecta(T_SEMICOLON);
            return n;
        }

    case T_BREAK:
        lex_proximum();
        expecta(T_SEMICOLON);
        return nodus_novus(N_BREAK);

    case T_CONTINUE:
        lex_proximum();
        expecta(T_SEMICOLON);
        return nodus_novus(N_CONTINUE);

    case T_GOTO: {
            lex_proximum();
            nodus_t *n = nodus_novus(N_GOTO);
            n->nomen   = strdup(sig.chorda);
            lex_proximum(); /* consume nomen */
            expecta(T_SEMICOLON);
            return n;
        }

    case T_SEMICOLON:
        lex_proximum();
        return nodus_novus(N_NOP);

    default:
        /* declaratio vel expressio */
        if (est_specifier_typi()) {
            return parse_declaratio(0);
        }
        /* label: sententia (goto label) */
        if (sig.genus == T_IDENT && lex_specta() == T_COLON) {
            nodus_t *n = nodus_novus(N_LABEL);
            n->nomen   = strdup(sig.chorda);
            lex_proximum(); /* consume nomen */
            lex_proximum(); /* consume : */
            /* sententia post label */
            n->sinister = parse_sententia();
            return n;
        }
        {
            nodus_t *n  = nodus_novus(N_EXPR_STMT);
            n->sinister = parse_expr();
            expecta(T_SEMICOLON);
            return n;
        }
    }
}

/* blocum { ... } */
static nodus_t *parse_blocum(void)
{
    expecta(T_LBRACE);
    ambitus_intra();

    int cap        = 1024;
    nodus_t **sent = calloc(cap, sizeof(nodus_t *));
    int num        = 0;

    while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
        nodus_t *s = parse_sententia();
        if (s) {
            if (num >= cap) {
                cap *= 2;
                sent = realloc(sent, cap * sizeof(nodus_t *));
            }
            sent[num++] = s;
        }
    }
    expecta(T_RBRACE);
    ambitus_exi();

    nodus_t *b = nodus_novus(N_BLOCK);
    b->membra  = calloc(num, sizeof(nodus_t *));
    memcpy(b->membra, sent, num * sizeof(nodus_t *));
    b->num_membrorum = num;
    return b;
}

/* ================================================================
 * declaratio (localis vel globalis)
 * ================================================================ */

static nodus_t *parse_declaratio(int est_globalis)
{
    int s_stat  = 0, s_ext = 0, s_td = 0;
    typus_t *tb = parse_specifiers(&s_stat, &s_ext, &s_td);

    /* typedef */
    if (s_td) {
        char nomen[256] = {0};
        typus_t *tt = parse_declarator(tb, nomen, 256);
        if (nomen[0]) {
            symbolum_t *s = ambitus_adde(nomen, SYM_TYPEDEF);
            s->typus      = tt;
            lex_registra_typedef(nomen);
        }
        expecta(T_SEMICOLON);
        return nodus_novus(N_NOP);
    }

    /* si solum specifier sine declarator (e.g., struct def) */
    if (sig.genus == T_SEMICOLON) {
        lex_proximum();
        return nodus_novus(N_NOP);
    }

    /* prima declaratio */
    nodus_t *blocum = nodus_novus(N_BLOCK);
    nodus_t *decls[256];
    int ndecl = 0;

    do {
        char nomen[256] = {0};
        typus_t *td = parse_declarator(tb, nomen, 256);

        /* est definitio functionis? */
        if (sig.genus == T_LPAREN && est_globalis) {
            ambitus_intra();
            symbolum_t **psyms = NULL;
            int nparams        = 0;
            typus_t *tf        = parse_parametros(td, &psyms, &nparams);

            symbolum_t *fs = ambitus_quaere(nomen, SYM_FUNC);
            if (!fs) {
                /* adde in ambitu parentis */
                ambitus_t *cur_salva = cur_ambitus;
                cur_ambitus = cur_ambitus->parens;
                fs = ambitus_adde(nomen, SYM_FUNC);
                cur_ambitus = cur_salva;
            }
            fs->typus        = tf;
            fs->est_globalis = 1;
            fs->est_staticus = s_stat;

            if (sig.genus == T_LBRACE) {
                nodus_t *fn      = nodus_novus(N_FUNC_DEF);
                fn->nomen        = strdup(nomen);
                fn->typus        = tf;
                fn->typus_decl   = tf;
                fn->est_staticus = s_stat;

                /* allocare offsets pro parametris */
                /* parametri: primi 8 in registris, salvati in acervo */
                for (int i = 0; i < nparams; i++) {
                    psyms[i]->offset       = -(16 + (i + 1) * 8);
                    psyms[i]->est_globalis = 0;
                }
                ambitus_currens()->proximus_offset = -(16 + nparams * 8);

                fn->dexter = parse_blocum();
                fn->sinister = nodus_novus(N_NOP);
                fn->sinister->valor = nparams;
                /* salva profunditatem acervi maximam */
                fn->op = -(ambitus_currens()->proximus_offset);

                ambitus_exi();
                free(psyms);
                return fn;
            }
            ambitus_exi();
            free(psyms);
            /* declaratio functionis sine corpore */
            expecta(T_SEMICOLON);
            return nodus_novus(N_NOP);
        }

        /* declaratio functionis (protypus) */
        if (sig.genus == T_LPAREN) {
            symbolum_t **psyms = NULL;
            int nparams        = 0;
            typus_t *tf        = parse_parametros(td, &psyms, &nparams);
            free(psyms);

            symbolum_t *fs   = ambitus_adde(nomen, SYM_FUNC);
            fs->typus        = tf;
            fs->est_globalis = est_globalis;
            fs->est_staticus = s_stat;

            if (sig.genus == T_COMMA) {
                lex_proximum();
                continue;
            }
            expecta(T_SEMICOLON);
            return nodus_novus(N_NOP);
        }

        /* declaratio variabilis */
        nodus_t *vd      = nodus_novus(N_VAR_DECL);
        vd->nomen        = strdup(nomen);
        vd->typus_decl   = td;
        vd->typus        = td;
        vd->est_staticus = s_stat;
        vd->est_externus = s_ext;
        if (ultima_vla_expr)
            vd->tertius = ultima_vla_expr; /* §6.7.5.2: VLA magnitudo */

        /* symbolum — salva in nodo */
        symbolum_t *vs = ambitus_adde(nomen, SYM_VAR);
        vs->typus = td;
        vs->est_globalis = est_globalis || s_stat || s_ext;
        vs->est_staticus = s_stat;
        vs->est_externus = s_ext;
        vd->sym = vs;

        int alloca_differtur = 0;
        if (!est_globalis && !s_stat && !s_ext) {
            /* variabilis localis — allocare in acervo */
            int mag = typus_magnitudo(td);
            if (mag == 0 && td->genus == TY_ARRAY && td->num_elementorum <= 0) {
                if (vd->tertius) {
                    /* §6.7.5.2: VLA — allocare 8 octeti pro indice ad tabulam */
                    int off = cur_ambitus->proximus_offset - 8;
                    off = off & ~7;
                    vs->offset = off;
                    cur_ambitus->proximus_offset = off;
                } else {
                    /* tabula sine magnitudine — differe allocatam usque ad initiale */
                    alloca_differtur = 1;
                }
            } else {
                if (mag == 0)
                    mag = 8;
                int col = typus_colineatio(td);
                if (col < 1)
                    col = 1;
                int off = cur_ambitus->proximus_offset - mag;
                off = off & ~(col - 1);
                vs->offset = off;
                cur_ambitus->proximus_offset = off;
            }
        }

        /* initiale */
        if (congruet(T_ASSIGN)) {
            if (sig.genus == T_LBRACE) {
                /* initializer list { expr, expr, ... } */
                lex_proximum();
                nodus_t *elems[4096];
                int nelem = 0;
                while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
                    /* praetermitte designatores: [N] = vel .nomen = */
                    if (sig.genus == T_LBRACKET) {
                        lex_proximum(); /* [ */
                        if (sig.genus == T_NUM)
                            lex_proximum(); /* N */
                        if (sig.genus == T_RBRACKET)
                            lex_proximum(); /* ] */
                        if (sig.genus == T_ASSIGN)
                            lex_proximum(); /* = */
                    }
                    if (sig.genus == T_DOT) {
                        lex_proximum(); /* . */
                        if (sig.genus == T_IDENT)
                            lex_proximum(); /* nomen */
                        if (sig.genus == T_ASSIGN)
                            lex_proximum(); /* = */
                    }
                    if (sig.genus == T_LBRACE) {
                        /* sub-initializer (e.g., {{1,2},{3,4}}) */
                        lex_proximum();
                        nodus_t *sub_elems[256];
                        int nsub = 0;
                        while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
                            /* praetermitte designatores in sub-init */
                            if (sig.genus == T_DOT) {
                                lex_proximum();
                                if (sig.genus == T_IDENT)
                                    lex_proximum();
                                if (sig.genus == T_ASSIGN)
                                    lex_proximum();
                            }
                            sub_elems[nsub++] = parse_expr_assign();
                            if (!congruet(T_COMMA))
                                break;
                        }
                        expecta(T_RBRACE);
                        for (int si = 0; si < nsub; si++)
                            elems[nelem++] = sub_elems[si];
                    } else {
                        elems[nelem++] = parse_expr_assign();
                    }
                    if (!congruet(T_COMMA))
                        break;
                }
                expecta(T_RBRACE);
                if (nelem > 0) {
                    vd->membra = calloc(nelem, sizeof(nodus_t *));
                    memcpy(vd->membra, elems, nelem * sizeof(nodus_t *));
                    vd->num_membrorum = nelem;
                }
                /* §6.7.8: si tabula sine magnitudine, defini ex initializatore */
                if (td->genus == TY_ARRAY && td->num_elementorum <= 0) {
                    int basis_mag = typus_magnitudo(td->basis);
                    if (basis_mag > 0 && td->basis->genus == TY_ARRAY) {
                        /* 2D: int[][3] — dividi elementa plana per inner */
                        int folium_mag = basis_mag;
                        typus_t *f     = td->basis;
                        while (f && f->genus == TY_ARRAY)
                            f = f->basis;
                        if (f)
                            folium_mag = typus_magnitudo(f);
                        if (folium_mag < 1)
                            folium_mag = 4;
                        int inner_n = basis_mag / folium_mag;
                        if (inner_n < 1)
                            inner_n = 1;
                        td->num_elementorum = (nelem + inner_n - 1) / inner_n;
                    } else {
                        td->num_elementorum = nelem;
                    }
                    td->magnitudo = td->num_elementorum * typus_magnitudo(td->basis);
                }
                /* allocatio differata — nunc allocamus */
                if (alloca_differtur) {
                    int mag = td->magnitudo;
                    if (mag == 0)
                        mag = 8;
                    int col = typus_colineatio(td);
                    if (col < 1)
                        col = 1;
                    int off = cur_ambitus->proximus_offset - mag;
                    off = off & ~(col - 1);
                    vs->offset = off;
                    cur_ambitus->proximus_offset = off;
                    alloca_differtur = 0;
                }
            } else {
                vd->sinister = parse_expr_assign();
            }
        }

        decls[ndecl++] = vd;

    } while (congruet(T_COMMA));

    expecta(T_SEMICOLON);

    if (ndecl == 1)
        return decls[0];
    blocum->membra = calloc(ndecl, sizeof(nodus_t *));
    memcpy(blocum->membra, decls, ndecl * sizeof(nodus_t *));
    blocum->num_membrorum = ndecl;
    return blocum;
}

/* ================================================================
 * translatio (unitas translationis — top level)
 * ================================================================ */

void parse_initia(void)
{
    nodi_vertex    = 0;
    typi_vertex    = 0;
    symbola_vertex = 0;
    ambitus_vertex = 0;
    cur_ambitus    = NULL;

    initia_typos();
    ambitus_intra(); /* ambitus globalis */

    /* lege primum signum */
    lex_proximum();
}

nodus_t *parse_translatio(void)
{
    int cap         = 1024;
    nodus_t **decls = calloc(cap, sizeof(nodus_t *));
    int ndecl       = 0;

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
    radix->membra  = calloc(ndecl, sizeof(nodus_t *));
    memcpy(radix->membra, decls, ndecl * sizeof(nodus_t *));
    radix->num_membrorum = ndecl;
    return radix;
}
