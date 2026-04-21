/*
 * parser.c — CCC parser descendens recursivus
 *
 * Producit arborem syntaxis abstractam (AST) ex signis.
 * Tractat declarationes, sententias, expressiones C99.
 */

#include "utilia.h"
#include "parser.h"
#include "fluat.h"

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

static ambitus_t *cur_ambitus = NULL;

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

static typus_t *parse_struct_vel_union(void);
static typus_t *parse_declarator(typus_t *basis, char *nomen, int max_nomen);
static typus_t *parse_parametros(
    typus_t *reditus,
    symbolum_t ***param_sym, int *num_param
);
static typus_t *parse_enum(void);
static nodus_t *parse_expr(void);
static nodus_t *parse_expr_assign(void);
static nodus_t *parse_expr_conditio(void);
static nodus_t *parse_sententia(void);
static nodus_t *parse_blocum(void);
static nodus_t *parse_declaratio(int est_globalis);
static int      parse_init_elementa(
    nodus_t **elems, int *nelem, int max,
    typus_t *t, int base_off
);

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
    typus_t        *t     = NULL;

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
        case T_INLINE:   lex_proximum(); continue; /* §6.7.4: praetermittitur */
        case T_RESTRICT: lex_proximum(); continue; /* §6.7.3.1: praetermittitur */
        case T_BOOL:     lex_proximum();           /* §6.2.5: _Bool — magnit. 1 oct. */
            t = ty_uchar;
            continue;
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
            t = ty_float;   /* §6.7.2: float — Annex F §F.2 */
            continue;
        case T_DOUBLE:   lex_proximum();
            t = ty_double;  /* §6.7.2: double — Annex F §F.2 */
            continue;
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
                if (!sym || !sym->typus)
                    erratum_ad(sig_linea, "typedef '%s' sine typo", sig.chorda);
                t = sym->typus;
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
            typus_t *tc      = typus_novus(t->genus);
            tc[0]   = *t;
            tc      ->est_constans = 1;
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
        ns         ->typus      = t;
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
            ns         ->typus      = t;
        }
    }

    if (sig.genus == T_LBRACE) {
        lex_proximum();
        t->membra = calloc(MAX_MEMBRA, sizeof(membrum_t));
        if (!t->membra)
            erratum("memoria exhausta");
        t->num_membrorum = 0;
        int offset       = 0;
        int cb_offset    = 0;  /* §6.7.2.1: offset ūnitātis campōrum bitōrum */
        int cb_positus   = 0;  /* positus bitī in ūnitāte currentī */

        while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
            int s_stat = 0, s_ext = 0;
            typus_t    *typ_basis = parse_specifiers(&s_stat, &s_ext, NULL);

            while (sig.genus != T_SEMICOLON && sig.genus != T_EOF) {
                /* §6.7.5: declarator plenus (incl. indicatorēs functionum) */
                char nom_mem[256] = {0};
                typus_t *typ_mem  = parse_declarator(typ_basis, nom_mem, 256);

                /* §6.7.2.1: campus bitōrum — : N */
                int campus_lat  = 0;  /* latitūdō in bitīs, 0 sī nōn campus */
                int campus_pos  = 0;  /* positus in ūnitāte repositōriā */
                int campus_sign = 0;
                if (sig.genus == T_COLON) {
                    lex_proximum();
                    if (sig.genus == T_NUM) {
                        campus_lat = (int)sig.valor;
                        lex_proximum();
                    } else {
                        erratum_ad(
                            sig_linea,
                            "latitūdō campī bitōrum expectābātur"
                        );
                    }
                    campus_sign = !typ_basis->est_sine_signo;
                }

                /* colineatio — §6.7.2.1 */
                if (campus_lat > 0 && est_struct) {
                    /* campus bitōrum — impōne in ūnitātem 4-octētōrum */
                    /* novam ūnitātem incipiō sī prīmus campus vel plēna */
                    if (
                        cb_positus == 0 || cb_positus + campus_lat > 32
                        || offset != cb_offset
                    ) {
                        int col    = 4;
                        offset     = (offset + col - 1) & ~(col - 1);
                        cb_offset  = offset;
                        cb_positus = 0;
                    }
                    campus_pos = cb_positus;
                    cb_positus += campus_lat;
                    /* membrum */
                    if (t->num_membrorum >= MAX_MEMBRA)
                        erratum_ad(
                            sig_linea,
                            "nimis multa membra in structūrā"
                        );
                    membrum_t *mem = &t->membra[t->num_membrorum++];
                    strncpy(mem->nomen, nom_mem, 127);
                    mem ->typus           = ty_int;
                    mem ->offset          = cb_offset;
                    mem ->campus_bitorum  = campus_lat;
                    mem ->campus_positus  = campus_pos;
                    mem ->campus_signatus = campus_sign;
                    /* nōn incrementāmus offset — proximī campī eundem
                     * locum possunt occupāre */
                    if (cb_positus >= 32) {
                        offset += 4;
                        cb_positus = 0;
                    }
                } else {
                    int col = typus_colineatio(typ_mem);
                    if (est_struct && col > 0)
                        offset = (offset + col - 1) & ~(col - 1);

                    if (t->num_membrorum >= MAX_MEMBRA)
                        erratum_ad(
                            sig_linea,
                            "nimis multa membra in structūrā"
                        );
                    membrum_t *mem = &t->membra[t->num_membrorum++];
                    strncpy(mem->nomen, nom_mem, 127);
                    mem ->typus           = typ_mem;
                    mem ->offset          = est_struct ? offset : 0;
                    mem ->campus_bitorum  = 0;
                    mem ->campus_positus  = 0;
                    mem ->campus_signatus = 0;
                    if (est_struct)
                        offset += typus_magnitudo(typ_mem);
                    else if (typus_magnitudo(typ_mem) > offset)
                        offset = typus_magnitudo(typ_mem);
                }

                if (sig.genus == T_COMMA)
                    lex_proximum();
                else
                    break;
            }
            expecta(T_SEMICOLON);
        }
        /* §6.7.2.1: claudī ultima ūnitātem campōrum bitōrum */
        if (cb_positus > 0)
            offset = cb_offset + 4;
        expecta(T_RBRACE);

        int max_col = 1;
        for (int i = 0; i < t->num_membrorum; i++) {
            int c = typus_colineatio(t->membra[i].typus);
            if (c > max_col)
                max_col = c;
        }
        t ->colineatio    = max_col;
        t ->magnitudo     = (offset + max_col - 1) & ~(max_col - 1);
        t ->est_perfectum = 1;
    }

    return t;
}

