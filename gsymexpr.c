/*
 * gsymexpr.c — generatio expressionum et l-valorum (extractum ex generasym.c).
 *
 * Continet genera_expr (dispatchum per genus nodi), genera_lval
 * (adresses pro l-valoribus), et auxilia (genera_magnitudo_typi,
 * genera_lectio_campi_bitorum).
 */

#include "utilia.h"
#include "parser.h"
#include "generasym.h"
#include "emittesym.h"
#include "typus.h"
#include "generasym_intern.h"

#include <string.h>
#include <stdlib.h>


/* §6.7.2.1: lectio campi bitorum — SBFX/UBFX */
void genera_lectio_campi_bitorum(int r, membrum_t *mb)
{
    esym_ldr32(r, r, 0);
    if (mb->campus_signatus)
        esym_sbfx(r, r, mb->campus_positus, mb->campus_bitorum);
    else
        esym_ubfx(r, r, mb->campus_positus, mb->campus_bitorum);
}

/* quaere membrum campī bitorum ex nodo N_MEMBER/N_ARROW; reddit NULL
 * si non est campus bitorum. */
static membrum_t *campus_bitorum_ex_lval(nodus_t *lv)
{
    if (!lv || (lv->genus != N_MEMBER && lv->genus != N_ARROW))
        return NULL;
    typus_t *cb_st = NULL;
    if (lv->genus == N_MEMBER && lv->sinister)
        cb_st = lv->sinister->typus;
    else if (
        lv->genus == N_ARROW && lv->sinister
        && lv->sinister->typus
        && lv->sinister->typus->genus == TY_PTR
    )
        cb_st = lv->sinister->typus->basis;
    membrum_t *mb = quaere_membrum(cb_st, lv->nomen);
    if (mb && mb->campus_bitorum == 0)
        mb = NULL;
    return mb;
}

void genera_magnitudo_typi(typus_t *t, int dest_reg)
{
    int constant_mag = 1;
    nodus_t *dims[16];
    int ndims      = 0;
    typus_t   *cur = t;
    while (cur && cur->genus == TY_ARRAY) {
        if (cur->num_elementorum > 0) {
            constant_mag *= cur->num_elementorum;
        } else if (cur->vla_dim) {
            if (ndims >= 16)
                erratum("nimis multae dimēnsiōnēs VLA");
            dims[ndims++] = (nodus_t *)cur->vla_dim;
        } else {
            erratum("tabula sine magnitudine in computatione");
        }
        cur = cur->basis;
    }
    if (!cur)
        erratum("typus VLA sine basi scalari");
    int leaf_mag = typus_magnitudo(cur);
    if (leaf_mag < 1)
        erratum("magnitudo basis VLA invalida");
    constant_mag *= leaf_mag;

    int rarm = reg_arm(dest_reg);
    if (ndims == 0) {
        esym_movi(rarm, constant_mag);
        return;
    }
    genera_expr(dims[0], dest_reg);
    for (int i = 1; i < ndims; i++) {
        int rt = reg_alloca();
        genera_expr(dims[i], rt);
        esym_mul(rarm, rarm, reg_arm(rt));
        reg_libera(rt);
    }
    if (constant_mag > 1) {
        esym_movi(17, constant_mag);
        esym_mul(rarm, rarm, 17);
    }
}

/* genera l-valor (adresse in dest) */
void genera_lval(nodus_t *n, int dest)
{
    if (dest >= reg_vertex)
        reg_vertex = dest + 1;
    int r = reg_arm(dest);

    switch (n->genus) {
    case N_IDENT:
        {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            if (!s)
                erratum_ad(n->linea, "symbolum ignotum: %s", n->nomen);
            if (s->est_globalis && s->est_externus) {
                /* externum — per GOT */
                gsym_extern_adde(s->nomen);
                esym_adrp_ldr_got(r, s->nomen);
            } else if (s->est_globalis) {
                /* definitum localis — per @PAGE/@PAGEOFF.
                 * §6.2.2: staticae locales habent linkage nullam; nomen
                 * internum (fortasse muncum) in tabula globalium servatur. */
                if (s->globalis_index < 0)
                    s->globalis_index = gsym_globalis_adde(s->nomen, s->typus, s->est_staticus, 0);
                esym_adrp_add_sym(r, gsym_globales[s->globalis_index].nomen);
            } else {
                /* localis — offset a FP */
                int off = s->offset;
                if (off < 0) {
                    esym_movi(r, -off);
                    esym_sub(r, FP, r);
                } else {
                    esym_addi(r, FP, off);
                }
                /* §6.9.1¶10: parametrus strūctūrae > 16 octētōrum transīt
                 * per pointārium — excipe HFA (quae redita est in v-registris
                 * et conservāta in sēde locālī ut valor). */
                {
                    int _hn = 0, _ht = 0;
                    if (
                        s->est_parametrus && s->typus
                        && s->typus->genus == TY_STRUCT
                        && s->typus->magnitudo > 16
                        && !typus_hfa(s->typus, &_hn, &_ht)
                    )
                        esym_ldr64(r, r, 0);
                }
            }
            break;
        }
    case N_DEREF:
        genera_expr(n->sinister, dest);
        break;
    case N_INDEX:
        {
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            typus_t *elem_t = n->sinister->typus
                ? typus_basis_indicis(n->sinister->typus) : NULL;
            if (elem_t && typus_habet_vla(elem_t)) {
                int rsz = reg_alloca();
                genera_magnitudo_typi(elem_t, rsz);
                esym_mul(reg_arm(r2), reg_arm(r2), reg_arm(rsz));
                reg_libera(rsz);
            } else {
                int basis_mag = 1;
                if (elem_t)
                    basis_mag = mag_typi_verus(elem_t);
                if (basis_mag > 1) {
                    esym_movi(17, basis_mag);
                    esym_mul(reg_arm(r2), reg_arm(r2), 17);
                }
            }
            esym_add(r, r, reg_arm(r2));
            reg_libera(r2);
            break;
        }
    case N_MEMBER:
        {
            genera_lval(n->sinister, dest);
            if (n->sinister->typus && n->sinister->typus->genus == TY_STRUCT) {
                typus_t *st = n->sinister->typus;
                for (int i = 0; i < st->num_membrorum; i++) {
                    if (strcmp(st->membra[i].nomen, n->nomen) == 0) {
                        if (st->membra[i].offset > 0)
                            esym_addi(r, r, st->membra[i].offset);
                        break;
                    }
                }
            }
            break;
        }
    case N_ARROW:
        {
            genera_expr(n->sinister, dest);
            if (
                n->sinister->typus && n->sinister->typus->genus == TY_PTR &&
                n->sinister->typus->basis && n->sinister->typus->basis->genus == TY_STRUCT
            ) {
                typus_t *st = n->sinister->typus->basis;
                for (int i = 0; i < st->num_membrorum; i++) {
                    if (strcmp(st->membra[i].nomen, n->nomen) == 0) {
                        if (st->membra[i].offset > 0)
                            esym_addi(r, r, st->membra[i].offset);
                        break;
                    }
                }
            }
            break;
        }
    default:
        genera_expr(n, dest);
        break;
    }
}

