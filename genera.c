/*
 * genera.c — CCC generans codicis ARM64
 *
 * Generat instructiones ARM64 directe in alveum.
 */

#include "utilia.h"
#include "parser.h"
#include "genera.h"
#include "func.h"
#include "emitte.h"
#include "scribo.h"
#include "fluat.h"
#include <string.h>

/* acervus break/continue labels */
static int break_labels[MAX_BREAK];
static int continue_labels[MAX_BREAK];
static int break_vertex = 0;

/* switch context */
static casus_t switch_casus[MAX_CASUS];
static int switch_num_casuum;
static int switch_default_label;
static int in_switch = 0;


/* goto labels intra functionem */
typedef struct {
    char nomen[256];
    int label;
} goto_label_t;

static goto_label_t goto_labels[256];
static int num_goto_labels = 0;

static int goto_label_quaere_vel_crea(const char *nomen)
{
    for (int i = 0; i < num_goto_labels; i++)
        if (strcmp(goto_labels[i].nomen, nomen) == 0)
            return goto_labels[i].label;
    if (num_goto_labels >= 256)
        erratum("nimis multa goto labels");
    int lab = label_novus();
    strncpy(goto_labels[num_goto_labels].nomen, nomen, 255);
    goto_labels[num_goto_labels].label = lab;
    num_goto_labels++;
    return lab;
}

/* status functionis currentis */
static int cur_frame_mag;     /* magnitudo frame */
static int cur_param_num;     /* numerus parametrorum */
static typus_t *cur_func_typus;
static int profunditas_vocationis = 0; /* profunditas vocationum nestarum */

/* registra temporaria: x0-x7, x9-x15 */
static int reg_vertex = 0;

static int reg_alloca(void)
{
    int r = reg_vertex++;
    if (r >= 15)
        erratum("registra exhausta");
    return r;
}

static void reg_libera(int r)
{
    (void)r;
    if (reg_vertex > 0)
        reg_vertex--;
}

static int reg_arm(int slot)
{
    if (slot < 8)
        return slot;      /* x0-x7 */
    return slot + 1;      /* x9-x15 (praetermitte x8) */
}

/* ================================================================
 * generatio expressionum
 * ================================================================ */

static void genera_expr(nodus_t *n, int dest);
static void genera_lval(nodus_t *n, int dest);
static void genera_sententia(nodus_t *n);

static int mag_typi(typus_t *t)
{
    if (!t)
        return 8;
    if (t->genus == TY_PTR || t->genus == TY_ARRAY || t->genus == TY_FUNC)
        return 8;
    if (t->genus == TY_STRUCT)
        return t->magnitudo;
    return t->magnitudo ? t->magnitudo : 4;
}

/* magnitudo vera pro accessu tabulae — non reducit TY_ARRAY ad 8 */
static int mag_typi_verus(typus_t *t)
{
    if (!t)
        return 8;
    if (t->genus == TY_ARRAY)
        return t->magnitudo > 0 ? t->magnitudo : 8;
    return mag_typi(t);
}

static int est_unsigned(typus_t *t)
{
    if (!t)
        return 0;
    return t->est_sine_signo ||
        t->genus == TY_UCHAR || t->genus == TY_USHORT ||
        t->genus == TY_UINT || t->genus == TY_ULONG ||
        t->genus == TY_ULLONG ||
        t->genus == TY_PTR || t->genus == TY_ARRAY;
}

/* §6.7.8: imple regionem memoriae cum zerīs.
 * Cūrat nē extrā fīnēs scrībātur (ultima scrīptūra 4 vel 1 octetī). */
static void emit_imple_zeris(int off_basis, int magnitudo)
{
    if (magnitudo <= 0)
        return;
    emit_movi(0, 0);
    for (int z = 0; z < magnitudo; ) {
        int rem = magnitudo - z;
        emit_movi(17, -(off_basis + z));
        emit_sub(17, FP, 17);
        if (rem >= 8) {
            emit_str64(0, 17, 0);
            z += 8;
        } else if (rem >= 4) {
            emit_str32(0, 17, 0);
            z += 4;
        } else {
            emit_strb(0, 17, 0);
            z += 1;
        }
    }
}

/* ================================================================
 * auxiliaria pro typis fluitantibus
 *
 * Strategia: idem reg_arm(slot) ūtitur, sed valor vivit in
 * d-registrō (non x-registrō) cum typus fluitans est.
 * ================================================================ */

/* carrica constantem fluitantem in d-registrum —
 * §6.4.4.2: per bit-pattern in registrum integrum, deinde FMOV */
static void emit_fconst(int dreg, double val)
{
    long bits;
    memcpy(&bits, &val, 8);
    emit_movi(dreg, bits);       /* MOV Xn, #bits */
    emit_fmov_dx(dreg, dreg);   /* FMOV Dn, Xn */
}

/* carrica valorem fluitantem ex adresse in memoria ad d-registrum */
static void emit_fload_from_addr(int dreg, int addr_reg, typus_t *t)
{
    if (t && t->genus == TY_FLOAT) {
        emit_fldr32(dreg, addr_reg, 0); /* LDR Sn, [Xaddr] */
        emit_fcvt_ds(dreg, dreg);       /* §6.3.1.5: promove ad double */
    } else {
        emit_fldr64(dreg, addr_reg, 0); /* LDR Dn, [Xaddr] */
    }
}

/* salva valorem fluitantem ex d-registrō in memoriam */
static void emit_fstore_to_addr(int dreg, int addr_reg, typus_t *t)
{
    if (t && t->genus == TY_FLOAT) {
        emit_fcvt_sd(dreg, dreg);       /* §6.3.1.5: demove ad float */
        emit_fstr32(dreg, addr_reg, 0); /* STR Sn, [Xaddr] */
    } else {
        emit_fstr64(dreg, addr_reg, 0); /* STR Dn, [Xaddr] */
    }
}

/* converte integrum in registrō Xn ad double in registrō Dn */
static void emit_int_to_double(int reg, typus_t *src_type)
{
    /* §6.3.1.4¶2: integer → double */
    if (est_unsigned(src_type))
        emit_ucvtf_dx(reg, reg);    /* UCVTF Dn, Xn */
    else
        emit_scvtf_dx(reg, reg);    /* SCVTF Dn, Xn */
}

/* converte double in registrō Dn ad integrum in registrō Xn */
static void emit_double_to_int(int reg)
{
    /* §6.3.1.4¶1: truncatio ad zero */
    emit_fcvtzs_xd(reg, reg);      /* FCVTZS Xn, Dn */
}

/* carrica valorem ex l-valor adresse in dest */
static void emit_load_from_addr(int dest, typus_t *t)
{
    int mag = mag_typi(t);
    if (t && (t->genus == TY_STRUCT || t->genus == TY_ARRAY))
        return; /* iam adresse */
    if (est_unsigned(t))
        emit_load_unsigned(dest, dest, 0, mag);
    else
        emit_load(dest, dest, 0, mag);
}

