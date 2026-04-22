/*
 * parser_typi.c — dissectio typorum: specifiers (int, long, struct,
 * enum, typedef, storage-class), structūra/ūniōnis, enumerātōris,
 * declarātōris (stēllae + indicis + tabulae + pointer-to-function),
 * et parametrōrum (§6.7).
 */

#include "utilia.h"
#include "parser.h"
#include "parser_intern.h"
#include "fluat.h"

#include <string.h>
#include <stdlib.h>

nodus_t *ultima_vla_expr; /* §6.7.5.2: VLA expressio magnitudinis */

typus_t *parse_specifiers(
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
typus_t *parse_struct_vel_union(void)
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
                    /* §6.7.2.1: claude ūnitātem campōrum bitōrum pendentem */
                    if (est_struct && cb_positus > 0) {
                        offset     = cb_offset + 4;
                        cb_positus = 0;
                    }
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

/* parse enum */
typus_t *parse_enum(void)
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

typus_t *parse_declarator(typus_t *basis, char *nomen, int max_nomen)
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

typus_t *parse_parametros(
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