/* ================================================================
 * evaluator expressionis constantis — §6.6
 *
 * Percurrit AST et computat valorem integer tempore compilationis.
 * Vocatur pro enumeratoribus et casibus switch.
 * ================================================================ */

static int constans_est(nodus_t *n)
{
    if (!n)
        return 0;
    switch (n->genus) {
    case N_NUM:
        return 1;
    case N_BINOP:
        return constans_est(n->sinister) && constans_est(n->dexter);
    case N_UNOP:
        return constans_est(n->sinister);
    case N_TERNARY:
        return constans_est(n->sinister) && constans_est(n->dexter)
            && constans_est(n->tertius);
    case N_CAST:
        return constans_est(n->sinister);
    default:
        return 0;
    }
}

long evalua_constans(nodus_t *n)
{
    if (!n)
        erratum("evalua_constans: nodus nullus");

    switch (n->genus) {
    case N_NUM:
        return n->valor;
    case N_BINOP: {
            long s = evalua_constans(n->sinister);
            long d = evalua_constans(n->dexter);
            switch (n->op) {
            case T_PLUS:     return s + d;
            case T_MINUS:    return s - d;
            case T_STAR:     return s * d;
            case T_SLASH:
                if (d == 0)
                    erratum("divisio per zerum in constante");
                return s / d;
            case T_PERCENT:
                if (d == 0)
                    erratum("modulus per zerum in constante");
                return s % d;
            case T_LTLT:     return s << d;
            case T_GTGT:     return s >> d;
            case T_AMP:      return s & d;
            case T_PIPE:     return s | d;
            case T_CARET:    return s ^ d;
            case T_LT:       return s < d;
            case T_GT:       return s > d;
            case T_LTEQ:     return s <= d;
            case T_GTEQ:     return s >= d;
            case T_EQEQ:     return s == d;
            case T_BANGEQ:   return s != d;
            case T_AMPAMP:   return s && d;
            case T_PIPEPIPE: return s || d;
            default:
                erratum("operator non constans in enumeratore: %d", n->op);
            }
        }
    case N_UNOP:
        switch (n->op) {
        case T_MINUS: return -evalua_constans(n->sinister);
        case T_TILDE: return ~evalua_constans(n->sinister);
        case T_BANG:  return !evalua_constans(n->sinister);
        default:
            erratum("operator unarius non constans: %d", n->op);
        }
    case N_TERNARY: {
            long conditio = evalua_constans(n->sinister);
            return conditio ? evalua_constans(n->dexter)
                : evalua_constans(n->tertius);
        }
    case N_CAST:
        return evalua_constans(n->sinister);
    default:
        erratum("expressio non constans in enumeratore (genus %d)", n->genus);
    }
    return 0;
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
    t       ->magnitudo  = 4;
    t       ->colineatio = 4;
    strncpy(t->nomen_tag, nomen_tag, 127);

    if (nomen_tag[0]) {
        symbolum_t *es = ambitus_quaere(nomen_tag, SYM_ENUM_TAG);
        if (!es) {
            es = ambitus_adde(nomen_tag, SYM_ENUM_TAG);
            es ->typus = t;
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
                /* §6.6: expressio constans plena */
                nodus_t *expr = parse_expr_conditio();
                val     = (int)evalua_constans(expr);
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

    /* indicis: * * * — §6.7.5.1 */
    int num_stellarum = 0;
    while (sig.genus == T_STAR) {
        num_stellarum++;
        lex_proximum();
        /* praetermitte const/volatile/restrict post * — §6.7.3 */
        while (
            sig.genus == T_CONST || sig.genus == T_VOLATILE
            || sig.genus == T_RESTRICT
        )
            lex_proximum();
    }

    /* nomen (optionale) */
    nomen[0] = '\0';
    if (sig.genus == T_LPAREN && lex_specta() == T_STAR) {
        /* indicis ad functionem: float (*f)(args) — vel cum plūribus * */
        lex_proximum(); /* ( */
        int fp_stellae = 0;
        while (sig.genus == T_STAR) {
            fp_stellae++;
            lex_proximum();
            while (
                sig.genus == T_CONST || sig.genus == T_VOLATILE
                || sig.genus == T_RESTRICT
            )
                lex_proximum();
        }
        if (sig.genus == T_IDENT) {
            strncpy(nomen, sig.chorda, max_nomen - 1);
            lex_proximum();
        }
        expecta(T_RPAREN);
        /* reditus = basis cum indicibus exteriōribus applicātīs */
        typus_t *reditus = t;
        for (int i = 0; i < num_stellarum; i++)
            reditus = typus_indicem(reditus);
        /* parse params → TY_FUNC cum parametris propriīs */
        typus_t *tfunc = parse_parametros(reditus, NULL, NULL);
        /* applica indices internōs (stellae intra parenthesēs) */
        typus_t *tp = tfunc;
        for (int i = 0; i < fp_stellae; i++)
            tp = typus_indicem(tp);
        return tp;
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
            int num = 0;
            nodus_t *ve = NULL;
            /* §6.7.5.3: praetermitte 'static' in [static N] */
            if (sig.genus == T_STATIC)
                lex_proximum();
            if (sig.genus == T_RBRACKET) {
                /* [] — dimensio ignota */
            } else {
                nodus_t *e = parse_expr_assign();
                if (e && e->genus == N_NUM) {
                    num = (int)e->valor;
                } else if (e && constans_est(e)) {
                    /* §6.7.5.2: arīes cum dīmēnsiōne cōnstantī */
                    num = (int)evalua_constans(e);
                } else {
                    ve = e; /* VLA — expressio non constans */
                }
            }
            expecta(T_RBRACKET);
            if (ndims >= 64)
                erratum_ad(sig_linea, "nimis multae dimensiones tabulae");
            dims[ndims]     = num;
            vla_expr[ndims] = ve;
            ndims++;
        }
        for (int i = ndims - 1; i >= 0; i--) {
            t = typus_tabulam(t, dims[i]);
            t ->vla_dim = (struct nodus *)vla_expr[i];
        }
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
    typus_t *tf    = typus_novus(TY_FUNC);
    tf      ->reditus    = reditus;
    tf      ->magnitudo  = 8;
    tf      ->colineatio = 8;
    tf      ->parametri  = calloc(MAX_PARAM, sizeof(typus_t *));
    if (!tf->parametri)
        erratum("memoria exhausta");
    tf->nomina_param = calloc(MAX_PARAM, sizeof(char *));
    if (!tf->nomina_param)
        erratum("memoria exhausta");
    tf ->num_parametrorum = 0;
    tf ->est_variadicus   = 0;

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

        int s_stat = 0, s_ext = 0;
        typus_t    *tb = parse_specifiers(&s_stat, &s_ext, NULL);
        char nomen[256] = {0};
        typus_t *tp = parse_declarator(tb, nomen, 256);

        /* §6.7.5.3: tabulae in parametris adaptantur ad indicem */
        if (tp->genus == TY_ARRAY)
            tp = typus_indicem(tp->basis);

        if (tf->num_parametrorum >= MAX_PARAM)
            erratum_ad(sig_linea, "nimis multi parametri");
        tf ->parametri[tf->num_parametrorum]    = tp;
        tf ->nomina_param[tf->num_parametrorum] = nomen[0] ? strdup(nomen) : NULL;

        /* Addere ad scopum sōlum sī vocātor expectat symbola (i.e. est
         * dēfīnītiō/dēclārātiō functiōnis, nōn declārātor fn-pointārii). */
        if (nomen[0] && param_sym) {
            symbolum_t *ps = ambitus_adde(nomen, SYM_VAR);
            ps->typus = tp;
            ps->est_parametrus = 1;
            param_syms[np++] = ps;
        }
        tf->num_parametrorum++;

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
    /* §6.4.4.1: typus constantis integrae ex amplitudine valoris */
    case T_NUM:
        n = nodus_novus(N_NUM);
        n ->valor = sig.valor;
        n ->typus = ty_int;
        if (sig.valor > 0x7FFFFFFF || sig.valor < -0x7FFFFFFF)
            n->typus = ty_long;
        lex_proximum();
        return n;

    /* §6.4.4.2: constans fluitans — sine suffixo typus est double */
    case T_NUM_FLUAT:
        n = nodus_novus(N_NUM_FLUAT);
        n ->valor_f = sig.valor_f;
        n ->typus   = ty_double;
        lex_proximum();
        return n;

    case T_CHARLIT:
        n = nodus_novus(N_NUM);
        n ->valor = sig.valor;
        n ->typus = ty_int;
        lex_proximum();
        return n;

    /* §6.4.5, §6.3.2.1: chorda litteralis — index ad char */
    case T_STR:
        n = nodus_novus(N_STR);
        n ->chorda = malloc(sig.lon_chordae + 1);
        memcpy(n->chorda, sig.chorda, sig.lon_chordae + 1);
        n ->lon_chordae = sig.lon_chordae;
        n ->typus       = typus_indicem(ty_char);
        lex_proximum();
        return n;

    case T_IDENT: {
            char nomen[256];
            strncpy(nomen, sig.chorda, 255);
            nomen[255] = '\0';
            lex_proximum();

            /* va_start(ap, ultimum) */
            if (strcmp(nomen, "va_start") == 0) {
                expecta(T_LPAREN);
                n = nodus_novus(N_VA_START);
                n ->sinister = parse_expr_assign();
                expecta(T_COMMA);
                parse_expr_assign();  /* ultimum — praetermittitur */
                expecta(T_RPAREN);
                n->typus = ty_void;
                return n;
            }

            /* va_end(ap) */
            if (strcmp(nomen, "va_end") == 0) {
                expecta(T_LPAREN);
                n = nodus_novus(N_VA_END);
                n ->sinister = parse_expr_assign();
                expecta(T_RPAREN);
                n->typus = ty_void;
                return n;
            }

            /* va_arg(ap, typus) */
            if (strcmp(nomen, "va_arg") == 0) {
                expecta(T_LPAREN);
                n = nodus_novus(N_VA_ARG);
                n ->sinister = parse_expr_assign();
                expecta(T_COMMA);
                int s_stat = 0, s_ext = 0;
                typus_t    *tb = parse_specifiers(&s_stat, &s_ext, NULL);
                char nom[256] = {0};
                n ->typus_decl = parse_declarator(tb, nom, 256);
                n ->typus      = n->typus_decl;
                expecta(T_RPAREN);
                return n;
            }

            n = nodus_novus(N_IDENT);
            n ->nomen = strdup(nomen);

        /* quaere symbolum et salva in nodo */
            symbolum_t *s = ambitus_quaere_omnes(nomen);
            if (s) {
                if (s->genus == SYM_ENUM_CONST) {
                    n ->genus = N_NUM;
                    n ->valor = s->valor_enum;
                    n ->typus = ty_int;
                } else {
                    n ->typus = s->typus;
                    n ->sym   = s;
                }
            } else {
                erratum_ad(n->linea, "symbolum non declaratum: '%s'", nomen);
            }
            return n;
        }

    case T_LPAREN: {
            lex_proximum();
        /* est cast? */
            if (est_specifier_typi()) {
                int s_stat = 0, s_ext = 0;
                typus_t    *tb = parse_specifiers(&s_stat, &s_ext, NULL);
                char nom[256] = {0};
                typus_t *ct = parse_declarator(tb, nom, 256);
                expecta(T_RPAREN);

                /* §6.7.8 + §6.5.2.5: compound literal: (typename){ ... }
                 * — ūtimur eandem logicam initializer sicut in declaratione */
                if (sig.genus == T_LBRACE) {
                    nodus_t *elems[256];
                    int nel = 0;
                    parse_init_elementa(elems, &nel, 256, ct, 0);
                    n = nodus_novus(N_BLOCK);
                    n ->typus  = ct;
                    n ->membra = calloc(nel, sizeof(nodus_t *));
                    memcpy(n->membra, elems, nel * sizeof(nodus_t *));
                    n->num_membrorum = nel;
                    return n;
                }

                n = nodus_novus(N_CAST);
                n ->typus_decl = ct;
                n ->typus = ct;
                n ->sinister = parse_expr_unaria();
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
                    int s_stat = 0;
                    int s_ext  = 0;
                    typus_t    *tb = parse_specifiers(&s_stat, &s_ext, NULL);
                    char nom[256] = {0};
                    typus_t *st   = parse_declarator(tb, nom, 256);

                    expecta(T_RPAREN);
                    n = nodus_novus(N_NUM);
                    n ->valor = typus_magnitudo(st);
                    n ->typus = ty_long;
                    return n;
                }

                /* sizeof(expr) — §6.5.3.4p2 */
                nodus_t *e = parse_expr();

                expecta(T_RPAREN);
                /* §6.5.3.4p2: si VLA, sizeof computatur tempore executionis */
                if (
                    e->genus == N_IDENT && e->sym && e->sym->vla_expr
                    && e->typus && e->typus->genus == TY_ARRAY
                    && e->typus->magnitudo <= 0
                ) {
                    /* sizeof(vla) = dim * sizeof(elementum) */
                    typus_t *basis = e->typus->basis;
                    int basis_mag = basis ? typus_magnitudo(basis) : 1;
                    nodus_t *mag_nod = nodus_novus(N_NUM);
                    mag_nod->valor = basis_mag;
                    mag_nod->typus = ty_long;
                    n = nodus_novus(N_BINOP);
                    n->op = T_STAR;
                    n->sinister = (nodus_t *)e->sym->vla_expr;
                    n->dexter = mag_nod;
                    n->typus = ty_long;
                    return n;
                }
                n = nodus_novus(N_NUM);
                n ->valor = e->typus ? typus_magnitudo(e->typus) : 0;
                n ->typus = ty_long;
                return n;
            }

            /* sizeof expr */
            nodus_t *e = parse_expr_unaria();
            n       = nodus_novus(N_NUM);
            n       ->valor = e->typus ? typus_magnitudo(e->typus) : 0;
            n       ->typus = ty_long;
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
            idx     ->sinister = n;
            idx     ->dexter   = parse_expr();
            expecta(T_RBRACKET);
            if (n->typus)
                idx->typus = typus_basis_indicis(n->typus);
            n = idx;
        } else if (sig.genus == T_DOT) {
            lex_proximum();
            int mem_linea = sig_linea;
            nodus_t *mem  = nodus_novus(N_MEMBER);
            mem->sinister = n;
            mem->nomen    = strdup(sig.chorda);
            expecta(T_IDENT);
            /* typus membri */
            if (n->typus && n->typus->genus == TY_STRUCT) {
                int invenit = 0;
                for (int i = 0; i < n->typus->num_membrorum; i++) {
                    if (strcmp(n->typus->membra[i].nomen, mem->nomen) == 0) {
                        mem     ->typus = n->typus->membra[i].typus;
                        invenit = 1;
                        break;
                    }
                }
                if (!invenit) {
                    erratum_ad(
                        mem_linea,
                        "structura '%s' campum '%s' non habet",
                        n->typus->nomen_tag[0] ?
                        n->typus->nomen_tag : "(anonyma)",
                        mem->nomen
                    );
                }
            } else if (n->typus) {
                erratum_ad(
                    mem_linea,
                    "'.' applicatus non-structurae (campus '%s')",
                    mem->nomen
                );
            }
            n = mem;
        } else if (sig.genus == T_ARROW) {
            lex_proximum();
            int mem_linea = sig_linea;
            nodus_t *mem  = nodus_novus(N_ARROW);
            mem->sinister = n;
            mem->nomen    = strdup(sig.chorda);
            expecta(T_IDENT);
            if (
                n->typus && n->typus->genus == TY_PTR && n->typus->basis &&
                n->typus->basis->genus == TY_STRUCT
            ) {
                typus_t     *st = n->typus->basis;
                int invenit = 0;
                for (int i = 0; i < st->num_membrorum; i++) {
                    if (strcmp(st->membra[i].nomen, mem->nomen) == 0) {
                        mem     ->typus = st->membra[i].typus;
                        invenit = 1;
                        break;
                    }
                }
                if (!invenit) {
                    erratum_ad(
                        mem_linea,
                        "structura '%s' campum '%s' non habet",
                        st->nomen_tag[0] ? st->nomen_tag : "(anonyma)",
                        mem->nomen
                    );
                }
            } else if (n->typus) {
                erratum_ad(
                    mem_linea,
                    "'->' applicatus non-indici-structurae (campus '%s')",
                    mem->nomen
                );
            }
            n = mem;
        } else if (sig.genus == T_LPAREN) {
            /* vocatio functionis */
            lex_proximum();
            nodus_t *vocatio  = nodus_novus(N_CALL);
            vocatio ->sinister = n;

            nodus_t *args[MAX_PARAM];
            int nargs         = 0;
            int vocatio_linea = sig_linea;
            while (sig.genus != T_RPAREN && sig.genus != T_EOF) {
                if (nargs > 0)
                    expecta(T_COMMA);
                args[nargs++] = parse_expr_assign();
            }
            expecta(T_RPAREN);

            /* §6.5.2.2¶2: cōnstrāinēs prōtotypī — numerus et typī
             * argūmentōrum cum parametrīs congruere dēbent.
             * §6.5.2.2¶7: argūmenta convertuntur, velut assīgnātiōne,
             * ad typōs parametrōrum. */
            typus_t *pft = NULL;
            if (n->typus && n->typus->genus == TY_FUNC)
                pft = n->typus;
            else if (
                n->typus && n->typus->genus == TY_PTR &&
                n->typus->basis && n->typus->basis->genus == TY_FUNC
            )
                pft = n->typus->basis;

            const char *nomen_func = (n->genus == N_IDENT) ? n->nomen : "(functio)";
            if (pft && pft->num_parametrorum > 0) {
                if (!pft->est_variadicus && nargs != pft->num_parametrorum)
                    erratum_ad(
                        vocatio_linea,
                        "vocatiō '%s': %d argūmenta data, %d expectāta",
                        nomen_func, nargs, pft->num_parametrorum
                    );
                if (pft->est_variadicus && nargs < pft->num_parametrorum)
                    erratum_ad(
                        vocatio_linea,
                        "vocatiō '%s': %d argūmenta data, saltem %d expectāta",
                        nomen_func, nargs, pft->num_parametrorum
                    );
                for (int i = 0; i < pft->num_parametrorum && i < nargs; i++) {
                    typus_t *pt = pft->parametri[i];
                    typus_t *at = args[i]->typus;
                    if (!pt || !at)
                        continue;
                    int pt_arith = typus_est_arithmeticus(pt);
                    int at_arith = typus_est_arithmeticus(at);
                    int pt_idx   = typus_est_index(pt);
                    int at_idx   = typus_est_index(at);
                    int compat   = 0;
                    if (pt_arith && at_arith)
                        compat = 1;
                    else if (pt_idx && at_idx)
                        compat = 1;
                    else if (pt_idx && typus_est_integer(at))
                        compat = 1;  /* constans integra (e.g. 0 ad NULL) */
                    else if (
                        pt->genus == TY_STRUCT && at->genus == TY_STRUCT
                        && pt->magnitudo == at->magnitudo
                    )
                        compat = 1;
                    else if (pt->genus == at->genus)
                        compat = 1;
                    /* §6.3.2.1¶4: functiō in vocātiōne decidit in
                     * indicem ad functiōnem */
                    else if (pt_idx && at->genus == TY_FUNC)
                        compat = 1;
                    if (!compat)
                        erratum_ad(
                            vocatio_linea,
                            "vocatiō '%s': typus argūmentī %d incompatibilis cum parametrō",
                            nomen_func, i + 1
                        );
                    /* §6.5.2.2¶7: inserta N_CAST sī typī dissimilēs */
                    int eq = (
                        pt->genus == at->genus
                        && pt->magnitudo == at->magnitudo
                        && pt->est_sine_signo == at->est_sine_signo
                    );
                    if (!eq && pt_arith && at_arith) {
                        nodus_t *c   = nodus_novus(N_CAST);
                        c       ->typus_decl = pt;
                        c       ->typus      = pt;
                        c       ->sinister   = args[i];
                        args[i] = c;
                    }
                }
            }

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
            post    ->op = T_PLUSPLUS;
            post    ->sinister = n;
            post    ->typus = n->typus;
            n       = post;
        } else if (sig.genus == T_MINUSMINUS) {
            lex_proximum();
            nodus_t *post = nodus_novus(N_POSTOP);
            post    ->op = T_MINUSMINUS;
            post    ->sinister = n;
            post    ->typus = n->typus;
            n       = post;
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
    case T_PLUS: {
            /* §6.5.3.3: unary + — valor operandi (promoti); nihil ad opus */
            lex_proximum();
            return parse_expr_unaria();
        }
    case T_MINUS: case T_TILDE: case T_BANG: {
            int op = sig.genus;
            lex_proximum();
            nodus_t *n  = nodus_novus(N_UNOP);
            n       ->op       = op;
            n       ->sinister = parse_expr_unaria();
            n       ->typus    = n->sinister->typus;
            return n;
        }
    case T_STAR: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_DEREF);
            n       ->sinister = parse_expr_unaria();
            if (n->sinister->typus)
                n->typus = typus_basis_indicis(n->sinister->typus);
            else
                n->typus = ty_int;
            return n;
        }
    case T_AMP: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_ADDR);
            n       ->sinister = parse_expr_unaria();
            n       ->typus    = typus_indicem(n->sinister->typus ? n->sinister->typus : ty_int);
            return n;
        }
    case T_PLUSPLUS: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_UNOP);
            n       ->op       = T_PLUSPLUS;
            n       ->sinister = parse_expr_unaria();
            n       ->typus    = n->sinister->typus;
            return n;
        }
    case T_MINUSMINUS: {
            lex_proximum();
            nodus_t *n  = nodus_novus(N_UNOP);
            n       ->op       = T_MINUSMINUS;
            n       ->sinister = parse_expr_unaria();
            n       ->typus    = n->sinister->typus;
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
        n       ->op       = op;
        n       ->sinister = sinister;
        n       ->dexter   = dexter;

        /* determinatio typi — §6.3.1.8: conversiones arithmeticae usitae */
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
        } else if (typus_est_fluat(sinister->typus) || typus_est_fluat(dexter->typus)) {
            /* §6.3.1.8: si unus fluitans → typus communis fluitans */
            n->typus = typus_communis_fluat(sinister->typus, dexter->typus);
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
        bin     ->op = T_PIPEPIPE;
        bin     ->sinister = n;
        bin     ->dexter = dex;
        bin     ->typus = ty_int;
        n       = bin;
    }

    if (sig.genus == T_QUESTION) {
        lex_proximum();
        nodus_t *ter  = nodus_novus(N_TERNARY);
        ter     ->sinister = n;
        ter     ->dexter   = parse_expr();
        expecta(T_COLON);
        ter ->tertius = parse_expr_conditio();
        ter ->typus   = ter->dexter->typus;
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
        a       ->sinister = n;
        a       ->dexter   = parse_expr_assign();
        a       ->typus    = n->typus;
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
        a       ->op       = op;
        a       ->sinister = n;
        a       ->dexter   = parse_expr_assign();
        a       ->typus    = n->typus;
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
        c       ->sinister = n;
        c       ->dexter = parse_expr_assign();
        c       ->typus = c->dexter->typus;
        n       = c;
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
            n       ->dexter  = parse_sententia();
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
                n ->sinister = nodus_novus(N_EXPR_STMT);
                n ->sinister->sinister = parse_expr();
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
                if (!s)
                    erratum_ad(
                        sig_linea,
                        "constans ignota in case: '%s'", sig.chorda
                    );
                n->valor = s->valor_enum;
                lex_proximum();
            } else {
                erratum_ad(
                    sig_linea,
                    "valor constans expectabatur in case"
                );
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
            n       ->nomen   = strdup(sig.chorda);
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
            n       ->nomen   = strdup(sig.chorda);
            lex_proximum(); /* consume nomen */
            lex_proximum(); /* consume : */
            /* sententia post label */
            n->sinister = parse_sententia();
            return n;
        }
        {
            nodus_t *n  = nodus_novus(N_EXPR_STMT);
            n       ->sinister = parse_expr();
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

    int cap = 1024;
    nodus_t **sent = calloc(cap, sizeof(nodus_t *));
    int num = 0;

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
    b       ->membra  = calloc(num, sizeof(nodus_t *));
    memcpy(b->membra, sent, num * sizeof(nodus_t *));
    b->num_membrorum = num;
    return b;
}