/* genera l-valor (adresse in dest) */
static void genera_lval(nodus_t *n, int dest)
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
                /* variabilis externa — adresse per GOT */
                char got_nomen[260];
                snprintf(got_nomen, 260, "_%s", s->nomen);
                int gid = got_adde(got_nomen);
                emit_adrp_fixup(r, FIX_ADRP_GOT, gid);
                fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                emit_ldr64(r, r, 0);
            } else if (s->est_globalis) {
                int gid = s->globalis_index;
                if (gid < 0)
                    gid = globalis_adde(s->nomen, s->typus, s->est_staticus, 0);
                s->globalis_index = gid;
                emit_adrp_fixup(r, FIX_ADRP_DATA, gid);
                fixup_adde(FIX_ADD_LO12_DATA, codex_lon, gid, 0);
                emit32(0x91000000 | (r << 5) | r); /* ADD Xr, Xr, #lo12 — placeholder */
            } else {
            /* localis — offset a FP */
                int off = s->offset;
                if (off < 0) {
                    emit_movi(r, -off);
                    emit_sub(r, FP, r);
                } else {
                    emit_addi(r, FP, off);
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
            int basis_mag = 1;
            if (n->sinister->typus)
                basis_mag = mag_typi_verus(typus_basis_indicis(n->sinister->typus));
            if (basis_mag > 1) {
                emit_movi(17, basis_mag);
                emit_mul(reg_arm(r2), reg_arm(r2), 17);
            }
            emit_add(r, r, reg_arm(r2));
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
                            emit_addi(r, r, st->membra[i].offset);
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
                            emit_addi(r, r, st->membra[i].offset);
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
static void genera_expr(nodus_t *n, int dest)
{
    if (!n)
        erratum("genera_expr: nodus nullus");

    /* cura ut reg_vertex superet dest, ne allocator eum redat */
    if (dest >= reg_vertex)
        reg_vertex = dest + 1;
    int r = reg_arm(dest);

    switch (n->genus) {
    case N_NUM:
        emit_movi(r, n->valor);
        break;

    /* §6.4.4.2: constans fluitans — carrica bit-pattern, deinde FMOV */
    case N_NUM_FLUAT:
        emit_fconst(r, n->valor_f);
        break;

    case N_STR:
        {
            int sid = chorda_adde(n->chorda, n->lon_chordae);
            emit_adrp_fixup(r, FIX_ADRP, sid);
            fixup_adde(FIX_ADD_LO12, codex_lon, sid, 0);
            emit32(0x91000000 | (r << 5) | r); /* ADD placeholder */
            break;
        }

    case N_IDENT:
        {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            if (!s)
                erratum_ad(n->linea, "symbolum ignotum: '%s'", n->nomen);
            if (s->genus == SYM_FUNC) {
                /* proba si functio est localis */
                int flabel = func_loc_quaere(s->nomen);
                if (flabel >= 0) {
                    /* adresse functionis localis per ADR */
                    fixup_adde(FIX_ADR_LABEL, codex_lon, flabel, 0);
                    emit32(0x10000000 | r); /* ADR Xr, label — placeholder */
                } else {
                    /* functio externa — per GOT */
                    char got_nomen[260];
                    snprintf(got_nomen, 260, "_%s", s->nomen);
                    int gid      = got_adde(got_nomen);
                    s->got_index = gid;
                    emit_adrp_fixup(r, FIX_ADRP_GOT, gid);
                    fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                    emit_ldr64(r, r, 0);
                }
                break;
            }
            genera_lval(n, dest);
            if (s->typus && (s->typus->genus == TY_ARRAY)) {
            /* tabula → adresse iam in r */
                if (s->typus->magnitudo == 0 && s->typus->num_elementorum <= 0)
                    emit_ldr64(r, r, 0); /* §6.7.5.2: VLA — carrica indicem ex slot */
            } else if (s->typus && s->typus->genus == TY_STRUCT) {
            /* structura → adresse */
            } else if (typus_est_fluat(s->typus)) {
            /* §6.2.5¶10: typus fluitans — carrica in d-registrum */
                emit_fload_from_addr(r, r, s->typus);
            } else {
                emit_load_from_addr(r, s->typus);
            }
            break;
        }

    case N_BINOP:
        {
            /* §6.5.13, §6.5.14: short-circuit — dextrum non evaluatur
             * si sinistrum iam determinat resultatum */
            if (n->op == T_AMPAMP) {
                int l_false = label_novus();
                int l_end   = label_novus();
                genera_expr(n->sinister, dest);
                emit_cbz_label(r, l_false);
                genera_expr(n->dexter, dest);
                emit_cbz_label(r, l_false);
                emit_movi(r, 1);
                emit_b_label(l_end);
                label_pone(l_false);
                emit_movi(r, 0);
                label_pone(l_end);
                break;
            }
            if (n->op == T_PIPEPIPE) {
                int l_true = label_novus();
                int l_end  = label_novus();
                genera_expr(n->sinister, dest);
                emit_cbnz_label(r, l_true);
                genera_expr(n->dexter, dest);
                emit_cbnz_label(r, l_true);
                emit_movi(r, 0);
                emit_b_label(l_end);
                label_pone(l_true);
                emit_movi(r, 1);
                label_pone(l_end);
                break;
            }
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            int ra = r, rb = reg_arm(r2);

            /* §6.3.1.8: si unus operandus fluitans, via FP */
            if (typus_est_fluat(n->sinister->typus) || typus_est_fluat(n->dexter->typus)) {
                /* converte operandum integrum ad double si necesse */
                if (!typus_est_fluat(n->sinister->typus))
                    emit_int_to_double(ra, n->sinister->typus);
                if (!typus_est_fluat(n->dexter->typus))
                    emit_int_to_double(rb, n->dexter->typus);

                /* §6.5.5, §6.5.6, Annex F §F.3: operationes FP */
                switch (n->op) {
                case T_PLUS:
                    emit_fadd(ra, ra, rb);
                    break;
                case T_MINUS:
                    emit_fsub(ra, ra, rb);
                    break;
                case T_STAR:
                    emit_fmul(ra, ra, rb);
                    break;
                case T_SLASH:
                    emit_fdiv(ra, ra, rb);
                    break;

                /* §6.5.8, §6.5.9: comparationes FP —
                 * FCMP ponit NZCV, deinde CSET */

                case T_EQEQ:
                    emit_fcmp(ra, rb);
                    emit_cset(ra, COND_EQ);
                    break;
                case T_BANGEQ:
                    emit_fcmp(ra, rb);
                    emit_cset(ra, COND_NE);
                    break;
                case T_LT:
                    emit_fcmp(ra, rb);
                    emit_cset(ra, COND_LT);
                    break;
                case T_GT:
                    emit_fcmp(ra, rb);
                    emit_cset(ra, COND_GT);
                    break;
                case T_LTEQ:
                    emit_fcmp(ra, rb);
                    emit_cset(ra, COND_LE);
                    break;
                case T_GTEQ:
                    emit_fcmp(ra, rb);
                    emit_cset(ra, COND_GE);
                    break;
                default:
                    erratum("operator %d non sustentatur pro typis fluitantibus", n->op);
                }
                reg_libera(r2);
                break;
            }

            switch (n->op) {
            /* §6.5.6.8: ptr + int — scala integrum per magnitudinem elementi */
            case T_PLUS:
                if (typus_est_index(n->sinister->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_mul(rb, rb, 17);
                    }
                }
                emit_add(ra, ra, rb);
                break;
            /* §6.5.6.9: ptr - int scala; ptr - ptr dividitur per elem */
            case T_MINUS:
                if (typus_est_index(n->sinister->typus) && typus_est_integer(n->dexter->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_mul(rb, rb, 17);
                    }
                }
                emit_sub(ra, ra, rb);
                if (typus_est_index(n->sinister->typus) && typus_est_index(n->dexter->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_sdiv(ra, ra, 17);
                    }
                }
                break;
            case T_STAR:
                emit_mul(ra, ra, rb);
                break;

            /* §6.5.5: / et % — unsigned vel signed secundum typum */
            case T_SLASH:
                if (est_unsigned(n->typus))
                    emit_udiv(ra, ra, rb);
                else
                    emit_sdiv(ra, ra, rb);
                break;
            case T_PERCENT:
                if (est_unsigned(n->typus))
                    emit_udiv(17, ra, rb);
                else
                    emit_sdiv(17, ra, rb);
                emit_mul(17, 17, rb);
                emit_sub(ra, ra, 17);
                break;
            case T_AMP:
                emit_and(ra, ra, rb);
                break;
            case T_PIPE:
                emit_orr(ra, ra, rb);
                break;
            case T_CARET:
                emit_eor(ra, ra, rb);
                break;
            case T_LTLT:
                emit_lsl(ra, ra, rb);
                break;

            /* §6.5.7: >> logicus pro unsigned, arithmeticus pro signed */
            case T_GTGT:
                if (est_unsigned(n->sinister->typus))
                    emit_lsr(ra, ra, rb);
                else
                    emit_asr(ra, ra, rb);
                break;
            case T_EQEQ:
                emit_cmp(ra, rb);
                emit_cset(ra, COND_EQ);
                break;
            case T_BANGEQ:
                emit_cmp(ra, rb);
                emit_cset(ra, COND_NE);
                break;

            /* §6.3.1.8: usual arithmetic conversions —
             * si uterque operandus unsigned, comparatio unsigned */
            case T_LT:
                emit_cmp(ra, rb);
                emit_cset(ra, (est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus)) ? COND_LO : COND_LT);
                break;
            case T_GT:
                emit_cmp(ra, rb);
                emit_cset(ra, (est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus)) ? COND_HI : COND_GT);
                break;
            case T_LTEQ:
                emit_cmp(ra, rb);
                emit_cset(ra, (est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus)) ? COND_LS : COND_LE);
                break;
            case T_GTEQ:
                emit_cmp(ra, rb);
                emit_cset(ra, (est_unsigned(n->sinister->typus) || est_unsigned(n->dexter->typus)) ? COND_HS : COND_GE);
                break;
            case T_AMPAMP:
                {
                    int l_false = label_novus();
                    int l_end   = label_novus();
                    emit_cbz_label(ra, l_false);
                    emit_cbz_label(rb, l_false);
                    emit_movi(ra, 1);
                    emit_b_label(l_end);
                    label_pone(l_false);
                    emit_movi(ra, 0);
                    label_pone(l_end);
                    break;
                }
            case T_PIPEPIPE:
                {
                    int l_true = label_novus();
                    int l_end  = label_novus();
                    emit_cbnz_label(ra, l_true);
                    emit_cbnz_label(rb, l_true);
                    emit_movi(ra, 0);
                    emit_b_label(l_end);
                    label_pone(l_true);
                    emit_movi(ra, 1);
                    label_pone(l_end);
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
                /* §6.5.3.3: negatio — FNEG pro fluitantibus, NEG pro integris */
                if (typus_est_fluat(n->typus))
                    emit_fneg(r, r);
                else
                    emit_neg(r, r);
                break;
            case T_TILDE:
                emit_mvn(r, r);
                break;
            case T_BANG:
                emit_cmpi(r, 0);
                emit_cset(r, COND_EQ);
                break;
            case T_PLUSPLUS:
                /* pre-increment */
                {
                    int r2 = reg_alloca();
                    genera_lval(n->sinister, r2);
                    int mag = mag_typi(n->typus);
                    int inc = 1;
                    if (typus_est_index(n->typus))
                        inc = mag_typi(typus_basis_indicis(n->typus));
                    if (est_unsigned(n->typus))
                        emit_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    else
                        emit_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    emit_addi(r, r, inc);
                    emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    reg_libera(r2);
                    break;
                }
            case T_MINUSMINUS:
                {
                    int r2 = reg_alloca();
                    genera_lval(n->sinister, r2);
                    int mag = mag_typi(n->typus);
                    int dec = 1;
                    if (typus_est_index(n->typus))
                        dec = mag_typi(typus_basis_indicis(n->typus));
                    if (est_unsigned(n->typus))
                        emit_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    else
                        emit_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    emit_subi(r, r, dec);
                    emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
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
            int mag = mag_typi(n->typus);
            if (est_unsigned(n->typus))
                emit_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            else
                emit_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            int inc = 1;
            if (typus_est_index(n->typus))
                inc = mag_typi(typus_basis_indicis(n->typus));
            if (n->op == T_PLUSPLUS)
                emit_addi(17, r, inc);
            else
                emit_subi(17, r, inc);
            emit_store(17, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            reg_libera(r2);
            break;
        }

    case N_ASSIGN:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            genera_expr(n->dexter, dest);
            int mag = mag_typi(n->sinister->typus);
            if (n->sinister->typus && n->sinister->typus->genus == TY_STRUCT) {
                /* copia structurae */
                for (int i = 0; i < mag; i += 8) {
                    int rem = mag - i;
                    if (rem >= 8) {
                        emit_ldr64(17, r, i);
                        emit_str64(17, reg_arm(r2), i);
                    }else if (rem >= 4) {
                        emit_ldr32(17, r, i);
                        emit_str32(17, reg_arm(r2), i);
                    }else {
                        emit_ldrb(17, r, i);
                        emit_strb(17, reg_arm(r2), i);
                    }
                }
            } else if (typus_est_fluat(n->sinister->typus)) {
                /* §6.5.16.1: assignatio fluitans — converte si necesse, salva per FP STR */
                if (!typus_est_fluat(n->dexter->typus))
                    emit_int_to_double(r, n->dexter->typus);
                emit_fstore_to_addr(r, reg_arm(r2), n->sinister->typus);
            } else {
                emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            }
            reg_libera(r2);
            break;
        }

    /* §6.5.16.2: assignatio composita — E1 op= E2 ≡ E1 = E1 op E2 */
    case N_OPASSIGN:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            int mag = mag_typi(n->sinister->typus);
            if (typus_est_fluat(n->sinister->typus)) {
                /* §6.5.16.2, Annex F §F.3: assignatio composita fluitans */
                emit_fload_from_addr(r, reg_arm(r2), n->sinister->typus);
                int r3 = reg_alloca();
                genera_expr(n->dexter, r3);
                int rb = reg_arm(r3);
                if (!typus_est_fluat(n->dexter->typus))
                    emit_int_to_double(rb, n->dexter->typus);
                switch (n->op) {
                case T_PLUSEQ:
                    emit_fadd(r, r, rb);
                    break;
                case T_MINUSEQ:
                    emit_fsub(r, r, rb);
                    break;
                case T_STAREQ:
                    emit_fmul(r, r, rb);
                    break;
                case T_SLASHEQ:
                    emit_fdiv(r, r, rb);
                    break;
                default:
                    erratum_ad(
                        n->linea,
                        "operator %d non sustentatur pro typis fluitantibus", n->op
                    );
                }
                reg_libera(r3);
                emit_fstore_to_addr(r, reg_arm(r2), n->sinister->typus);
            } else {
                /* carrica valorem currentem */
                if (est_unsigned(n->sinister->typus))
                    emit_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                else
                    emit_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                int r3 = reg_alloca();
                genera_expr(n->dexter, r3);
                int rb = reg_arm(r3);
                switch (n->op) {
                case T_PLUSEQ:
                    if (typus_est_index(n->sinister->typus)) {
                        int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                        if (bm > 1) {
                            emit_movi(17, bm);
                            emit_mul(rb, rb, 17);
                        }
                    }
                    emit_add(r, r, rb);
                    break;
                case T_MINUSEQ:
                    if (typus_est_index(n->sinister->typus)) {
                        int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                        if (bm > 1) {
                            emit_movi(17, bm);
                            emit_mul(rb, rb, 17);
                        }
                    }
                    emit_sub(r, r, rb);
                    break;
                case T_STAREQ:
                    emit_mul(r, r, rb);
                    break;
                case T_SLASHEQ:
                    emit_sdiv(r, r, rb);
                    break;
                case T_PERCENTEQ:
                    emit_sdiv(17, r, rb);
                    emit_mul(17, 17, rb);
                    emit_sub(r, r, 17);
                    break;
                case T_AMPEQ:
                    emit_and(r, r, rb);
                    break;
                case T_PIPEEQ:
                    emit_orr(r, r, rb);
                    break;
                case T_CARETEQ:
                    emit_eor(r, r, rb);
                    break;
                case T_LTLTEQ:
                    emit_lsl(r, r, rb);
                    break;

                /* §6.5.7: >>= logicus pro unsigned, arithmeticus pro signed */
                case T_GTGTEQ:
                    if (est_unsigned(n->sinister->typus))
                        emit_lsr(r, r, rb);
                    else
                        emit_asr(r, r, rb);
                    break;
                }
                reg_libera(r3);
                emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            }
            reg_libera(r2);
            break;
        }

    case N_TERNARY:
        {
            int l_false = label_novus(), l_end = label_novus();
            genera_expr(n->sinister, dest);
            emit_cbz_label(r, l_false);
            genera_expr(n->dexter, dest);
            emit_b_label(l_end);
            label_pone(l_false);
            genera_expr(n->tertius, dest);
            label_pone(l_end);
            break;
        }

    case N_COMMA_EXPR:
        genera_expr(n->sinister, dest);
        genera_expr(n->dexter, dest);
        break;

    case N_CALL:
        /* salva registra viva */
        {
            int salvati = reg_vertex;
            for (int i = 0; i < salvati; i++)
                emit_str64(reg_arm(i), FP, -(cur_frame_mag - 16 - i * 8));

            /* evaluare argumenta et salvare in acervo temporario.
             * §6.5.2.2¶6: argumenta float promota ad double (default argument promotions).
             * Valores fluitantes salvantur per FP STR (bit-pattern IEC 60559). */
            int nargs = n->num_membrorum;
            int arg_spill_base = cur_frame_mag - 16 - 15 * 8
                + profunditas_vocationis * 8 * 8;
            profunditas_vocationis++;

            /* pro vocationibus indirectis: salva indicem functionis ante
             * argumenta in registris carricantur, ne x0 cloberetur. */
            int fptr_spill    = arg_spill_base + (nargs + 1) * 8;
            int est_indirecta = 0;
            if (n->sinister->genus == N_IDENT) {
                symbolum_t *si = n->sinister->sym ? n->sinister->sym : ambitus_quaere_omnes(n->sinister->nomen);
                if (si && si->genus != SYM_FUNC && func_loc_quaere(n->sinister->nomen) < 0)
                    est_indirecta = 1;
            } else {
                est_indirecta = 1;
            }
            if (est_indirecta) {
                reg_vertex = 0;
                genera_expr(n->sinister, 0);
                emit_str64(0, FP, -(fptr_spill));
            }
            for (int i = 0; i < nargs; i++) {
                reg_vertex = 0;
                genera_expr(n->membra[i], 0);
                if (typus_est_fluat(n->membra[i]->typus)) {
                    /* §6.5.2.2¶6: salva bit-pattern duplicis (IEC 60559) per FP STR.
                     * Computa adresse quia FP STR non tractat offsets negativos. */
                    int off = arg_spill_base + i * 8;
                    emit_movi(17, off);
                    emit_sub(17, FP, 17);
                    emit_fstr64(0, 17, 0);
                } else {
                    emit_str64(0, FP, -(arg_spill_base + i * 8));
                }
            }

            /* ABI Apple ARM64: argumenta variadica in acervo, non in registris.
             * determinare quot parametri nominati sint. */

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

            /* determinare typos parametrorum nominatorum */
            int num_fp_regs = 0;  /* d-registra usata */
            int num_gp_regs = 0;  /* x-registra usata */

            /* §6.5.2.2: nominati in registris x0-x7 (integri) vel d0-d7 (fluitantes) */
            for (int i = 0; i < num_nominati && i < nargs; i++) {
                typus_t *pt = NULL;
                if (func_typ && i < func_typ->num_parametrorum)
                    pt = func_typ->parametri[i];
                if (!pt)
                    pt = n->membra[i]->typus;
                if (typus_est_fluat(pt)) {
                    if (num_fp_regs < 8) {
                        int off = arg_spill_base + i * 8;
                        emit_movi(17, off);
                        emit_sub(17, FP, 17);
                        emit_fldr64(num_fp_regs, 17, 0);
                    }
                    num_fp_regs++;
                } else {
                    if (num_gp_regs < 8)
                        emit_ldr64(num_gp_regs, FP, -(arg_spill_base + i * 8));
                    num_gp_regs++;
                }
            }

            /* si variadica: reliqua argumenta in acervo.
             * §6.5.2.2¶6: float → double (default argument promotions).
             * Omnia variadica per integer LDR/STR (bit-patterns). */
            int acervus_args = 0;
            if (est_variadica && nargs > num_nominati) {
                acervus_args    = nargs - num_nominati;
                int acervus_mag = ((acervus_args * 8) + 15) & ~15;
                emit_subi(SP, SP, acervus_mag);
                for (int i = 0; i < acervus_args; i++) {
                    emit_ldr64(17, FP, -(arg_spill_base + (num_nominati + i) * 8));
                    emit_str64(17, SP, i * 8);
                }
            } else if (!est_variadica) {
                /* non variadica: superflua argumenta in registris vel acervo */
                for (int i = num_nominati; i < nargs; i++) {
                    typus_t *pt = n->membra[i]->typus;
                    if (typus_est_fluat(pt)) {
                        if (num_fp_regs < 8) {
                            int off = arg_spill_base + i * 8;
                            emit_movi(17, off);
                            emit_sub(17, FP, 17);
                            emit_fldr64(num_fp_regs, 17, 0);
                            num_fp_regs++;
                        }
                    } else {
                        if (num_gp_regs < 8) {
                            emit_ldr64(num_gp_regs, FP, -(arg_spill_base + i * 8));
                            num_gp_regs++;
                        }
                    }
                }
                /* superflua in acervo */
                if (nargs > 8) {
                    int extra     = nargs - 8;
                    int extra_mag = ((extra * 8) + 15) & ~15;
                    emit_subi(SP, SP, extra_mag);
                    for (int i = 8; i < nargs; i++) {
                        emit_ldr64(17, FP, -(arg_spill_base + i * 8));
                        emit_str64(17, SP, (i - 8) * 8);
                    }
                    acervus_args = extra;
                }
            }

        /* voca functionem */
            reg_vertex = 0;
            if (n->sinister->genus == N_IDENT) {
                /* proba si functio est localis */
                int flabel = func_loc_quaere(n->sinister->nomen);
                if (flabel >= 0) {
                    /* vocationem directam per BL */
                    emit_bl_label(flabel);
                } else {
                    symbolum_t *s = n->sinister->sym ? n->sinister->sym : ambitus_quaere_omnes(n->sinister->nomen);
                    if (s && s->genus != SYM_FUNC) {
                        /* index functionis — carrica ex spill (iam salvatum ante args) */
                        emit_ldr64(16, FP, -(fptr_spill));
                        emit_blr(16);
                    } else {
                        /* functio externa — per GOT */
                        char got_nomen[260];
                        snprintf(got_nomen, 260, "_%s", n->sinister->nomen);
                        int gid = got_adde(got_nomen);
                        if (s)
                            s->got_index = gid;
                        emit_adrp_fixup(16, FIX_ADRP_GOT, gid);
                        fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                        emit_ldr64(16, 16, 0);
                        emit_blr(16);
                    }
                }
            } else {
                /* expressio indirecta — carrica ex spill */
                emit_ldr64(16, FP, -(fptr_spill));
                emit_blr(16);
            }

            profunditas_vocationis--;

            if (acervus_args > 0) {
                int acervus_mag = ((acervus_args * 8) + 15) & ~15;
                emit_addi(SP, SP, acervus_mag);
            }

            /* resultatum: x0 pro integris, d0 pro fluitantibus */
            {
                typus_t *ret_typ = NULL;
                if (func_typ)
                    ret_typ = func_typ->reditus;
                if (typus_est_fluat(ret_typ)) {
                    /* d0 → d(dest) */
                    if (r != 0) {
                        /* FMOV Dd, D0 — codificatio: 0x1E604000 | (src << 5) | dest */
                        emit32(0x1E604000 | (0 << 5) | r);
                    }
                } else {
                    if (r != 0)
                        emit_mov(r, 0);
                }
            }

            /* restitue registra — praetermitte dest ne resultatum deleatur */
            reg_vertex = salvati;
            for (int i = 0; i < salvati; i++) {
                if (i == dest)
                    continue; /* hic iam habet resultatum */
                emit_ldr64(reg_arm(i), FP, -(cur_frame_mag - 16 - i * 8));
            }
            break;
        }

    case N_INDEX:
        {
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            int basis_mag = mag_typi_verus(n->typus);
            if (basis_mag > 1) {
                emit_movi(17, basis_mag);
                emit_mul(reg_arm(r2), reg_arm(r2), 17);
            }
            emit_add(r, r, reg_arm(r2));
            reg_libera(r2);
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
                emit_load_from_addr(r, n->typus);
            break;
        }

    case N_MEMBER:
        {
            genera_lval(n, dest);
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
                emit_load_from_addr(r, n->typus);
            break;
        }

    case N_ARROW:
        {
            genera_lval(n, dest);
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
                emit_load_from_addr(r, n->typus);
            break;
        }

    case N_DEREF:
        genera_expr(n->sinister, dest);
        if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
            emit_load_from_addr(r, n->typus);
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
                /* §6.3.1.4¶2: integer → fluitans */
                emit_int_to_double(r, n->sinister->typus);
                if (n->typus_decl->genus == TY_FLOAT)
                    emit_fcvt_sd(r, r);  /* §6.3.1.5: demove ad float */
            } else if (!dest_fluat && src_fluat) {
                /* §6.3.1.4¶1: fluitans → integer (truncatio) */
                emit_double_to_int(r);
                /* truncatio ad minorem typum integrum si necesse */
                int tm = mag_typi(n->typus_decl);
                if (tm < 8) {
                    switch (tm) {
                    case 1:
                        if (est_unsigned(n->typus_decl))
                            emit_uxtb(r, r);
                        else
                            emit_sxtb(r, r);
                        break;
                    case 2:
                        if (est_unsigned(n->typus_decl))
                            emit_uxth(r, r);
                        else
                            emit_sxth(r, r);
                        break;
                    case 4:
                        if (!est_unsigned(n->typus_decl))
                            emit_sxtw(r, r);
                        break;
                    }
                }
            } else if (dest_fluat && src_fluat) {
                /* §6.3.1.5: inter typos fluitantes */
                if (n->typus_decl->genus == TY_FLOAT && n->sinister->typus->genus == TY_DOUBLE)
                    emit_fcvt_sd(r, r);
                else if (n->typus_decl->genus == TY_DOUBLE && n->sinister->typus->genus == TY_FLOAT)
                    emit_fcvt_ds(r, r);
            } else {
                /* truncatio vel extensio integra */
                int tm = mag_typi(n->typus_decl);
                int sm = n->sinister->typus ? mag_typi(n->sinister->typus) : 8;
                if (tm < sm) {
                    switch (tm) {
                    case 1:
                        if (est_unsigned(n->typus_decl))
                            emit_uxtb(r, r);
                        else
                            emit_sxtb(r, r);
                        break;
                    case 2:
                        if (est_unsigned(n->typus_decl))
                            emit_uxth(r, r);
                        else
                            emit_sxth(r, r);
                        break;
                    case 4:
                        if (!est_unsigned(n->typus_decl))
                            emit_sxtw(r, r);
                        break;
                    }
                } else if (tm > sm && sm == 4 && !est_unsigned(n->sinister->typus)) {
                    emit_sxtw(r, r);
                } else if (
                    tm == sm && tm < 8 &&
                    est_unsigned(n->typus_decl) != est_unsigned(n->sinister->typus)
                ) {
                    /* eadem magnitudo, diversa signatura — re-extende */
                    switch (tm) {
                    case 1:
                        if (est_unsigned(n->typus_decl))
                            emit_uxtb(r, r);
                        else
                            emit_sxtb(r, r);
                        break;
                    case 2:
                        if (est_unsigned(n->typus_decl))
                            emit_uxth(r, r);
                        else
                            emit_sxth(r, r);
                        break;
                    case 4:
                        if (!est_unsigned(n->typus_decl))
                            emit_sxtw(r, r);
                        break;
                    }
                }
            }
        }
        break;

    case N_SIZEOF_TYPE:
        emit_movi(r, n->typus_decl ? typus_magnitudo(n->typus_decl) : 0);
        break;

    case N_SIZEOF_EXPR:
        emit_movi(
            r, n->sinister && n->sinister->typus ?
            typus_magnitudo(n->sinister->typus) : 0
        );
        break;

    /* va_start(ap, ultimum): ap = FP + 16 (argumenta variadica in acervo vocantis) */
    case N_VA_START:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);  /* &ap */
            emit_addi(r, FP, 16);          /* r = FP + 16 */
            emit_str64(r, reg_arm(r2), 0); /* ap = r */
            reg_libera(r2);
            break;
        }

    /* va_arg(ap, typus): reddit *(typus*)ap, deinde ap += 8 */
    case N_VA_ARG:
        {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);      /* r2 = &ap */
            emit_ldr64(r, reg_arm(r2), 0);     /* r = ap */
            int r3 = reg_alloca();
            emit_addi(reg_arm(r3), r, 8);      /* r3 = ap + 8 */
            emit_str64(reg_arm(r3), reg_arm(r2), 0); /* ap = ap + 8 */
            reg_libera(r3);
            reg_libera(r2);
            /* carrica valorem ex *ap */
            if (n->typus_decl)
                emit_load_from_addr(r, n->typus_decl);
            else
                emit_ldr64(r, r, 0);
            break;
        }

    /* va_end(ap): nihil agit */
    case N_VA_END:
        break;

    case N_NOP:
        break;

    default:
        erratum_ad(n->linea, "expressio non supportata: %d", n->genus);
    }
}

