/*
 * parser_sent.c — dissectio sententiarum et declarationum localium.
 * Extractum ex parser.c. Continet parse_sententia, parse_blocum,
 * parse_init_elementa, et parse_declaratio (incluso decl globalium
 * et functionum definitionibus).
 */

#include "utilia.h"
#include "parser.h"
#include "parser_intern.h"
#include "fluat.h"

#include <string.h>
#include <stdlib.h>


/* ================================================================
 * sententiae (statements)
 * ================================================================ */

nodus_t *parse_sententia(void)
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
nodus_t *parse_blocum(void)
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
int parse_init_elementa(
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

nodus_t *parse_declaratio(int est_globalis)
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