/* §6.7.8: initializer list parse. Computat offset per elementum
 * utens typō targetī et designatoribus. Reddit numerum elementōrum
 * huius līstae (nōn plānōrum scālārium, sed elementōrum directōrum
 * quae comma dēlimitat — prō arīetibus strūctūrārum ūtile ut
 * dimēnsiō computētur). */
static int parse_init_elementa(
    nodus_t **elems, int *nelem, int max,
    typus_t *t, int base_off
) {
    expecta(T_LBRACE);
    int cur_off = base_off;
    int cur_idx = 0; /* index elementī currentīs intra t */
    int max_idx = 0; /* maximum index+1 vīsum — prō dēsignātōribus [N]= */
    while (sig.genus != T_RBRACE && sig.genus != T_EOF) {
        /* §6.7.8: designatores possunt encatenari: [N].membrum[K] = val */
        typus_t         *sub_t  = t;   /* typus currentis positionis */
        int sub_off     = cur_off;
        int habet_desig = 0;
        int sub_bitpos  = 0;   /* §6.7.2.1: campus bitōrum — 0 sī nōn */
        int sub_bitwd   = 0;
        while (sig.genus == T_LBRACKET || sig.genus == T_DOT) {
            habet_desig = 1;
            if (sig.genus == T_LBRACKET) {
                lex_proximum();
                nodus_t  *ie = parse_expr_conditio();
                long idx = evalua_constans(ie);
                expecta(T_RBRACKET);
                if (!sub_t || sub_t->genus != TY_ARRAY)
                    erratum_ad(
                        sig_linea,
                        "designator [N]= sed typus non est aries"
                    );
                int elem_sz = typus_magnitudo(sub_t->basis);
                if (elem_sz < 1)
                    erratum_ad(sig_linea, "elementum arietis magnitudinis zero");
                sub_off += (int)idx * elem_sz;
                sub_t   = sub_t->basis;
                cur_idx = (int)idx; /* proximum elementum sequitur */
            } else {
                lex_proximum();
                if (sig.genus != T_IDENT)
                    erratum_ad(sig_linea, "nomen expectatum post '.'");
                if (!sub_t || sub_t->genus != TY_STRUCT)
                    erratum_ad(
                        sig_linea,
                        "designator .%s= sed typus non est structura",
                        sig.chorda
                    );
                int mi_inv = -1;
                for (int mi = 0; mi < sub_t->num_membrorum; mi++) {
                    if (strcmp(sub_t->membra[mi].nomen, sig.chorda) == 0) {
                        mi_inv = mi;
                        break;
                    }
                }
                if (mi_inv < 0)
                    erratum_ad(
                        sig_linea,
                        "membrum '%s' in structura non inventum", sig.chorda
                    );
                sub_off += sub_t->membra[mi_inv].offset;
                sub_bitpos = sub_t->membra[mi_inv].campus_positus;
                sub_bitwd  = sub_t->membra[mi_inv].campus_bitorum;
                sub_t      = sub_t->membra[mi_inv].typus;
                lex_proximum();
            }
        }
        if (sig.genus == T_ASSIGN)
            lex_proximum();

        /* si designator adest, utere sub_off/sub_t; aliter naturalis positio */
        typus_t         *elem_t = habet_desig ? sub_t : NULL;
        int elem_off    = habet_desig ? sub_off : -1;
        int elem_bitpos = habet_desig ? sub_bitpos : 0;
        int elem_bitwd  = habet_desig ? sub_bitwd  : 0;
        if (!habet_desig && t) {
            /* computa naturalem positionem ex t et cur_idx */
            if (t->genus == TY_ARRAY && t->basis) {
                int elem_sz = typus_magnitudo(t->basis);
                if (elem_sz < 1)
                    erratum_ad(sig_linea, "elementum arietis magnitudinis zero");
                elem_off = cur_off + cur_idx * elem_sz;
                elem_t   = t->basis;
            } else if (
                t->genus == TY_STRUCT && t->membra
                && cur_idx < t->num_membrorum
            ) {
                elem_off    = cur_off + t->membra[cur_idx].offset;
                elem_t      = t->membra[cur_idx].typus;
                elem_bitpos = t->membra[cur_idx].campus_positus;
                elem_bitwd  = t->membra[cur_idx].campus_bitorum;
            }
        }

        if (sig.genus == T_LBRACE) {
            parse_init_elementa(elems, nelem, max, elem_t, elem_off);
        } else {
            if (*nelem >= max)
                erratum_ad(
                    sig_linea,
                    "nimis multa elementa in initializatore (max %d)", max
                );
            nodus_t *e       = parse_expr_assign();
            e       ->init_offset   = elem_off;
            e       ->init_bitpos   = elem_bitpos;
            e       ->init_bitwidth = elem_bitwd;
            /* §6.7.8: computa magnitudinem scripturae per typum targetī.
             * char[N]=chorda scribit N bytes; aliter scalar magnitudo. */
            if (
                elem_t && elem_t->genus == TY_ARRAY && elem_t->basis
                && (
                    elem_t->basis->genus == TY_CHAR
                    || elem_t->basis->genus == TY_UCHAR
                )
            )
                e->init_size = typus_magnitudo(elem_t);
            else if (elem_t) {
                typus_t *fol = elem_t;
                while (
                    fol && (
                        fol->genus == TY_ARRAY
                        || fol->genus == TY_STRUCT
                    )
                ) {
                    if (fol->genus == TY_ARRAY && fol->basis)
                        fol = fol->basis;
                    else if (
                        fol->genus == TY_STRUCT && fol->membra
                        && fol->num_membrorum > 0
                    )
                        fol = fol->membra[0].typus;
                    else
                        break;
                }
                e->init_size = typus_magnitudo(fol);
                if (e->init_size < 1)
                    e->init_size = 8;
            } else {
                e->init_size = 0;
            }
            elems[(*nelem)++] = e;
        }
        cur_idx++;
        if (cur_idx > max_idx)
            max_idx = cur_idx;
        if (!congruet(T_COMMA))
            break;
    }
    expecta(T_RBRACE);
    return max_idx;
}