/* ================================================================
 * generatio sententiarum
 * ================================================================ */

static void genera_sententia(nodus_t *n)
{
    if (!n)
        return;

    switch (n->genus) {
    case N_NOP:
        break;

    case N_BLOCK:
        for (int i = 0; i < n->num_membrorum; i++)
            genera_sententia(n->membra[i]);
        break;

    case N_EXPR_STMT:
        reg_vertex = 0;
        if (n->sinister)
            genera_expr(n->sinister, 0);
        reg_vertex = 0;
        break;

    case N_VAR_DECL:
        {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            /* §6.7.5.2: VLA — allocatio dynamica in acervo */
            if (n->tertius && s && !s->est_globalis) {
                reg_vertex = 0;
                genera_expr(n->tertius, 0); /* magnitudo in x0 */
                /* multiplica per magnitudinem elementi */
                typus_t *elem = n->typus_decl ? n->typus_decl->basis : ty_int;
                int emag      = typus_magnitudo(elem);
                if (emag < 1)
                    emag = 4;
                if (emag > 1) {
                    emit_movi(17, emag);
                    emit_mul(0, 0, 17);
                }
                /* allinea ad 16 */
                emit_addi(0, 0, 15);
                emit_movi(17, ~(long)15);
                emit_and(0, 0, 17);
                /* SUB SP, SP, magnitudo (per x16 quia SUB non tractat SP) */
                emit_addi(16, SP, 0); /* x16 = SP */
                emit_sub(16, 16, 0);  /* x16 = x16 - mag */
                emit_addi(SP, 16, 0); /* SP = x16 */
                /* serva adresse tabulae in slotum variabilis */
                int off = s->offset;
                emit_movi(17, -off);
                emit_sub(17, FP, 17);
                emit_str64(16, 17, 0); /* *slot = adresse tabulae */
                reg_vertex = 0;
                break;
            }
            if (s && (s->est_globalis || s->est_staticus) && !s->est_externus) {
            /* globalis/statica (non extern) */
                if (s->globalis_index < 0) {
                    long val = 0;
                    if (n->sinister && n->sinister->genus == N_NUM)
                        val = n->sinister->valor;
                    int gid = globalis_adde(n->nomen, n->typus_decl, n->est_staticus, val);
                    s->globalis_index = gid;
                }
            } else if (n->num_membrorum > 0 && n->membra) {
            /* §6.7.8: initiale tabulae vel structurae { expr, expr, ... } */
                if (s) {
                    int off_basis = s->offset;
                    int tot_mag = (s->typus && s->typus->genus == TY_ARRAY) ?
                        s->typus->magnitudo : mag_typi(s->typus);
                    emit_imple_zeris(off_basis, tot_mag);
                    if (
                        s->typus && s->typus->genus == TY_STRUCT &&
                        s->typus->membra && s->typus->num_membrorum > 0
                    ) {
                        /* structura: utere offset et magnitudine cuiusque campi */
                        for (int i = 0; i < n->num_membrorum && i < s->typus->num_membrorum; i++) {
                            reg_vertex = 0;
                            genera_expr(n->membra[i], 0);
                            int moff = off_basis + s->typus->membra[i].offset;
                            int mmag = mag_typi(s->typus->membra[i].typus);
                            emit_movi(17, -moff);
                            emit_sub(17, FP, 17);
                            emit_store(0, 17, 0, mmag);
                            reg_vertex = 0;
                        }
                    } else {
                        /* tabula: elementa eiusdem magnitudinis */
                        typus_t *elem_typus = (s->typus && s->typus->genus == TY_ARRAY) ?
                            s->typus->basis : ty_int;
                        while (elem_typus && elem_typus->genus == TY_ARRAY)
                            elem_typus = elem_typus->basis;
                        int elem_mag = typus_magnitudo(elem_typus);
                        if (elem_mag < 1)
                            elem_mag = 4;
                        for (int i = 0; i < n->num_membrorum; i++) {
                            reg_vertex = 0;
                            genera_expr(n->membra[i], 0);
                            int elem_off = off_basis + i * elem_mag;
                            emit_movi(17, -elem_off);
                            emit_sub(17, FP, 17);
                            emit_store(0, 17, 0, elem_mag);
                            reg_vertex = 0;
                        }
                    }
                }
            } else if (
                n->sinister && n->sinister->genus == N_BLOCK &&
                n->sinister->typus &&
                (
                    n->sinister->typus->genus == TY_STRUCT ||
                    n->sinister->typus->genus == TY_ARRAY
                ) && s
            ) {
            /* compound literal — scribe membra singulariter */
                {
                    int off     = s->offset;
                    typus_t *st = n->sinister->typus;
                    int tot_mag = st->magnitudo > 0 ? st->magnitudo : mag_typi(st);
                    emit_imple_zeris(off, tot_mag);
                    /* scribe singula elementa */
                    if (st->genus == TY_STRUCT && st->membra) {
                        for (int i = 0; i < n->sinister->num_membrorum && i < st->num_membrorum; i++) {
                            reg_vertex = 0;
                            genera_expr(n->sinister->membra[i], 0);
                            int moff = off + st->membra[i].offset;
                            int mmag = mag_typi(st->membra[i].typus);
                            emit_movi(17, -moff);
                            emit_sub(17, FP, 17);
                            emit_store(0, 17, 0, mmag);
                            reg_vertex = 0;
                        }
                    } else {
                        /* tabula */
                        typus_t *elem_t = st->basis ? st->basis : ty_int;
                        int emag        = typus_magnitudo(elem_t);
                        if (emag < 1)
                            emag = 4;
                        for (int i = 0; i < n->sinister->num_membrorum; i++) {
                            reg_vertex = 0;
                            genera_expr(n->sinister->membra[i], 0);
                            emit_movi(17, -(off + i * emag));
                            emit_sub(17, FP, 17);
                            emit_store(0, 17, 0, emag);
                            reg_vertex = 0;
                        }
                    }
                }
            } else if (
                n->sinister && n->sinister->genus == N_STR &&
                s && s->typus && s->typus->genus == TY_ARRAY &&
                s->typus->basis && (
                    s->typus->basis->genus == TY_CHAR ||
                    s->typus->basis->genus == TY_UCHAR
                )
            ) {
            /* initiale tabulae char cum chorda litterali */
                {
                    int off     = s->offset;
                    int arr_mag = s->typus->magnitudo;
                    emit_imple_zeris(off, arr_mag);
                    /* deinde copia characteres chordae */
                    const char *str = n->sinister->chorda;
                    int slen        = n->sinister->lon_chordae;
                    for (int i = 0; i <= slen && i < arr_mag; i++) {
                        emit_movi(0, (unsigned char)str[i]);
                        emit_movi(17, -(off + i));
                        emit_sub(17, FP, 17);
                        emit_strb(0, 17, 0);
                    }
                }
                reg_vertex = 0;
            } else if (n->sinister) {
            /* initiale scalaris */
                reg_vertex = 0;
                genera_expr(n->sinister, 0);
                if (s) {
                    int mag = mag_typi(s->typus);
                    int off = s->offset;
                    if (s->typus && s->typus->genus == TY_STRUCT) {
                    /* copia structurae */
                        emit_movi(17, -off);
                        emit_sub(17, FP, 17);
                        for (int i = 0; i < mag; i += 8) {
                            int rem = mag - i;
                            if (rem >= 8) {
                                emit_ldr64(16, 0, i);
                                emit_str64(16, 17, i);
                            }else if (rem >= 4) {
                                emit_ldr32(16, 0, i);
                                emit_str32(16, 17, i);
                            }else {
                                emit_ldrb(16, 0, i);
                                emit_strb(16, 17, i);
                            }
                        }
                    } else if (typus_est_fluat(s->typus)) {
                    /* §6.7.8, §6.3.1.4¶2: initiale fluitans — converte integer si necesse */
                        if (!typus_est_fluat(n->sinister->typus))
                            emit_int_to_double(0, n->sinister->typus);
                        emit_movi(17, -off);
                        emit_sub(17, FP, 17);
                        emit_fstore_to_addr(0, 17, s->typus);
                    } else {
                        emit_movi(17, -off);
                        emit_sub(17, FP, 17);
                        emit_store(0, 17, 0, mag > 8 ? 8 : mag);
                    }
                }
                reg_vertex = 0;
            }
            break;
        }

    case N_IF:
        {
            int l_else = label_novus(), l_end = label_novus();
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            emit_cbz_label(0, n->tertius ? l_else : l_end);
            reg_vertex = 0;
            genera_sententia(n->dexter);
            if (n->tertius) {
                emit_b_label(l_end);
                label_pone(l_else);
                genera_sententia(n->tertius);
            }
            label_pone(l_end);
            break;
        }

    case N_WHILE:
        {
            int l_cond = label_novus(), l_end = label_novus();
            int l_cont = l_cond;
            if (break_vertex >= MAX_BREAK)
                erratum("nimis profunda nidificatio iterationum");
            break_labels[break_vertex]    = l_end;
            continue_labels[break_vertex] = l_cont;
            break_vertex++;
            label_pone(l_cond);
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            emit_cbz_label(0, l_end);
            reg_vertex = 0;
            genera_sententia(n->dexter);
            emit_b_label(l_cond);
            label_pone(l_end);
            break_vertex--;
            break;
        }

    case N_DOWHILE:
        {
            int l_top = label_novus(), l_cond = label_novus(), l_end = label_novus();
            if (break_vertex >= MAX_BREAK)
                erratum("nimis profunda nidificatio iterationum");
            break_labels[break_vertex]    = l_end;
            continue_labels[break_vertex] = l_cond;
            break_vertex++;
            label_pone(l_top);
            genera_sententia(n->dexter);
            label_pone(l_cond);
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            emit_cbnz_label(0, l_top);
            label_pone(l_end);
            break_vertex--;
            break;
        }

    case N_FOR:
        {
            int l_cond = label_novus(), l_inc = label_novus(), l_end = label_novus();
            if (break_vertex >= MAX_BREAK)
                erratum("nimis profunda nidificatio iterationum");
            break_labels[break_vertex]    = l_end;
            continue_labels[break_vertex] = l_inc;
            break_vertex++;
            if (n->sinister) {
                reg_vertex = 0;
                genera_sententia(n->sinister);
            }
            label_pone(l_cond);
            if (n->dexter) {
                reg_vertex = 0;
                genera_expr(n->dexter, 0);
                emit_cbz_label(0, l_end);
            }
            reg_vertex = 0;
            genera_sententia(n->quartus);
            label_pone(l_inc);
            if (n->tertius) {
                reg_vertex = 0;
                genera_expr(n->tertius, 0);
            }
            emit_b_label(l_cond);
            label_pone(l_end);
            break_vertex--;
            reg_vertex = 0;
            break;
        }

    case N_SWITCH:
        /* collige casus ex corpore */
        {
            int l_end         = label_novus();
            int old_in_switch = in_switch;
            casus_t old_casus[MAX_CASUS];
            int old_num     = switch_num_casuum;
            int old_default = switch_default_label;
            memcpy(old_casus, switch_casus, sizeof(old_casus));

            switch_num_casuum = 0;
            switch_default_label = -1;
            in_switch = 1;

            if (break_vertex >= MAX_BREAK)
                erratum("nimis profunda nidificatio iterationum");
            break_labels[break_vertex] = l_end;
            continue_labels[break_vertex] = break_vertex > 0 ?
                continue_labels[break_vertex - 1] : l_end;
            break_vertex++;

            /* evaluare expressionem switch */
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            /* salva in acervo */
            emit_str64(0, FP, -(cur_frame_mag - 16 - 15 * 8 - 8));

            /* pre-scan corpus pro casus et default labels */
            /* genera corpus — casus labels ponentur suis locis */
            int l_dispatch = label_novus();
            emit_b_label(l_dispatch); /* salta ad dispatch */

            /* genera corpus */
            reg_vertex = 0;
            genera_sententia(n->dexter);
            emit_b_label(l_end);

            /* dispatch tabula */
            label_pone(l_dispatch);
            emit_ldr64(0, FP, -(cur_frame_mag - 16 - 15 * 8 - 8));
            for (int i = 0; i < switch_num_casuum; i++) {
                emit_movi(17, switch_casus[i].valor);
                emit_cmp(0, 17);
                emit_bcond_label(COND_EQ, switch_casus[i].label);
            }
            if (switch_default_label >= 0)
                emit_b_label(switch_default_label);
            else
                emit_b_label(l_end);

            label_pone(l_end);
            break_vertex--;

            in_switch = old_in_switch;
            memcpy(switch_casus, old_casus, sizeof(old_casus));
            switch_num_casuum    = old_num;
            switch_default_label = old_default;
            break;
        }

    case N_CASE:
        {
            int l = label_novus();
            /* §5.2.4.1: 1023 case labels maximum */
            if (switch_num_casuum >= MAX_CASUS)
                erratum_ad(n->linea, "nimis multi casus in switch");
            switch_casus[switch_num_casuum].valor = n->valor;
            switch_casus[switch_num_casuum].label = l;
            switch_num_casuum++;
            label_pone(l);
            if (n->dexter)
                genera_sententia(n->dexter);
            break;
        }

    case N_DEFAULT:
        {
            int l = label_novus();
            switch_default_label = l;
            label_pone(l);
            if (n->dexter)
                genera_sententia(n->dexter);
            break;
        }

    case N_RETURN:
        if (n->sinister) {
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
        }
        /* epilogus */
        emit_addi(SP, FP, 0);
        emit_ldp_post(FP, LR, SP, 16);
        emit_ret();
        break;

    /* §6.8.6.3: "shall appear only in or as a switch body or loop body" */
    case N_BREAK:
        if (break_vertex <= 0)
            erratum_ad(n->linea, "break extra iterationem vel switch");
        emit_b_label(break_labels[break_vertex - 1]);
        break;

    /* §6.8.6.2: "shall appear only in or as a loop body" */
    case N_CONTINUE:
        if (break_vertex <= 0)
            erratum_ad(n->linea, "continue extra iterationem");
        emit_b_label(continue_labels[break_vertex - 1]);
        break;

    case N_GOTO:
        {
            int lab = goto_label_quaere_vel_crea(n->nomen);
            emit_b_label(lab);
        }
        break;

    case N_LABEL:
        {
            int lab = goto_label_quaere_vel_crea(n->nomen);
            label_pone(lab);
            if (n->sinister)
                genera_sententia(n->sinister);
        }
        break;

    case N_FUNC_DEF:
        /* tractatur in genera_translatio */
        break;

    default:
        /* try as expression */
        reg_vertex = 0;
        genera_expr(n, 0);
        reg_vertex = 0;
        break;
    }
}

