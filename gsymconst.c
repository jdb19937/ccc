/*
 * gsymconst.c — evaluatio expressionum constantium.
 *
 * §6.6: expressiones constantes pro initializatoribus globalibus.
 * Sustinet N_NUM, N_NUM_FLUAT, N_CAST, N_UNOP, N_BINOP, et
 * idioma offsetof per N_ADDR cum N_MEMBER/N_ARROW/N_DEREF.
 */

#include "utilia.h"
#include "parser.h"
#include "typus.h"
#include "lexator.h"
#include "gsymconst.h"

#include <string.h>

int gsymconst_continet_fluat(nodus_t *n)
{
    if (!n)
        return 0;
    if (n->genus == N_NUM_FLUAT)
        return 1;
    if (n->typus && typus_est_fluat(n->typus))
        return 1;
    if (n->genus == N_BINOP || n->genus == N_UNOP || n->genus == N_CAST)
        return gsymconst_continet_fluat(n->sinister)
            || gsymconst_continet_fluat(n->dexter);
    return 0;
}

double gsymconst_evalua_fluat(nodus_t *n)
{
    if (!n)
        erratum("evalua_constans_fluat: nodus nullus");
    switch (n->genus) {
    case N_NUM_FLUAT:
        return n->valor_f;
    case N_NUM:
        return (double)n->valor;
    case N_CAST:
        return gsymconst_evalua_fluat(n->sinister);
    case N_UNOP:
        switch (n->op) {
        case T_MINUS:
            return -gsymconst_evalua_fluat(n->sinister);
        case T_PLUS:
            return  gsymconst_evalua_fluat(n->sinister);
        }
        erratum_ad(
            n->linea,
            "evalua_constans_fluat: operator unarius %d non tractatus", n->op
        );
        break;
    case N_BINOP:
        {
            double a = gsymconst_evalua_fluat(n->sinister);
            double b = gsymconst_evalua_fluat(n->dexter);
            switch (n->op) {
            case T_PLUS:
                return a + b;
            case T_MINUS:
                return a - b;
            case T_STAR:
                return a * b;
            case T_SLASH:
                return a / b;
            }
            erratum_ad(
                n->linea,
                "evalua_constans_fluat: operator binarius %d non tractatus",
                n->op
            );
        }
        break;
    }
    erratum_ad(
        n->linea,
        "evalua_constans_fluat: genus nodi %d non tractatum", n->genus
    );
    return 0.0;
}

long gsymconst_evalua_integer(nodus_t *n)
{
    if (!n)
        erratum("evalua: nodus nullus");
    switch (n->genus) {
    case N_NUM:
        return n->valor;
    case N_CAST:
        return gsymconst_evalua_integer(n->sinister);
    case N_UNOP:
        switch (n->op) {
        case T_MINUS:
            return -gsymconst_evalua_integer(n->sinister);
        case T_TILDE:
            return ~gsymconst_evalua_integer(n->sinister);
        case T_BANG:
            return !gsymconst_evalua_integer(n->sinister);
        case T_PLUS:
            return gsymconst_evalua_integer(n->sinister);
        }
        break;
    case N_ADDR:
        {
            /* §6.6: recognosce idiōma offsetof:
             * &((T*)0)->membrum  vel  &((T*)0)->m.sub[...] */
            long base    = 0;
            nodus_t   *e = n->sinister;
            while (e) {
                if (e->genus == N_ARROW) {
                    typus_t *st = (
                        e->sinister && e->sinister->typus
                        && e->sinister->typus->genus == TY_PTR
                    )
                        ? e->sinister->typus->basis : NULL;
                    if (!st || st->genus != TY_STRUCT)
                        break;
                    int inv = 0;
                    for (int i = 0; i < st->num_membrorum; i++) {
                        if (strcmp(st->membra[i].nomen, e->nomen) == 0) {
                            base += st->membra[i].offset;
                            inv = 1;
                            break;
                        }
                    }
                    if (!inv)
                        break;
                    return base + gsymconst_evalua_integer(e->sinister);
                } else if (e->genus == N_MEMBER) {
                    typus_t *st = e->sinister ? e->sinister->typus : NULL;
                    if (!st || st->genus != TY_STRUCT)
                        break;
                    int inv = 0;
                    for (int i = 0; i < st->num_membrorum; i++) {
                        if (strcmp(st->membra[i].nomen, e->nomen) == 0) {
                            base += st->membra[i].offset;
                            inv = 1;
                            break;
                        }
                    }
                    if (!inv)
                        break;
                    e = e->sinister;
                } else if (e->genus == N_DEREF) {
                    return base + gsymconst_evalua_integer(e->sinister);
                } else {
                    break;
                }
            }
            break;
        }
    case N_BINOP:
        {
            long a = gsymconst_evalua_integer(n->sinister);
            long b = gsymconst_evalua_integer(n->dexter);
            switch (n->op) {
            case T_PLUS:
                return a + b;
            case T_MINUS:
                return a - b;
            case T_STAR:
                return a * b;
            case T_SLASH:   if (!b)
                    erratum("divisio per 0 in constante");
                return a / b;
            case T_PERCENT: if (!b)
                    erratum("divisio per 0 in constante");
                return a % b;
            case T_AMP:
                return a & b;
            case T_PIPE:
                return a | b;
            case T_CARET:
                return a ^ b;
            case T_LTLT:
                return a << b;
            case T_GTGT:
                return a >> b;
            }
            break;
        }
    }
    erratum_ad(n->linea, "expressio non constans in initializatore");
    return 0;
}