/* ================================================================
 * declaratio (localis vel globalis)
 * ================================================================ */

static nodus_t *parse_declaratio(int est_globalis)
{
    int s_stat = 0, s_ext = 0, s_td = 0;
    typus_t    *tb = parse_specifiers(&s_stat, &s_ext, &s_td);

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
            symbolum_t  **psyms = NULL;
            int nparams = 0;
            typus_t     *tf        = parse_parametros(td, &psyms, &nparams);

            symbolum_t *fs = ambitus_quaere(nomen, SYM_FUNC);
            if (!fs) {
                /* adde in ambitu parentis */
                ambitus_t *cur_salva = cur_ambitus;
                cur_ambitus = cur_ambitus->parens;
                fs = ambitus_adde(nomen, SYM_FUNC);
                cur_ambitus = cur_salva;
            }
            fs ->typus        = tf;
            fs ->est_globalis = 1;
            fs ->est_staticus = s_stat;

            if (sig.genus == T_LBRACE) {
                nodus_t *fn      = nodus_novus(N_FUNC_DEF);
                fn      ->nomen        = strdup(nomen);
                fn      ->typus        = tf;
                fn      ->typus_decl   = tf;
                fn      ->est_staticus = s_stat;

                /* allocare offsets pro parametris
                 * §6.9.1 AAPCS64: primi 8 in registris.  Struct ≤ 16
                 * octeti occupat 1-2 registros consecutivos; in acervo
                 * reservantur slots contiguae et basis symbolī est
                 * slot maxime negativus ut membrōrum offsets positivē
                 * cēdant. */
                {
                    int slot_cur = -16 - 8;
                    int gp_reg   = 0;
                    int fp_reg   = 0;
                    int stk_off  = 16;
                    for (int i = 0; i < nparams; i++) {
                        typus_t     *pt = psyms[i]->typus;
                        psyms[i]    ->est_globalis = 0;
                        int hfa_n   = 0, hfa_typ = 0;
                        int est_hfa = typus_hfa(pt, &hfa_n, &hfa_typ);
                        if (est_hfa && fp_reg + hfa_n <= 8) {
                            int nregs = (pt->magnitudo + 7) / 8;
                            int base  = slot_cur - (nregs - 1) * 8;
                            psyms[i]  ->offset = base;
                            slot_cur -= nregs * 8;
                            fp_reg   += hfa_n;
                        } else if (est_hfa) {
                            /* NSRN := 8; argumentum in acervum */
                            fp_reg   = 8;
                            psyms[i] ->offset = stk_off;
                            stk_off += (pt->magnitudo + 7) & ~7;
                        } else if (pt && typus_est_fluat(pt) && fp_reg >= 8) {
                            /* NSRN exhaustum — float/double in acervum,
                             * magnitūdine natūrālī (Apple AAPCS64). */
                            int sz   = (pt->genus == TY_FLOAT) ? 4 : 8;
                            stk_off  = (stk_off + sz - 1) & ~(sz - 1);
                            psyms[i] ->offset = stk_off;
                            stk_off += sz;
                        } else if (pt && typus_est_fluat(pt)) {
                            /* fluat in fp registro */
                            psyms[i]->offset = slot_cur;
                            slot_cur -= 8;
                            fp_reg++;
                        } else if (gp_reg >= 8) {
                            /* §6.9.1 AAPCS64: argumentum in acervo vocantis */
                            psyms[i]->offset = stk_off;
                            if (
                                pt && pt->genus == TY_STRUCT
                                && pt->magnitudo <= 16 && pt->magnitudo > 0
                            )
                                stk_off += (pt->magnitudo + 7) & ~7;
                            else
                                stk_off += 8;
                        } else if (
                            pt && pt->genus == TY_STRUCT
                            && pt->magnitudo <= 16 && pt->magnitudo > 0
                        ) {
                            int nregs = (pt->magnitudo + 7) / 8;
                            int base  = slot_cur - (nregs - 1) * 8;
                            psyms[i]  ->offset = base;
                            slot_cur -= nregs * 8;
                            gp_reg   += nregs;
                        } else {
                            psyms[i]->offset = slot_cur;
                            slot_cur -= 8;
                            gp_reg++;
                        }
                    }
                    ambitus_currens()->proximus_offset = slot_cur + 8;
                }

                fn ->dexter = parse_blocum();
                fn ->sinister = nodus_novus(N_NOP);
                fn ->sinister->valor = nparams;
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
            symbolum_t  **psyms = NULL;
            int nparams = 0;
            typus_t     *tf        = parse_parametros(td, &psyms, &nparams);
            free(psyms);

            symbolum_t *fs   = ambitus_adde(nomen, SYM_FUNC);
            fs         ->typus        = tf;
            fs         ->est_globalis = est_globalis;
            fs         ->est_staticus = s_stat;

            if (sig.genus == T_COMMA) {
                lex_proximum();
                continue;
            }
            expecta(T_SEMICOLON);
            return nodus_novus(N_NOP);
        }

        /* declaratio variabilis */
        nodus_t *vd      = nodus_novus(N_VAR_DECL);
        vd      ->nomen        = strdup(nomen);
        vd      ->typus_decl   = td;
        vd      ->typus        = td;
        vd      ->est_staticus = s_stat;
        vd      ->est_externus = s_ext;
        if (ultima_vla_expr)
            vd->tertius = ultima_vla_expr; /* §6.7.5.2: VLA magnitudo */

        /* symbolum — salva in nodo */
        symbolum_t *vs   = ambitus_adde(nomen, SYM_VAR);
        vs         ->typus        = td;
        vs         ->est_globalis = est_globalis || s_stat || s_ext;
        vs         ->est_staticus = s_stat;
        vs         ->est_externus = s_ext;
        /* §6.5.3.4p2: serva expressionem VLA pro sizeof */
        if (ultima_vla_expr)
            vs->vla_expr = (struct nodus *)ultima_vla_expr;
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
                    erratum_ad(
                        sig_linea,
                        "magnitudo variabilis '%s' est zero", nomen
                    );
                int col = typus_colineatio(td);
                if (col < 1)
                    erratum_ad(
                        sig_linea,
                        "colineatio variabilis '%s' invalida", nomen
                    );
                int off = cur_ambitus->proximus_offset - mag;
                off = off & ~(col - 1);
                vs->offset = off;
                cur_ambitus->proximus_offset = off;
            }
        }

        /* initiale */
        if (congruet(T_ASSIGN)) {
            if (sig.genus == T_LBRACE) {
                /* §6.7.8: initializer list { expr, expr, ... }
                 * designatores [N]= et .nomen= ad omnes gradus recursive
                 * per parse_init_elementa praetermittuntur */
                nodus_t *elems[4096];
                int nelem = 0;
                int n_top = parse_init_elementa(elems, &nelem, 4096, td, 0);
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
                        typus_t        *f     = td->basis;
                        while (f && f->genus == TY_ARRAY)
                            f = f->basis;
                        if (f)
                            folium_mag = typus_magnitudo(f);
                        if (folium_mag < 1)
                            erratum_ad(
                                sig_linea,
                                "magnitudo folii tabulae invalida"
                            );
                        int inner_n = basis_mag / folium_mag;
                        if (inner_n < 1)
                            inner_n = 1;
                        td->num_elementorum = (nelem + inner_n - 1) / inner_n;
                    } else if (
                        td->basis->genus == TY_STRUCT
                        && td->basis->num_membrorum > 0
                    ) {
                        /* §6.7.8: tabula strūctūrārum — nūmerus elementōrum
                         * est nūmerus initiālizātōrum directōrum (sīve
                         * ūnusquisque { ... } vel expressiō singula cūm
                         * parse_init_elementa praetermittit prō strūctūrā) */
                        td->num_elementorum = n_top;
                        if (td->num_elementorum < 1)
                            td->num_elementorum = 1;
                    } else {
                        td->num_elementorum = nelem;
                    }
                    td->magnitudo = td->num_elementorum * typus_magnitudo(td->basis);
                }
                /* allocatio differata — nunc allocamus */
                if (alloca_differtur) {
                    int mag = td->magnitudo;
                    if (mag == 0)
                        erratum_ad(
                            sig_linea,
                            "magnitudo tabulae differatae est zero"
                        );
                    int col = typus_colineatio(td);
                    if (col < 1)
                        erratum_ad(
                            sig_linea,
                            "colineatio tabulae differatae invalida"
                        );
                    int off = cur_ambitus->proximus_offset - mag;
                    off = off & ~(col - 1);
                    vs->offset = off;
                    cur_ambitus->proximus_offset = off;
                    alloca_differtur = 0;
                }
            } else {
                vd->sinister = parse_expr_assign();
                /* §6.7.8p14: char a[] = "ABC" — defini magnitudinem
                 * ex chorda.  Applicatur globalibus et localibus. */
                if (
                    vd->sinister && vd->sinister->genus == N_STR &&
                    td->genus == TY_ARRAY && td->num_elementorum <= 0 &&
                    td->basis && (td->basis->genus == TY_CHAR || td->basis->genus == TY_UCHAR)
                ) {
                    td ->num_elementorum = vd->sinister->lon_chordae + 1;
                    td ->magnitudo       = td->num_elementorum;
                }
            }
        }

        /* allocatio differata pro tabulis cum magnitudine iam nota */
        if (alloca_differtur && td->magnitudo > 0) {
            int mag = td->magnitudo;
            int col = typus_colineatio(td);
            if (col < 1)
                erratum_ad(sig_linea, "colineatio invalida");
            int off = cur_ambitus->proximus_offset - mag;
            off = off & ~(col - 1);
            vs->offset = off;
            cur_ambitus->proximus_offset = off;
            alloca_differtur = 0;
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