/* ================================================================
 * generatio functionis
 * ================================================================ */

static void genera_functio(nodus_t *n)
{
    /* computare magnitudinem frame */
    int nparams    = (int)n->sinister->valor;
    int locals_mag = 0;

    /* scan corpus pro variabilibus localibus */
    /* usamus offset iam computatos a parsore */
    /* invenire minimum offset (maximus negativus) */
    /* simpliciter: frame = parametri + 128 spill + 256 extra */
    /* n->op continet profunditatem acervi maximam a parsore */
    int locals_depth = n->op > 0 ? n->op : 256;
    cur_frame_mag    = 16 + nparams * 8 + 15 * 8 + 16 + locals_depth + 512;
    cur_frame_mag    = (cur_frame_mag + 15) & ~15;
    cur_param_num    = nparams;
    cur_func_typus   = n->typus;
    (void)locals_mag;

    /* label pro functione (iam allocatum in genera_translatio) */
    int func_label = func_loc_quaere(n->nomen);
    if (func_label < 0)
        func_label = label_novus();
    label_pone(func_label);

    /* prologus — ADD/SUB imm tractant reg 31 ut SP, non XZR */
    emit_stp_pre(FP, LR, SP, -16);
    emit_addi(FP, SP, 0);            /* MOV x29, sp */
    /* SUB sp, x29, #frame: si frame > 4095, per registrum x16 */
    {
        int frame = cur_frame_mag - 16;
        if (frame <= 4095) {
            emit_subi(SP, FP, frame);
        } else {
            emit_movi(16, frame);
            emit_sub(16, FP, 16);    /* x16 = x29 - frame */
            emit_addi(SP, 16, 0);    /* sp = x16 */
        }
    }

    /* salva parametros in acervo.
     * si functio variadica, salva omnes x0-x7 ut va_start operetur.
     * §5.4.2 AAPCS64: fluitantes in d0-d7, integri in x0-x7 (separatim). */
    {
        int salva_n = nparams < 8 ? nparams : 8;
        if (cur_func_typus && cur_func_typus->est_variadicus && salva_n < 8)
            salva_n = 8;
        int gp_reg = 0;  /* x-registrum proximum */
        int fp_reg = 0;  /* d-registrum proximum */
        for (int i = 0; i < salva_n; i++) {
            int dest_off = -(16 + (i + 1) * 8);
            typus_t *pt  = NULL;
            if (cur_func_typus && i < cur_func_typus->num_parametrorum)
                pt = cur_func_typus->parametri[i];
            if (pt && typus_est_fluat(pt)) {
                /* parametrus fluitans: salva ex d-registro */
                emit_movi(17, -dest_off);
                emit_sub(17, FP, 17);
                emit_fstr64(fp_reg, 17, 0);
                fp_reg++;
            } else {
                emit_str64(gp_reg, FP, dest_off);
                gp_reg++;
            }
        }
    }

    /* genera corpus */
    reg_vertex      = 0;
    break_vertex    = 0;
    num_goto_labels = 0;
    genera_sententia(n->dexter);

    /* epilogus implicitum (si non iam reditum) */
    emit_movi(0, 0);
    emit_addi(SP, FP, 0);            /* MOV sp, x29 */
    emit_ldp_post(FP, LR, SP, 16);
    emit_ret();

    /* recorda informationem functionis */
    if (!n->est_staticus) {
        /* exportanda */
    }
}