/* genera expressionem, resultatum in dest */
void genera_expr(nodus_t *n, int dest)
{
    if (!n)
        erratum("genera_expr: nodus nullus");

    if (dest >= reg_vertex)
        reg_vertex = dest + 1;
    int r = reg_arm(dest);

    switch (n->genus) {
    case N_NUM:
        esym_movi(r, n->valor);
        break;

    case N_NUM_FLUAT:
        esym_fconst(r, n->valor_f);
        break;

    case N_STR:
        {
            int sid = gsym_chorda_adde(n->chorda, n->lon_chordae);
            esym_adrp_add_str(r, sid);
            break;
        }

    case N_IDENT:
        {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            if (!s)
                erratum_ad(n->linea, "symbolum ignotum: '%s'", n->nomen);
            if (s->genus == SYM_FUNC) {
                int flabel = gsym_func_loc_quaere(s->nomen);
                if (flabel >= 0) {
                    /* functio localis — adresse per @PAGE/@PAGEOFF.
                     * §6.3.2.1¶4: designator functionis convertitur
                     * ad indicatorem ad functionem. */
                    esym_adrp_add_sym(r, s->nomen);
                } else {
                    /* functio externa — per GOT */
                    gsym_extern_adde(s->nomen);
                    esym_adrp_ldr_got(r, s->nomen);
                }
                break;
            }
            genera_lval(n, dest);
            if (s->typus && (s->typus->genus == TY_ARRAY)) {
                if (s->typus->vla_dim)
                    esym_ldr64(r, r, 0);
            } else if (s->typus && s->typus->genus == TY_STRUCT) {
                /* structura → adresse */
            } else if (typus_est_fluat(s->typus)) {
                esym_fload_from_addr(r, r, s->typus);
            } else {
                esym_load_from_addr(r, s->typus);
            }
            break;
        }

    case N_BINOP:
        {
            /* §6.5.13, §6.5.14: short-circuit */
            if (n->op == T_AMPAMP) {
                int l_false = gsym_label_novus();
                int l_end   = gsym_label_novus();
                genera_expr(n->sinister, dest);
                esym_cbz_label(r, l_false);
                genera_expr(n->dexter, dest);
                esym_cbz_label(r, l_false);
                esym_movi(r, 1);
                esym_b_label(l_end);
                esym_label_pone(l_false);
                esym_movi(r, 0);
                esym_label_pone(l_end);
                break;
            }
            if (n->op == T_PIPEPIPE) {
                int l_true = gsym_label_novus();
                int l_end  = gsym_label_novus();
                genera_expr(n->sinister, dest);
                esym_cbnz_label(r, l_true);
                genera_expr(n->dexter, dest);
                esym_cbnz_label(r, l_true);
                esym_movi(r, 0);
                esym_b_label(l_end);
                esym_label_pone(l_true);
                esym_movi(r, 1);
                esym_label_pone(l_end);
                break;
            }
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            int ra = r, rb = reg_arm(r2);

            /* §6.3.1.8: si unus operandus fluitans, via FP */
            if (typus_est_fluat(n->sinister->typus) || typus_est_fluat(n->dexter->typus)) {
                if (!typus_est_fluat(n->sinister->typus))
                    esym_int_to_double(ra, n->sinister->typus);
                if (!typus_est_fluat(n->dexter->typus))
                    esym_int_to_double(rb, n->dexter->typus);

                switch (n->op) {
                case T_PLUS:   esym_fadd(ra, ra, rb); break;
                case T_MINUS:  esym_fsub(ra, ra, rb); break;
                case T_STAR:   esym_fmul(ra, ra, rb); break;
                case T_SLASH:  esym_fdiv(ra, ra, rb); break;
                case T_EQEQ:   esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_EQ);
                    break;
                case T_BANGEQ: esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_NE);
                    break;
                /* §F.3, §F.8.3 (annex_f_iec60559): comparationes cum NaN
                 * reddunt false (nisi !=). ARM64 FCMP pro NaN ponit
                 * N=0 Z=0 C=1 V=1 (unordered). LT = N!=V est TRUE pro
                 * NaN — falsum; utere LO (C=0). LE = Z=1 vel N!=V est
                 * TRUE pro NaN — falsum; utere LS (C=0 vel Z=1). GT et
                 * GE iam reddunt false pro NaN. */
                case T_LT:     esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_LO);
                    break;
                case T_GT:     esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_GT);
                    break;
                case T_LTEQ:   esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_LS);
                    break;
                case T_GTEQ:   esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_GE);
                    break;
                default:
                    erratum("operator %d non sustentatur pro typis fluitantibus", n->op);
                }
                reg_libera(r2);
                break;
            }

            switch (n->op) {
            case T_PLUS:
                if (typus_est_index(n->sinister->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        esym_movi(17, bm);
                        esym_mul(rb, rb, 17);
                    }
                }
                esym_add(ra, ra, rb);
                break;
            case T_MINUS:
                if (typus_est_index(n->sinister->typus) && typus_est_integer(n->dexter->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        esym_movi(17, bm);
                        esym_mul(rb, rb, 17);
                    }
                }
                esym_sub(ra, ra, rb);
                if (typus_est_index(n->sinister->typus) && typus_est_index(n->dexter->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        esym_movi(17, bm);
                        esym_sdiv(ra, ra, 17);
                    }
                }
                break;
            case T_STAR:  esym_mul(ra, ra, rb); break;
            case T_SLASH:
                if (est_unsigned(n->typus))
                    esym_udiv(ra, ra, rb);
                else
                    esym_sdiv(ra, ra, rb);
                break;
            case T_PERCENT:
                if (est_unsigned(n->typus))
                    esym_udiv(17, ra, rb);
                else
                    esym_sdiv(17, ra, rb);
                esym_mul(17, 17, rb);
                esym_sub(ra, ra, 17);
                break;
            case T_AMP:    esym_and(ra, ra, rb); break;
            case T_PIPE:   esym_orr(ra, ra, rb); break;
            case T_CARET:  esym_eor(ra, ra, rb); break;
            case T_LTLT:   esym_lsl(ra, ra, rb); break;
            case T_GTGT:
                if (est_unsigned(n->sinister->typus))
                    esym_lsr(ra, ra, rb);
                else
                    esym_asr(ra, ra, rb);
                break;
            case T_EQEQ: {
                    int sz = mag_typi(n->sinister->typus);
                    if (mag_typi(n->dexter->typus) > sz)
                        sz = mag_typi(n->dexter->typus);
                    esym_cmp(ra, rb, sz);
                    esym_cset(ra, GSYM_COND_EQ);
                    break;
                }
            case T_BANGEQ: {
                    int sz = mag_typi(n->sinister->typus);
                    if (mag_typi(n->dexter->typus) > sz)
                        sz = mag_typi(n->dexter->typus);
                    esym_cmp(ra, rb, sz);
                    esym_cset(ra, GSYM_COND_NE);
                    break;
                }
            case T_LT: {
                    int sz = mag_typi(n->sinister->typus);
                    int us = est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus);
                    if (mag_typi(n->dexter->typus) > sz)
                        sz = mag_typi(n->dexter->typus);
                    esym_cmp(ra, rb, sz);
                    esym_cset(ra, us ? GSYM_COND_LO : GSYM_COND_LT);
                    break;
                }
            case T_GT: {
                    int sz = mag_typi(n->sinister->typus);
                    int us = est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus);
                    if (mag_typi(n->dexter->typus) > sz)
                        sz = mag_typi(n->dexter->typus);
                    esym_cmp(ra, rb, sz);
                    esym_cset(ra, us ? GSYM_COND_HI : GSYM_COND_GT);
                    break;
                }
            case T_LTEQ: {
                    int sz = mag_typi(n->sinister->typus);
                    int us = est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus);
                    if (mag_typi(n->dexter->typus) > sz)
                        sz = mag_typi(n->dexter->typus);
                    esym_cmp(ra, rb, sz);
                    esym_cset(ra, us ? GSYM_COND_LS : GSYM_COND_LE);
                    break;
                }
            case T_GTEQ: {
                    int sz = mag_typi(n->sinister->typus);
                    int us = est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus);
                    if (mag_typi(n->dexter->typus) > sz)
                        sz = mag_typi(n->dexter->typus);
                    esym_cmp(ra, rb, sz);
                    esym_cset(ra, us ? GSYM_COND_HS : GSYM_COND_GE);
                    break;
                }
            case T_AMPAMP:
                {
                    int l_false = gsym_label_novus();
                    int l_end   = gsym_label_novus();
                    esym_cbz_label(ra, l_false);
                    esym_cbz_label(rb, l_false);
                    esym_movi(ra, 1);
                    esym_b_label(l_end);
                    esym_label_pone(l_false);
                    esym_movi(ra, 0);
                    esym_label_pone(l_end);
                    break;
                }
            case T_PIPEPIPE:
                {
                    int l_true = gsym_label_novus();
                    int l_end  = gsym_label_novus();
                    esym_cbnz_label(ra, l_true);
                    esym_cbnz_label(rb, l_true);
                    esym_movi(ra, 0);
                    esym_b_label(l_end);
                    esym_label_pone(l_true);
                    esym_movi(ra, 1);
                    esym_label_pone(l_end);
                    break;
                }
            default:
                erratum_ad(n->linea, "operator binarius %d non sustentatur", n->op);
            }
            reg_libera(r2);
            break;
        }

    case N_UNOP:
        {
            genera_expr(n->sinister, dest);
            switch (n->op) {
            case T_MINUS:
                if (typus_est_fluat(n->typus))
                    esym_fneg(r, r);
                else
                    esym_neg(r, r);
                break;
            case T_TILDE: esym_mvn(r, r); break;
            case T_BANG:  esym_cmpi(r, 0, mag_typi(n->sinister->typus));
                esym_cset(r, GSYM_COND_EQ);
                break;
            case T_PLUSPLUS:
            case T_MINUSMINUS:
                {
                    int r2 = reg_alloca();
                    genera_lval(n->sinister, r2);
                    membrum_t *cb_mem = campus_bitorum_ex_lval(n->sinister);
                    if (cb_mem) {
                        /* §6.7.2.1: pre-op in campo bitorum */
                        int r3  = reg_alloca();
                        int ra2 = reg_arm(r2);
                        int ra3 = reg_arm(r3);
                        esym_ldr32(ra3, ra2, 0);
                        if (cb_mem->campus_signatus)
                            esym_sbfx(r, ra3, cb_mem->campus_positus, cb_mem->campus_bitorum);
                        else
                            esym_ubfx(r, ra3, cb_mem->campus_positus, cb_mem->campus_bitorum);
                        if (n->op == T_PLUSPLUS)
                            esym_addi(r, r, 1);
                        else
                            esym_subi(r, r, 1);
                        esym_bfi(ra3, r, cb_mem->campus_positus, cb_mem->campus_bitorum);
                        esym_str32(ra3, ra2, 0);
                        reg_libera(r3);
                        reg_libera(r2);
                        break;
                    }
                    int mag  = mag_typi(n->typus);
                    int step = 1;
                    if (typus_est_index(n->typus))
                        step = mag_typi(typus_basis_indicis(n->typus));
                    int mag_op = mag > 8 ? 8 : mag;
                    int uns    = est_unsigned(n->typus);
                    if (uns)
                        esym_load_unsigned(r, reg_arm(r2), 0, mag_op);
                    else
                        esym_load(r, reg_arm(r2), 0, mag_op);
                    if (n->op == T_PLUSPLUS)
                        esym_addi(r, r, step);
                    else
                        esym_subi(r, r, step);
                    /* trunca ad magnitudinem typi — aliter valor effectus
                     * supra fines typi extendit (e.g. 0xFF+1 debet esse 0, non 256) */
                    if (mag_op < 8 && !typus_est_index(n->typus)) {
                        if (mag_op == 1) {
                            if (uns)
                                esym_uxtb(r, r);
                            else
                                esym_sxtb(r, r);
                        } else if (mag_op == 2) {
                            if (uns)
                                esym_uxth(r, r);
                            else
                                esym_sxth(r, r);
                        } else if (mag_op == 4) {
                            if (uns)
                                esym_uxtw(r, r);
                            else
                                esym_sxtw(r, r);
                        }
                    }
                    esym_store(r, reg_arm(r2), 0, mag_op);
                    reg_libera(r2);
                    break;
                }
            default:
                erratum_ad(n->linea, "operator unarius %d non sustentatur", n->op);
            }
            break;
        }

    case N_POSTOP:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            membrum_t *cb_mem = campus_bitorum_ex_lval(n->sinister);
            if (cb_mem) {
                /* §6.7.2.1: post-op in campo bitorum */
                int r3  = reg_alloca();
                int ra2 = reg_arm(r2);
                int ra3 = reg_arm(r3);
                esym_ldr32(ra3, ra2, 0);
                if (cb_mem->campus_signatus)
                    esym_sbfx(r, ra3, cb_mem->campus_positus, cb_mem->campus_bitorum);
                else
                    esym_ubfx(r, ra3, cb_mem->campus_positus, cb_mem->campus_bitorum);
                if (n->op == T_PLUSPLUS)
                    esym_addi(17, r, 1);
                else
                    esym_subi(17, r, 1);
                esym_bfi(ra3, 17, cb_mem->campus_positus, cb_mem->campus_bitorum);
                esym_str32(ra3, ra2, 0);
                reg_libera(r3);
                reg_libera(r2);
                break;
            }
            int mag = mag_typi(n->typus);
            if (est_unsigned(n->typus))
                esym_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            else
                esym_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            int inc = 1;
            if (typus_est_index(n->typus))
                inc = mag_typi(typus_basis_indicis(n->typus));
            if (n->op == T_PLUSPLUS)
                esym_addi(17, r, inc);
            else
                esym_subi(17, r, inc);
            esym_store(17, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            reg_libera(r2);
            break;
        }

    case N_ASSIGN:
        {
            /* §6.7.2.1: tracta campum bitōrum */
            membrum_t *cb_mem = NULL;
            if (
                n->sinister && (
                    n->sinister->genus == N_MEMBER
                    || n->sinister->genus == N_ARROW
                )
            ) {
                typus_t *cb_st = NULL;
                if (n->sinister->genus == N_MEMBER && n->sinister->sinister)
                    cb_st = n->sinister->sinister->typus;
                else if (
                    n->sinister->genus == N_ARROW && n->sinister->sinister
                    && n->sinister->sinister->typus
                    && n->sinister->sinister->typus->genus == TY_PTR
                )
                    cb_st = n->sinister->sinister->typus->basis;
                cb_mem = quaere_membrum(cb_st, n->sinister->nomen);
                if (cb_mem && cb_mem->campus_bitorum == 0)
                    cb_mem = NULL;
            }
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            genera_expr(n->dexter, dest);
            if (cb_mem) {
                int r3  = reg_alloca();
                int ra2 = reg_arm(r2);
                int ra3 = reg_arm(r3);
                esym_ldr32(ra3, ra2, 0);
                esym_bfi(ra3, r, cb_mem->campus_positus, cb_mem->campus_bitorum);
                esym_str32(ra3, ra2, 0);
                reg_libera(r3);
            } else {
                int mag = mag_typi(n->sinister->typus);
                if (n->sinister->typus && n->sinister->typus->genus == TY_STRUCT) {
                    for (int i = 0; i < mag; i += 8) {
                        int rem = mag - i;
                        if (rem >= 8) {
                            esym_ldr64(17, r, i);
                            esym_str64(17, reg_arm(r2), i);
                        }else if (rem >= 4) {
                            esym_ldr32(17, r, i);
                            esym_str32(17, reg_arm(r2), i);
                        }else {
                            esym_ldrb(17, r, i);
                            esym_strb(17, reg_arm(r2), i);
                        }
                    }
                } else if (typus_est_fluat(n->sinister->typus)) {
                    if (!typus_est_fluat(n->dexter->typus))
                        esym_int_to_double(r, n->dexter->typus);
                    esym_fstore_to_addr(r, reg_arm(r2), n->sinister->typus);
                } else {
                    esym_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                }
            }
            reg_libera(r2);
            break;
        }

    case N_OPASSIGN:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            membrum_t *cb_mem = campus_bitorum_ex_lval(n->sinister);
            if (cb_mem) {
                /* §6.7.2.1: compound-assign in campo bitorum */
                int r3  = reg_alloca();
                int ra2 = reg_arm(r2);
                int ra3 = reg_arm(r3);
                esym_ldr32(ra3, ra2, 0);
                if (cb_mem->campus_signatus)
                    esym_sbfx(r, ra3, cb_mem->campus_positus, cb_mem->campus_bitorum);
                else
                    esym_ubfx(r, ra3, cb_mem->campus_positus, cb_mem->campus_bitorum);
                int r4 = reg_alloca();
                genera_expr(n->dexter, r4);
                int rb = reg_arm(r4);
                switch (n->op) {
                case T_PLUSEQ:   esym_add(r, r, rb); break;
                case T_MINUSEQ:  esym_sub(r, r, rb); break;
                case T_STAREQ:   esym_mul(r, r, rb); break;
                case T_SLASHEQ:  esym_sdiv(r, r, rb); break;
                case T_PERCENTEQ:
                    esym_sdiv(17, r, rb);
                    esym_mul(17, 17, rb);
                    esym_sub(r, r, 17);
                    break;
                case T_AMPEQ:    esym_and(r, r, rb); break;
                case T_PIPEEQ:   esym_orr(r, r, rb); break;
                case T_CARETEQ:  esym_eor(r, r, rb); break;
                case T_LTLTEQ:   esym_lsl(r, r, rb); break;
                case T_GTGTEQ:
                    if (cb_mem->campus_signatus == 0)
                        esym_lsr(r, r, rb);
                    else
                        esym_asr(r, r, rb);
                    break;
                default:
                    erratum_ad(n->linea, "compound-assign operator %d in campō bitōrum ignotus", n->op);
                }
                reg_libera(r4);
                esym_bfi(ra3, r, cb_mem->campus_positus, cb_mem->campus_bitorum);
                esym_str32(ra3, ra2, 0);
                reg_libera(r3);
                reg_libera(r2);
                break;
            }
            int mag = mag_typi(n->sinister->typus);
            if (typus_est_fluat(n->sinister->typus)) {
                esym_fload_from_addr(r, reg_arm(r2), n->sinister->typus);
                int r3 = reg_alloca();
                genera_expr(n->dexter, r3);
                int rb = reg_arm(r3);
                if (!typus_est_fluat(n->dexter->typus))
                    esym_int_to_double(rb, n->dexter->typus);
                switch (n->op) {
                case T_PLUSEQ:   esym_fadd(r, r, rb); break;
                case T_MINUSEQ:  esym_fsub(r, r, rb); break;
                case T_STAREQ:   esym_fmul(r, r, rb); break;
                case T_SLASHEQ:  esym_fdiv(r, r, rb); break;
                default:
                    erratum_ad(n->linea, "operator %d non sustentatur pro typis fluitantibus", n->op);
                }
                reg_libera(r3);
                esym_fstore_to_addr(r, reg_arm(r2), n->sinister->typus);
            } else {
                if (est_unsigned(n->sinister->typus))
                    esym_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                else
                    esym_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                int r3 = reg_alloca();
                genera_expr(n->dexter, r3);
                int rb = reg_arm(r3);
                switch (n->op) {
                case T_PLUSEQ:
                    if (typus_est_index(n->sinister->typus)) {
                        int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                        if (bm > 1) {
                            esym_movi(17, bm);
                            esym_mul(rb, rb, 17);
                        }
                    }
                    esym_add(r, r, rb);
                    break;
                case T_MINUSEQ:
                    if (typus_est_index(n->sinister->typus)) {
                        int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                        if (bm > 1) {
                            esym_movi(17, bm);
                            esym_mul(rb, rb, 17);
                        }
                    }
                    esym_sub(r, r, rb);
                    break;
                case T_STAREQ:   esym_mul(r, r, rb); break;
                case T_SLASHEQ:  esym_sdiv(r, r, rb); break;
                case T_PERCENTEQ:
                    esym_sdiv(17, r, rb);
                    esym_mul(17, 17, rb);
                    esym_sub(r, r, 17);
                    break;
                case T_AMPEQ:    esym_and(r, r, rb); break;
                case T_PIPEEQ:   esym_orr(r, r, rb); break;
                case T_CARETEQ:  esym_eor(r, r, rb); break;
                case T_LTLTEQ:   esym_lsl(r, r, rb); break;
                case T_GTGTEQ:
                    if (est_unsigned(n->sinister->typus))
                        esym_lsr(r, r, rb);
                    else
                        esym_asr(r, r, rb);
                    break;
                default:
                    erratum_ad(n->linea, "compound-assign operator %d ignotus", n->op);
                }
                reg_libera(r3);
                esym_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            }
            reg_libera(r2);
            break;
        }

    case N_TERNARY:
        {
            int l_false = gsym_label_novus(), l_end = gsym_label_novus();
            genera_expr(n->sinister, dest);
            esym_cbz_label(r, l_false);
            genera_expr(n->dexter, dest);
            esym_b_label(l_end);
            esym_label_pone(l_false);
            genera_expr(n->tertius, dest);
            esym_label_pone(l_end);
            break;
        }

    case N_COMMA_EXPR:
        genera_expr(n->sinister, dest);
        genera_expr(n->dexter, dest);
        break;

    case N_CALL:
        {
            int salvati   = reg_vertex;
            int prof_meum = profunditas_vocationis;
            if (prof_meum >= 8)
                erratum_ad(
                    n->linea,
                    "vocatio nimis profunde nidificata (max 8)"
                );
            /* save regio: 16 banks (8 int + 8 fp) de 15 regibus × 8 octeti,
             * collocatī in fundō frame. Prof=0 deepest. */
            int reg_save_int_base = cur_frame_mag - 16 - 14 * 8
                - prof_meum * 15 * 8;
            int reg_save_fp_base  = cur_frame_mag - 16 - 14 * 8
                - 8 * 15 * 8 - prof_meum * 15 * 8;
            for (int i = 0; i < salvati; i++) {
                esym_str64(reg_arm(i), FP, -(reg_save_int_base + i * 8));
                esym_fstr64(reg_arm(i), FP, -(reg_save_fp_base + i * 8));
            }

            int nargs = n->num_membrorum;
            int arg_spill_base = cur_frame_mag - 16 - 16 * 15 * 8
                + profunditas_vocationis * 8 * 8;
            profunditas_vocationis++;

            int fptr_spill    = arg_spill_base + (nargs + 1) * 8;
            int ret_ptr_spill = fptr_spill + 8;
            /* struct_copy_base: in regiōne sēparātā nē collīdātur
             * cum slots vocātiōnis nestae (quae arg_spill_base
             * proximae profundit. occupant). */
            int struct_copy_base_spill = cur_frame_mag - 16 - 16 * 15 * 8 - 16
                - (profunditas_vocationis - 1) * 8;

            /* AAPCS64: structurae > 16 byte redduntur per indicatorem
             * in x8 quem vocator praeparat. Allocamus scrinium in acervo,
             * servamus adresse in scrinium FP-relativum ut nested calls
             * non corrumpant x8. */
            typus_t *call_ret_typ = NULL;
            if (n->sinister->typus && n->sinister->typus->genus == TY_FUNC)
                call_ret_typ = n->sinister->typus->reditus;
            else if (
                n->sinister->typus && n->sinister->typus->genus == TY_PTR
                && n->sinister->typus->basis
                && n->sinister->typus->basis->genus == TY_FUNC
            )
                call_ret_typ = n->sinister->typus->basis->reditus;
            int ret_hfa_n   = 0, ret_hfa_typ = 0;
            int ret_est_hfa = typus_hfa(call_ret_typ, &ret_hfa_n, &ret_hfa_typ);
            int ret_per_mem = (
                call_ret_typ && call_ret_typ->genus == TY_STRUCT
                && call_ret_typ->magnitudo > 16
                && !ret_est_hfa
            );
            int ret_in_regs_struct = (
                call_ret_typ && call_ret_typ->genus == TY_STRUCT
                && call_ret_typ->magnitudo > 0
                && call_ret_typ->magnitudo <= 16
                && !ret_est_hfa
            );
            int ret_mag_aligned = 0;
            if (ret_per_mem || ret_in_regs_struct || ret_est_hfa)
                ret_mag_aligned = (call_ret_typ->magnitudo + 15) & ~15;

            int est_indirecta = 0;
            if (n->sinister->genus == N_IDENT) {
                symbolum_t *si = n->sinister->sym ? n->sinister->sym : ambitus_quaere_omnes(n->sinister->nomen);
                if (si && si->genus != SYM_FUNC && gsym_func_loc_quaere(n->sinister->nomen) < 0)
                    est_indirecta = 1;
            } else {
                est_indirecta = 1;
            }
            if (est_indirecta) {
                reg_vertex = 0;
                genera_expr(n->sinister, 0);
                esym_str64(0, FP, -(fptr_spill));
            }

            int struct_copy_tot = 0;
            for (int i = 0; i < nargs; i++) {
                typus_t *pt = n->membra[i]->typus;
                int xhfa_n, xhfa_t;
                if (
                    pt && pt->genus == TY_STRUCT && pt->magnitudo > 16
                    && !typus_hfa(pt, &xhfa_n, &xhfa_t)
                )
                    struct_copy_tot += (pt->magnitudo + 15) & ~15;
            }
            int struct_copy_alloc = (struct_copy_tot + 15) & ~15;
            if (ret_per_mem || ret_in_regs_struct || ret_est_hfa) {
                esym_subi(SP, SP, ret_mag_aligned);
                esym_addi(17, SP, 0);
                esym_str64(17, FP, -ret_ptr_spill);
            }
            if (struct_copy_alloc > 0) {
                esym_subi(SP, SP, struct_copy_alloc);
                /* servamus basim scrini copiae FP-relativē: vocātiōnēs
                 * nestae perturbant SP (e.g. scrīnium reditūs) */
                esym_addi(17, SP, 0);
                esym_str64(17, FP, -struct_copy_base_spill);
            }
            int struct_copy_cur = 0;
            for (int i = 0; i < nargs; i++) {
                reg_vertex = 0;
                genera_expr(n->membra[i], 0);
                typus_t       *mt = n->membra[i]->typus;
                int m_hfa_n       = 0, m_hfa_t = 0;
                int m_est_hfa     = typus_hfa(mt, &m_hfa_n, &m_hfa_t);
                if (typus_est_fluat(mt)) {
                    int off = arg_spill_base + i * 8;
                    esym_movi(17, off);
                    esym_sub(17, FP, 17);
                    esym_fstr64(0, 17, 0);
                } else if (
                    mt
                    && mt->genus == TY_STRUCT
                    && mt->magnitudo > 16
                    && !m_est_hfa
                ) {
                    int mag    = n->membra[i]->typus->magnitudo;
                    int mag_al = (mag + 15) & ~15;
                    esym_ldr64(17, FP, -struct_copy_base_spill);
                    if (struct_copy_cur)
                        esym_addi(17, 17, struct_copy_cur);
                    for (int off = 0; off < mag; off += 8) {
                        int rem = mag - off;
                        if (rem >= 8) {
                            esym_ldr64(16, 0, off);
                            esym_str64(16, 17, off);
                        } else if (rem >= 4) {
                            esym_ldr32(16, 0, off);
                            esym_str32(16, 17, off);
                        } else {
                            esym_ldrb(16, 0, off);
                            esym_strb(16, 17, off);
                        }
                    }
                    esym_str64(17, FP, -(arg_spill_base + i * 8));
                    struct_copy_cur += mag_al;
                } else {
                    esym_str64(0, FP, -(arg_spill_base + i * 8));
                }
            }

            typus_t *func_typ = NULL;
            if (n->sinister->typus && n->sinister->typus->genus == TY_FUNC)
                func_typ = n->sinister->typus;
            else if (
                n->sinister->typus && n->sinister->typus->genus == TY_PTR &&
                n->sinister->typus->basis && n->sinister->typus->basis->genus == TY_FUNC
            )
                func_typ = n->sinister->typus->basis;

            int est_variadica = func_typ ? func_typ->est_variadicus : 0;
            int num_nominati  = func_typ ? func_typ->num_parametrorum : nargs;
            if (num_nominati > nargs)
                num_nominati = nargs;

            /* AAPCS64 (Apple ARM64): classificamus omnia argumenta in
             * registra (NGRN/NSRN) vel acervum. Cum NGRN/NSRN exhausti,
             * argumenta sequentia eunt in acervum in ordine. Variadica
             * non-nominata semper in acervum (conventio Apple).
             * Nullum argumentum silenter saltetur. */
            int num_fp_regs  = 0;
            int num_gp_regs  = 0;
            int *arg_reg     = malloc(nargs * sizeof(int));
            int *arg_stk_off = malloc(nargs * sizeof(int));
            int *arg_hfa_n   = malloc(nargs * sizeof(int));
            int *arg_hfa_typ = malloc(nargs * sizeof(int));
            if (nargs > 0 && (!arg_reg || !arg_stk_off || !arg_hfa_n || !arg_hfa_typ))
                erratum("memoria exhausta in vocatione");
            for (int i = 0; i < nargs; i++)
                arg_hfa_n[i] = 0;
            int stk_bytes = 0;

            for (int i = 0; i < nargs; i++) {
                typus_t *pt = NULL;
                if (func_typ && i < func_typ->num_parametrorum)
                    pt = func_typ->parametri[i];
                if (!pt)
                    pt = n->membra[i]->typus;
                int est_var_arg = (est_variadica && i >= num_nominati);
                int hfa_n       = 0, hfa_typ = 0;
                int est_hfa     = !est_var_arg && typus_hfa(pt, &hfa_n, &hfa_typ);
                if (est_var_arg) {
                    arg_reg[i]     = -1;
                    arg_stk_off[i] = stk_bytes;
                    stk_bytes += 8;
                } else if (est_hfa) {
                    if (num_fp_regs + hfa_n <= 8) {
                        arg_reg[i]     = num_fp_regs;
                        arg_hfa_n[i]   = hfa_n;
                        arg_hfa_typ[i] = hfa_typ;
                        num_fp_regs   += hfa_n;
                    } else {
                        /* AAPCS64 §B.5: NSRN := 8; HFA transitur per
                         * acervum ut N elementa consecutiva. */
                        num_fp_regs    = 8;
                        arg_reg[i]     = -1;
                        arg_hfa_n[i]   = hfa_n;
                        arg_hfa_typ[i] = hfa_typ;
                        arg_stk_off[i] = stk_bytes;
                        stk_bytes += (pt->magnitudo + 7) & ~7;
                    }
                } else if (typus_est_fluat(pt)) {
                    if (num_fp_regs < 8) {
                        arg_reg[i] = num_fp_regs;
                        num_fp_regs++;
                    } else {
                        /* Apple AAPCS64: arg fluat in acervum suā magnitūdine
                         * naturālī (float=4, double=8). */
                        int esz        = (pt->genus == TY_FLOAT) ? 4 : 8;
                        stk_bytes      = (stk_bytes + esz - 1) & ~(esz - 1);
                        arg_reg[i]     = -1;
                        arg_stk_off[i] = stk_bytes;
                        stk_bytes += esz;
                    }
                } else if (
                    pt && pt->genus == TY_STRUCT
                    && pt->magnitudo <= 16 && pt->magnitudo > 0
                ) {
                    int nr = (pt->magnitudo + 7) / 8;
                    if (num_gp_regs + nr <= 8) {
                        arg_reg[i] = num_gp_regs;
                        num_gp_regs += nr;
                    } else {
                        /* §B.4: NGRN := 8; argumentum in acervum */
                        num_gp_regs    = 8;
                        arg_reg[i]     = -1;
                        arg_stk_off[i] = stk_bytes;
                        stk_bytes += (pt->magnitudo + 7) & ~7;
                    }
                } else {
                    if (num_gp_regs < 8) {
                        arg_reg[i] = num_gp_regs;
                        num_gp_regs++;
                    } else {
                        arg_reg[i]     = -1;
                        arg_stk_off[i] = stk_bytes;
                        stk_bytes += 8;
                    }
                }
            }

            int acervus_args = (stk_bytes + 15) & ~15;
            if (acervus_args > 0)
                esym_subi(SP, SP, acervus_args);

            /* primum: stack args (SP-relative) */
            for (int i = 0; i < nargs; i++) {
                if (arg_reg[i] >= 0)
                    continue;
                typus_t *pt = NULL;
                if (func_typ && i < func_typ->num_parametrorum)
                    pt = func_typ->parametri[i];
                if (!pt)
                    pt = n->membra[i]->typus;
                int off = arg_stk_off[i];
                if (arg_hfa_n[i] > 0) {
                    /* HFA in acervum: carrica punctum struct et scribe
                     * N elementa consecutiva (float=4, double=8). */
                    esym_ldr64(17, FP, -(arg_spill_base + i * 8));
                    for (int k = 0; k < arg_hfa_n[i]; k++) {
                        if (arg_hfa_typ[i] == TY_FLOAT) {
                            esym_fldr32(0, 17, k * 4);
                            esym_fstr32(0, SP, off + k * 4);
                        } else {
                            esym_fldr64(0, 17, k * 8);
                            esym_fstr64(0, SP, off + k * 8);
                        }
                    }
                } else if (
                    pt && pt->genus == TY_STRUCT
                    && pt->magnitudo <= 16 && pt->magnitudo > 0
                ) {
                    /* struct in acervum: copia bytes ex punctu spill */
                    esym_ldr64(17, FP, -(arg_spill_base + i * 8));
                    int mag = pt->magnitudo;
                    int k;
                    for (k = 0; k + 8 <= mag; k += 8) {
                        esym_ldr64(16, 17, k);
                        esym_str64(16, SP, off + k);
                    }
                    while (k + 4 <= mag) {
                        esym_ldr32(16, 17, k);
                        esym_str32(16, SP, off + k);
                        k += 4;
                    }
                    while (k < mag) {
                        esym_ldrb(16, 17, k);
                        esym_strb(16, SP, off + k);
                        k++;
                    }
                } else if (pt && typus_est_fluat(pt)) {
                    /* spill continet d-reg (8 bytarum); si TY_FLOAT in
                     * positione prōtotypātā, converte ad s-reg et scribe
                     * 4 bytes. In positione variadicā (C99 §6.5.2.2/6),
                     * float prōmōvētur ad double — scrībimus 8 byte
                     * sine conversiōne. */
                    int est_var_arg_i = (est_variadica && i >= num_nominati);
                    esym_fldr64(17, FP, -(arg_spill_base + i * 8));
                    if (pt->genus == TY_FLOAT && !est_var_arg_i) {
                        esym_fcvt_ds(17, 17);
                        esym_fstr32(17, SP, off);
                    } else {
                        esym_fstr64(17, SP, off);
                    }
                } else {
                    esym_ldr64(17, FP, -(arg_spill_base + i * 8));
                    esym_str64(17, SP, off);
                }
            }

            /* deinde: reg loads */
            for (int i = 0; i < nargs; i++) {
                if (arg_reg[i] < 0)
                    continue;
                typus_t *pt = NULL;
                if (func_typ && i < func_typ->num_parametrorum)
                    pt = func_typ->parametri[i];
                if (!pt)
                    pt = n->membra[i]->typus;
                if (arg_hfa_n[i] > 0) {
                    /* HFA: carrica elementa ex structo in v-registra consecutiva */
                    esym_ldr64(17, FP, -(arg_spill_base + i * 8));
                    int esz = (arg_hfa_typ[i] == TY_FLOAT) ? 4 : 8;
                    for (int k = 0; k < arg_hfa_n[i]; k++) {
                        if (arg_hfa_typ[i] == TY_FLOAT)
                            esym_fldr32(arg_reg[i] + k, 17, k * esz);
                        else
                            esym_fldr64(arg_reg[i] + k, 17, k * esz);
                    }
                } else if (typus_est_fluat(pt)) {
                    int off = arg_spill_base + i * 8;
                    esym_movi(17, off);
                    esym_sub(17, FP, 17);
                    esym_fldr64(arg_reg[i], 17, 0);
                    if (pt->genus == TY_FLOAT)
                        esym_fcvt_ds(arg_reg[i], arg_reg[i]);
                } else if (
                    pt && pt->genus == TY_STRUCT
                    && pt->magnitudo <= 16 && pt->magnitudo > 0
                ) {
                    int nr = (pt->magnitudo + 7) / 8;
                    esym_ldr64(17, FP, -(arg_spill_base + i * 8));
                    for (int k = 0; k < nr; k++)
                        esym_ldr64(arg_reg[i] + k, 17, k * 8);
                } else {
                    esym_ldr64(arg_reg[i], FP, -(arg_spill_base + i * 8));
                }
            }

            free(arg_reg);
            free(arg_stk_off);
            free(arg_hfa_n);
            free(arg_hfa_typ);

            /* voca functionem */
            reg_vertex = 0;
            if (ret_per_mem)
                esym_ldr64(8, FP, -ret_ptr_spill);
            if (n->sinister->genus == N_IDENT) {
                int flabel = gsym_func_loc_quaere(n->sinister->nomen);
                if (flabel >= 0) {
                    /* vocatio directa — bl _nomen */
                    esym_bl_sym(n->sinister->nomen);
                } else {
                    symbolum_t *s = n->sinister->sym ? n->sinister->sym : ambitus_quaere_omnes(n->sinister->nomen);
                    if (s && s->genus != SYM_FUNC) {
                        esym_ldr64(16, FP, -(fptr_spill));
                        esym_blr(16);
                    } else {
                        /* functio externa — bl _nomen (imm et linker stub creant) */
                        gsym_extern_adde(n->sinister->nomen);
                        esym_bl_sym(n->sinister->nomen);
                    }
                }
            } else {
                esym_ldr64(16, FP, -(fptr_spill));
                esym_blr(16);
            }

            profunditas_vocationis--;

            if (acervus_args > 0)
                esym_addi(SP, SP, acervus_args);
            if (struct_copy_alloc > 0)
                esym_addi(SP, SP, struct_copy_alloc);

            {
                typus_t *ret_typ = NULL;
                if (func_typ)
                    ret_typ = func_typ->reditus;
                if (ret_per_mem) {
                    esym_ldr64(r, FP, -ret_ptr_spill);
                } else if (ret_est_hfa) {
                    /* AAPCS64 §5.9.5: HFA redditur in d0..dN-1 (float HFA
                     * in s0..sN-1). Effundimus in scrinium locale et
                     * reponimus adressum in r. */
                    esym_ldr64(17, FP, -ret_ptr_spill);
                    int esz = (ret_hfa_typ == TY_FLOAT) ? 4 : 8;
                    for (int k = 0; k < ret_hfa_n; k++) {
                        if (ret_hfa_typ == TY_FLOAT)
                            esym_fstr32(k, 17, k * esz);
                        else
                            esym_fstr64(k, 17, k * esz);
                    }
                    esym_ldr64(r, FP, -ret_ptr_spill);
                } else if (ret_in_regs_struct) {
                    /* AAPCS64: structura ≤ 16 bytarum redditur in x0 (et x1
                     * si magnitudo > 8). Effundimus in scrinium locale
                     * praeparatum et reponimus adressum in r. */
                    esym_ldr64(17, FP, -ret_ptr_spill);
                    esym_str64(0, 17, 0);
                    if (call_ret_typ->magnitudo > 8)
                        esym_str64(1, 17, 8);
                    esym_ldr64(r, FP, -ret_ptr_spill);
                } else if (typus_est_fluat(ret_typ)) {
                    if (ret_typ && ret_typ->genus == TY_FLOAT)
                        esym_fcvt_sd(0, 0);
                    if (r != 0)
                        esym_fmov_dd(r, 0);
                } else {
                    if (ret_typ && typus_est_integer(ret_typ)) {
                        int rm = mag_typi(ret_typ);
                        if (rm == 1) {
                            if (ret_typ->est_sine_signo)
                                esym_uxtb(0, 0);
                            else
                                esym_sxtb(0, 0);
                        } else if (rm == 2) {
                            if (ret_typ->est_sine_signo)
                                esym_uxth(0, 0);
                            else
                                esym_sxth(0, 0);
                        } else if (rm == 4) {
                            if (ret_typ->est_sine_signo)
                                esym_uxtw(0, 0);
                            else
                                esym_sxtw(0, 0);
                        }
                    }
                    if (r != 0)
                        esym_mov(r, 0);
                }
            }

            reg_vertex = salvati;
            for (int i = 0; i < salvati; i++) {
                if (i == dest)
                    continue;
                esym_ldr64(reg_arm(i), FP, -(reg_save_int_base + i * 8));
                esym_fldr64(reg_arm(i), FP, -(reg_save_fp_base + i * 8));
            }
            break;
        }

    case N_INDEX:
        {
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            if (n->typus && typus_habet_vla(n->typus)) {
                int rsz = reg_alloca();
                genera_magnitudo_typi(n->typus, rsz);
                esym_mul(reg_arm(r2), reg_arm(r2), reg_arm(rsz));
                reg_libera(rsz);
            } else {
                int basis_mag = mag_typi_verus(n->typus);
                if (basis_mag > 1) {
                    esym_movi(17, basis_mag);
                    esym_mul(reg_arm(r2), reg_arm(r2), 17);
                }
            }
            esym_add(r, r, reg_arm(r2));
            reg_libera(r2);
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT) {
                if (typus_est_fluat(n->typus))
                    esym_fload_from_addr(r, r, n->typus);
                else
                    esym_load_from_addr(r, n->typus);
            }
            break;
        }

    case N_MEMBER:
        {
            typus_t   *st_mem = n->sinister ? n->sinister->typus : NULL;
            membrum_t *mb     = quaere_membrum(st_mem, n->nomen);
            if (mb && mb->campus_bitorum > 0) {
                genera_lval(n, dest);
                genera_lectio_campi_bitorum(r, mb);
            } else {
                genera_lval(n, dest);
                if (
                    n->typus && n->typus->genus != TY_ARRAY
                    && n->typus->genus != TY_STRUCT
                ) {
                    if (typus_est_fluat(n->typus))
                        esym_fload_from_addr(r, r, n->typus);
                    else
                        esym_load_from_addr(r, n->typus);
                }
            }
            break;
        }

    case N_ARROW:
        {
            typus_t *st_arr = NULL;
            if (
                n->sinister && n->sinister->typus
                && n->sinister->typus->genus == TY_PTR
            )
                st_arr = n->sinister->typus->basis;
            membrum_t *mb = quaere_membrum(st_arr, n->nomen);
            if (mb && mb->campus_bitorum > 0) {
                genera_lval(n, dest);
                genera_lectio_campi_bitorum(r, mb);
            } else {
                genera_lval(n, dest);
                if (
                    n->typus && n->typus->genus != TY_ARRAY
                    && n->typus->genus != TY_STRUCT
                ) {
                    if (typus_est_fluat(n->typus))
                        esym_fload_from_addr(r, r, n->typus);
                    else
                        esym_load_from_addr(r, n->typus);
                }
            }
            break;
        }

    case N_DEREF:
        genera_expr(n->sinister, dest);
        if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT) {
            if (typus_est_fluat(n->typus))
                esym_fload_from_addr(r, r, n->typus);
            else
                esym_load_from_addr(r, n->typus);
        }
        break;

    case N_ADDR:
        genera_lval(n->sinister, dest);
        break;

    case N_CAST:
        genera_expr(n->sinister, dest);
        if (n->typus_decl) {
            int dest_fluat = typus_est_fluat(n->typus_decl);
            int src_fluat  = typus_est_fluat(n->sinister->typus);
            if (dest_fluat && !src_fluat) {
                esym_int_to_double(r, n->sinister->typus);
            } else if (!dest_fluat && src_fluat) {
                esym_double_to_int(r);
                int tm = mag_typi(n->typus_decl);
                if (tm < 8) {
                    switch (tm) {
                    case 1:
                        if (est_unsigned(n->typus_decl))
                            esym_uxtb(r, r);
                        else
                            esym_sxtb(r, r);
                        break;
                    case 2:
                        if (est_unsigned(n->typus_decl))
                            esym_uxth(r, r);
                        else
                            esym_sxth(r, r);
                        break;
                    case 4:
                        if (!est_unsigned(n->typus_decl))
                            esym_sxtw(r, r);
                        break;
                    default:
                        erratum_ad(n->linea, "cast: magnitudo inexspectata %d", tm);
                    }
                }
            } else if (dest_fluat && src_fluat) {
                /* inter float/double — nihil (d-reg internus) */
            } else {
                int tm = mag_typi(n->typus_decl);
                int sm = n->sinister->typus ? mag_typi(n->sinister->typus) : 8;
                if (tm < sm) {
                    switch (tm) {
                    case 1:
                        if (est_unsigned(n->typus_decl))
                            esym_uxtb(r, r);
                        else
                            esym_sxtb(r, r);
                        break;
                    case 2:
                        if (est_unsigned(n->typus_decl))
                            esym_uxth(r, r);
                        else
                            esym_sxth(r, r);
                        break;
                    case 4:
                        if (!est_unsigned(n->typus_decl))
                            esym_sxtw(r, r);
                        break;
                    default:
                        erratum_ad(n->linea, "cast: magnitudo inexspectata %d", tm);
                    }
                } else if (tm > sm && sm == 4 && !est_unsigned(n->sinister->typus)) {
                    esym_sxtw(r, r);
                } else if (
                    tm == sm && tm < 8 &&
                    est_unsigned(n->typus_decl) != est_unsigned(n->sinister->typus)
                ) {
                    switch (tm) {
                    case 1:
                        if (est_unsigned(n->typus_decl))
                            esym_uxtb(r, r);
                        else
                            esym_sxtb(r, r);
                        break;
                    case 2:
                        if (est_unsigned(n->typus_decl))
                            esym_uxth(r, r);
                        else
                            esym_sxth(r, r);
                        break;
                    case 4:
                        if (!est_unsigned(n->typus_decl))
                            esym_sxtw(r, r);
                        break;
                    default:
                        erratum_ad(n->linea, "cast: magnitudo inexspectata %d", tm);
                    }
                }
            }
        }
        break;

    case N_SIZEOF_TYPE:
        esym_movi(r, n->typus_decl ? typus_magnitudo(n->typus_decl) : 0);
        break;

    case N_SIZEOF_EXPR:
        esym_movi(r, n->sinister && n->sinister->typus ? typus_magnitudo(n->sinister->typus) : 0);
        break;

    case N_VA_START:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            esym_addi(r, FP, 16);
            esym_str64(r, reg_arm(r2), 0);
            reg_libera(r2);
            break;
        }

    case N_VA_ARG:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            esym_ldr64(r, reg_arm(r2), 0);
            int r3 = reg_alloca();
            esym_addi(reg_arm(r3), r, 8);
            esym_str64(reg_arm(r3), reg_arm(r2), 0);
            reg_libera(r3);
            reg_libera(r2);
            if (
                n->typus_decl && n->typus_decl->genus == TY_STRUCT
                && n->typus_decl->magnitudo > 16
            ) {
                esym_ldr64(r, r, 0);
            } else if (n->typus_decl && typus_est_fluat(n->typus_decl)) {
                esym_fload_from_addr(r, r, n->typus_decl);
            } else if (n->typus_decl) {
                esym_load_from_addr(r, n->typus_decl);
            } else {
                esym_ldr64(r, r, 0);
            }
            break;
        }

    case N_VA_END:
        break;

    case N_NOP:
        break;

    case N_BLOCK:
        {
            /* §6.5.2.5: compound literal */
            if (!n->typus || n->typus->genus != TY_STRUCT)
                erratum_ad(n->linea, "compound literal sine typo structurae");
            int mag = n->typus->magnitudo;
            if (mag <= 0)
                erratum_ad(n->linea, "compound literal magnitudinis invalidae: %d", mag);
            int mag_al = (mag + 15) & ~15;
            esym_subi(SP, SP, mag_al);
            esym_addi(r, SP, 0);
            int rv_salvus = reg_vertex;
            if (dest >= rv_salvus)
                rv_salvus = dest + 1;
            {
                reg_vertex = rv_salvus;
                int rz     = reg_alloca();
                esym_movi(reg_arm(rz), 0);
                for (int z = 0; z < mag; z += 8) {
                    int rem = mag - z;
                    if (rem >= 8)
                        esym_str64(reg_arm(rz), r, z);
                    else if (rem >= 4)
                        esym_str32(reg_arm(rz), r, z);
                    else
                        esym_strb(reg_arm(rz), r, z);
                }
                reg_libera(rz);
            }
            for (int i = 0; i < n->num_membrorum; i++) {
                nodus_t *elem = n->membra[i];
                if (elem->init_offset < 0)
                    erratum_ad(
                        elem->linea,
                        "initializator structurae sine offset computato"
                    );
                if (elem->init_size < 1 && elem->init_bitwidth == 0)
                    erratum_ad(
                        elem->linea,
                        "initializator structurae sine magnitudine"
                    );
                if (!elem->typus)
                    erratum_ad(elem->linea, "initializator sine typo");
                reg_vertex = rv_salvus;
                int r2     = reg_alloca();
                genera_expr(elem, r2);
                int eoff = elem->init_offset;
                if (elem->init_bitwidth > 0) {
                    int r3  = reg_alloca();
                    int ra2 = reg_arm(r2);
                    int ra3 = reg_arm(r3);
                    esym_ldr32(ra3, r, eoff);
                    esym_bfi(ra3, ra2, elem->init_bitpos, elem->init_bitwidth);
                    esym_str32(ra3, r, eoff);
                    reg_libera(r3);
                } else if (typus_est_fluat(elem->typus)) {
                    int r3 = reg_alloca();
                    esym_addi(reg_arm(r3), r, eoff);
                    esym_fstore_to_addr(r2, reg_arm(r3), elem->typus);
                    reg_libera(r3);
                } else {
                    int mmag = elem->init_size;
                    esym_store(reg_arm(r2), r, eoff, mmag > 8 ? 8 : mmag);
                }
                reg_libera(r2);
            }
            break;
        }

    default:
        erratum_ad(n->linea, "expressio non supportata: %d", n->genus);
    }
}
