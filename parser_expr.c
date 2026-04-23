/*
 * parser_expr.c — dissectio expressionum et evaluatio constantium.
 * Extractum ex parser.c. Continet parse_expr_primaria/postfixa/unaria/
 * binaria/conditio/assign/expr, praecedentiam binariorum, et
 * constans_est/evalua_constans (§6.6).
 */

#include "utilia.h"
#include "parser.h"
#include "parser_intern.h"
#include "typus.h"

#include <string.h>
#include <stdlib.h>

int constans_est(nodus_t *n)
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
            case T_PLUS:
                return s + d;
            case T_MINUS:
                return s - d;
            case T_STAR:
                return s * d;
            case T_SLASH:
                if (d == 0)
                    erratum("divisio per zerum in constante");
                return s / d;
            case T_PERCENT:
                if (d == 0)
                    erratum("modulus per zerum in constante");
                return s % d;
            case T_LTLT:
                return s << d;
            case T_GTGT:
                return s >> d;
            case T_AMP:
                return s & d;
            case T_PIPE:
                return s | d;
            case T_CARET:
                return s ^ d;
            case T_LT:
                return s < d;
            case T_GT:
                return s > d;
            case T_LTEQ:
                return s <= d;
            case T_GTEQ:
                return s >= d;
            case T_EQEQ:
                return s == d;
            case T_BANGEQ:
                return s != d;
            case T_AMPAMP:
                return s && d;
            case T_PIPEPIPE:
                return s || d;
            default:
                erratum("operator non constans in enumeratore: %d", n->op);
            }
        }
    case N_UNOP:
        switch (n->op) {
        case T_MINUS:
            return -evalua_constans(n->sinister);
        case T_TILDE:
            return ~evalua_constans(n->sinister);
        case T_BANG:
            return !evalua_constans(n->sinister);
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

static nodus_t *parse_expr_unaria(void);
static nodus_t *parse_expr_postfixa(void);
static nodus_t *parse_expr_primaria(void);
/* primaria: numerus, chorda, ident, (expr), sizeof */
static nodus_t *parse_expr_primaria(void)
{
    nodus_t *n;

    switch (sig.genus) {
    /* §6.4.4.1: typus constantis integrae ex amplitudine valoris */
    case T_NUM:
        n         = nodus_novus(N_NUM);
        n ->valor = sig.valor;
        /* §6.4.4.1: typus ex suffixo et amplitudine valoris */
        {
            int u      = sig.num_sfx_u;
            int l      = sig.num_sfx_l;
            int magnum = (sig.valor > 0x7FFFFFFF || sig.valor < -0x7FFFFFFF);
            if (l >= 1 || magnum)
                n->typus = u ? ty_ulong : ty_long;
            else
                n->typus = u ? ty_uint : ty_int;
        }
        lex_proximum();
        return n;

    /* §6.4.4.2: constans fluitans — sine suffixo typus est double;
     * 'f'/'F' → float; 'l'/'L' → long double (tractatum ut double). */
    case T_NUM_FLUAT:
        n = nodus_novus(N_NUM_FLUAT);
        n ->valor_f = sig.valor_f;
        n ->typus   = (sig.num_sfx_f == 1) ? ty_float : ty_double;
        lex_proximum();
        return n;

    case T_CHARLIT:
        n         = nodus_novus(N_NUM);
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
                int s_stat     = 0, s_ext = 0;
                typus_t    *tb = parse_specifiers(&s_stat, &s_ext, NULL);
                char nom[256] = {0};
                n ->typus_decl = parse_declarator(tb, nom, 256);
                n ->typus      = n->typus_decl;
                expecta(T_RPAREN);
                return n;
            }

            n         = nodus_novus(N_IDENT);
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
                int s_stat     = 0, s_ext = 0;
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
                    int s_stat     = 0;
                    int s_ext      = 0;
                    typus_t    *tb = parse_specifiers(&s_stat, &s_ext, NULL);
                    char nom[256] = {0};
                    typus_t *st   = parse_declarator(tb, nom, 256);

                    expecta(T_RPAREN);
                    n         = nodus_novus(N_NUM);
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
                n         = nodus_novus(N_NUM);
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
            nodus_t *idx       = nodus_novus(N_INDEX);
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
                        invenit         = 1;
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
                int invenit     = 0;
                for (int i = 0; i < st->num_membrorum; i++) {
                    if (strcmp(st->membra[i].nomen, mem->nomen) == 0) {
                        mem     ->typus = st->membra[i].typus;
                        invenit         = 1;
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
            nodus_t *vocatio   = nodus_novus(N_CALL);
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
            nodus_t *n         = nodus_novus(N_UNOP);
            n       ->op       = op;
            n       ->sinister = parse_expr_unaria();
            n       ->typus    = n->sinister->typus;
            return n;
        }
    case T_STAR: {
            lex_proximum();
            nodus_t *n         = nodus_novus(N_DEREF);
            n       ->sinister = parse_expr_unaria();
            if (n->sinister->typus)
                n->typus = typus_basis_indicis(n->sinister->typus);
            else
                n->typus = ty_int;
            return n;
        }
    case T_AMP: {
            lex_proximum();
            nodus_t *n         = nodus_novus(N_ADDR);
            n       ->sinister = parse_expr_unaria();
            n       ->typus    = typus_indicem(n->sinister->typus ? n->sinister->typus : ty_int);
            return n;
        }
    case T_PLUSPLUS: {
            lex_proximum();
            nodus_t *n         = nodus_novus(N_UNOP);
            n       ->op       = T_PLUSPLUS;
            n       ->sinister = parse_expr_unaria();
            n       ->typus    = n->sinister->typus;
            return n;
        }
    case T_MINUSMINUS: {
            lex_proximum();
            nodus_t *n         = nodus_novus(N_UNOP);
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
    case T_STAR:
    case T_SLASH:
    case T_PERCENT:
        return 13;
    case T_PLUS:
    case T_MINUS:
        return 12;
    case T_LTLT:
    case T_GTGT:
        return 11;
    case T_LT:
    case T_GT:
    case T_LTEQ:
    case T_GTEQ:
        return 10;
    case T_EQEQ:
    case T_BANGEQ:
        return 9;
    case T_AMP:
        return 8;
    case T_CARET:
        return 7;
    case T_PIPE:
        return 6;
    case T_AMPAMP:
        return 5;
    case T_PIPEPIPE:
        return 4;
    default:
        return -1;
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

        nodus_t *n         = nodus_novus(N_BINOP);
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
nodus_t *parse_expr_conditio(void)
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
        nodus_t *ter       = nodus_novus(N_TERNARY);
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
        nodus_t *a         = nodus_novus(N_ASSIGN);
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
        nodus_t *a         = nodus_novus(N_OPASSIGN);
        a       ->op       = op;
        a       ->sinister = n;
        a       ->dexter   = parse_expr_assign();
        a       ->typus    = n->typus;
        return a;
    }
    return n;
}

/* expressio plena (cum ,) */
nodus_t *parse_expr(void)
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