void genera_initia(void)
{
    emitte_initia();
    break_vertex           = 0;
    in_switch              = 0;
    num_func_loc           = 0;
    num_goto_labels        = 0;
    reg_vertex             = 0;
    profunditas_vocationis = 0;
}

void genera_translatio(nodus_t *radix, const char *plica_exitus, int modus_objecti)
{
    /* registra functionem _main in GOT */
    /* genera omnes functiones */
    int main_offset = -1;

    /* prima passu: collige globales (non extern — externae per GOT resolventur) */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_VAR_DECL && !n->est_externus) {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            long val      = 0;
            if (n->sinister && n->sinister->genus == N_NUM)
                val = n->sinister->valor;
            int gid = globalis_adde(n->nomen, n->typus_decl, n->est_staticus, val);
            if (s)
                s->globalis_index = gid;
        }
    }

    /* secunda passu: registra labels pro omnibus functionibus */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF)
            func_loc_adde(n->nomen, n->est_staticus);
    }

    /* tertia passu: genera functiones */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF) {
            if (strcmp(n->nomen, "main") == 0)
                main_offset = codex_lon;
            genera_functio(n);
        }
    }

    if (main_offset < 0 && !modus_objecti)
        erratum("functio main non inventa");

    /* quarta passu: genera __ccc_init si opus est —
     * initializat globales tabulae cum chordis et indicibus (§6.7.8) */
    {
        int habet_init = 0;
        for (int i = 0; i < radix->num_membrorum; i++) {
            nodus_t *n = radix->membra[i];
            if (n->genus != N_VAR_DECL || n->est_externus)
                continue;
            /* tabula cum initiatoribus */
            if (n->num_membrorum > 0 && n->membra)
                habet_init = 1;
            /* index scālāris cum chordā */
            if (
                n->sinister && n->sinister->genus == N_STR &&
                n->typus_decl && n->typus_decl->genus == TY_PTR
            )
                habet_init = 1;
        }
        if (habet_init) {
            int init_offset = codex_lon;
            if (modus_objecti) {
                /* in modō obiectī: functio __ccc_init simplex */
                func_loc_adde("__ccc_init", 0);
                label_pone(func_loc_quaere("__ccc_init"));
            }
            /* prologus */
            emit_stp_pre(FP, LR, SP, -16);
            emit_addi(FP, SP, 0);
            if (!modus_objecti) {
                emit_mov(19, 0); /* x19 = argc */
                emit_mov(20, 1); /* x20 = argv */
            }
            /* initializatiōnēs tabulārum */
            for (int i = 0; i < radix->num_membrorum; i++) {
                nodus_t *n = radix->membra[i];
                if (n->genus != N_VAR_DECL || n->est_externus)
                    continue;
                symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
                if (!s || s->globalis_index < 0)
                    continue;
                int gid = s->globalis_index;
                if (n->num_membrorum > 0 && n->membra) {
                    /* §6.7.8: tabula cum initiātōribus (complānātīs) */
                    typus_t *elem_t = (n->typus_decl && n->typus_decl->basis) ?
                        n->typus_decl->basis : ty_int;
                    int elem_mag = typus_magnitudo(elem_t);
                    if (elem_mag < 1)
                        elem_mag = 8;
                    /* si elementum est structūra, iter per membra */
                    int est_struct = (
                        elem_t->genus == TY_STRUCT &&
                        elem_t->membra && elem_t->num_membrorum > 0
                    );
                    int num_camp = est_struct ? elem_t->num_membrorum : 1;
                    for (int j = 0; j < n->num_membrorum; j++) {
                        nodus_t *elem = n->membra[j];
                        if (elem->genus == N_STR) {
                            int sid = chorda_adde(elem->chorda, elem->lon_chordae);
                            emit_adrp_fixup(0, FIX_ADRP, sid);
                            fixup_adde(FIX_ADD_LO12, codex_lon, sid, 0);
                            emit32(0x91000000 | (0 << 5) | 0);
                        } else if (elem->genus == N_NUM) {
                            emit_movi(0, elem->valor);
                        } else {
                            continue;
                        }
                        emit_adrp_fixup(17, FIX_ADRP_DATA, gid);
                        fixup_adde(FIX_ADD_LO12_DATA, codex_lon, gid, 0);
                        emit32(0x91000000 | (17 << 5) | 17);
                        if (est_struct) {
                            /* calcula offset per structūram et campum */
                            int idx_struct = j / num_camp;
                            int idx_camp   = j % num_camp;
                            int off = idx_struct * elem_mag +
                                elem_t->membra[idx_camp].offset;
                            int mag = mag_typi(elem_t->membra[idx_camp].typus);
                            if (mag < 1)
                                mag = 4;
                            emit_store(0, 17, off, mag);
                        } else {
                            emit_store(0, 17, j * elem_mag, elem_mag);
                        }
                    }
                } else if (
                    n->sinister && n->sinister->genus == N_STR &&
                    n->typus_decl && n->typus_decl->genus == TY_PTR
                ) {
                    /* index scālāris cum chordā litterālī */
                    int sid = chorda_adde(n->sinister->chorda, n->sinister->lon_chordae);
                    emit_adrp_fixup(0, FIX_ADRP, sid);
                    fixup_adde(FIX_ADD_LO12, codex_lon, sid, 0);
                    emit32(0x91000000 | (0 << 5) | 0);
                    emit_adrp_fixup(17, FIX_ADRP_DATA, gid);
                    fixup_adde(FIX_ADD_LO12_DATA, codex_lon, gid, 0);
                    emit32(0x91000000 | (17 << 5) | 17);
                    emit_str64(0, 17, 0);
                }
            }
            if (!modus_objecti) {
                /* voca main — restitue argc/argv */
                emit_mov(0, 19); /* x0 = argc */
                emit_mov(1, 20); /* x1 = argv */
                int main_label = func_loc_quaere("main");
                emit_bl_label(main_label);
            }
            /* epilogus */
            emit_addi(SP, FP, 0);
            emit_ldp_post(FP, LR, SP, 16);
            emit_ret();
            if (!modus_objecti)
                main_offset = init_offset;
        }
    }

    /* scribo plicam */
    if (modus_objecti)
        scribo_obiectum(plica_exitus);
    else
        scribo_macho(plica_exitus, main_offset);
}
