/*
 * gsymsent.c — generatio sententiarum et functionum (extractum ex
 * generasym.c). Continet genera_sententia (dispatchum per genus nodi
 * sententiae) et genera_functio (prologus, epilogus, AAPCS64 param
 * save).
 */

#include "utilia.h"
#include "parser.h"
#include "generasym.h"
#include "emittesym.h"
#include "typus.h"
#include "generasym_intern.h"

#include <string.h>
#include <stdlib.h>

/* ================================================================
 * status sententiae — solum intra hunc fasciculum
 * ================================================================ */

nodus_t *staticae_locales[1024];
int num_staticarum_localium = 0;

static int break_labels[MAX_BREAK];
static int continue_labels[MAX_BREAK];
static int break_vertex = 0;

static casus_t switch_casus[MAX_CASUS];
static int switch_num_casuum;
static int switch_default_label;
static int in_switch = 0;

typedef struct {
    char nomen[256];
    int label;
} goto_label_t;

static goto_label_t goto_labels[256];
static int num_goto_labels = 0;

static int cur_param_num;

static int goto_label_quaere_vel_crea(const char *nomen)
{
    for (int i = 0; i < num_goto_labels; i++)
        if (strcmp(goto_labels[i].nomen, nomen) == 0)
            return goto_labels[i].label;
    if (num_goto_labels >= 256)
        erratum("nimis multa goto labels");
    int lab = gsym_label_novus();
    strncpy(goto_labels[num_goto_labels].nomen, nomen, 255);
    goto_labels[num_goto_labels].label = lab;
    num_goto_labels++;
    return lab;
}

void gsymsent_initia(void)
{
    break_vertex            = 0;
    in_switch               = 0;
    num_goto_labels         = 0;
    num_staticarum_localium = 0;
}

static void genera_sententia(nodus_t *n);

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
        /* restitue SP ad basim salvatam in [FP-16] — ut littera
         * composita et alia allocata temporaria liberentur, sed VLAs
         * in acervo iam stantes servantur. */
        esym_ldr64(16, FP, -16);
        esym_addi(SP, 16, 0);
        break;

    case N_VAR_DECL:
        {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            /* §6.7.5.2: VLA — allocatio dynamica in acervo */
            if (n->tertius && s && !s->est_globalis) {
                reg_vertex = 0;
                genera_magnitudo_typi(n->typus_decl, 0);
                esym_addi(0, 0, 15);
                esym_movi(17, ~(long)15);
                esym_and(0, 0, 17);
                esym_addi(16, SP, 0);
                esym_sub(16, 16, 0);
                esym_addi(SP, 16, 0);
                int off = s->offset;
                esym_movi(17, -off);
                esym_sub(17, FP, 17);
                esym_str64(16, 17, 0);
                /* actualiza basim SP salvatam ad novam positionem post-VLA */
                esym_str64(16, FP, -16);
                reg_vertex = 0;
                break;
            }
            if (s && (s->est_globalis || s->est_staticus) && !s->est_externus) {
                if (s->globalis_index < 0) {
                    long val = 0;
                    if (n->sinister && n->sinister->genus == N_NUM)
                        val = n->sinister->valor;
                    else if (
                        n->sinister && n->typus_decl
                        && !typus_est_fluat(n->typus_decl)
                        && n->typus_decl->genus != TY_PTR
                        && n->typus_decl->genus != TY_ARRAY
                        && n->typus_decl->genus != TY_STRUCT
                    )
                        val = evalua_constans(n->sinister);
                    /* §6.2.2p3: staticae locales (function-scope) nomen
                     * internum distinctum habent. globalis_index < 0 hic
                     * significat statica haec nondum in tabula globalium —
                     * si est_staticus, munge nomen ne homonymae in aliis
                     * functionibus collidant. */
                    const char *nomen_reg = n->nomen;
                    char nomen_muncus[288];
                    if (s->est_staticus) {
                        static int contor_staticae = 0;
                        snprintf(
                            nomen_muncus, sizeof nomen_muncus,
                            "%s.%d", n->nomen, contor_staticae++
                        );
                        nomen_reg = nomen_muncus;
                    }
                    int gid = gsym_globalis_adde(
                        nomen_reg, n->typus_decl, n->est_staticus, val
                    );
                    s       ->globalis_index = gid;
                }
                if ((n->num_membrorum > 0 && n->membra) || n->sinister) {
                    if (num_staticarum_localium >= 1024)
                        erratum("nimis multae staticae locales cum initializatoribus");
                    staticae_locales[num_staticarum_localium++] = n;
                }
            } else if (n->num_membrorum > 0 && n->membra) {
                /* §6.7.8: initiale tabulae vel structurae */
                if (s) {
                    int off_basis = s->offset;
                    int tot_mag = (s->typus && s->typus->genus == TY_ARRAY) ?
                        s->typus->magnitudo : mag_typi(s->typus);
                    esym_imple_zeris(off_basis, tot_mag);
                    if (
                        s->typus && s->typus->genus == TY_STRUCT &&
                        s->typus->membra && s->typus->num_membrorum > 0
                    ) {
                        for (int i = 0; i < n->num_membrorum; i++) {
                            nodus_t    *elem = n->membra[i];
                            reg_vertex       = 0;
                            genera_expr(elem, 0);
                            int moff, mmag;
                            int bitpos        = elem->init_bitpos;
                            int bitwd         = elem->init_bitwidth;
                            typus_t    *mem_t = NULL;
                            if (elem->init_offset >= 0) {
                                moff = off_basis + elem->init_offset;
                                mmag = elem->init_size > 0 ? elem->init_size : 8;
                                /* Quaere typum membri cum offset concordī */
                                for (int k = 0; k < s->typus->num_membrorum; k++) {
                                    if (s->typus->membra[k].offset == elem->init_offset) {
                                        mem_t = s->typus->membra[k].typus;
                                        break;
                                    }
                                }
                            } else {
                                if (i >= s->typus->num_membrorum)
                                    erratum_ad(n->linea, "elementum %d extrā strūctūram sine designātōre", i);
                                moff   = off_basis + s->typus->membra[i].offset;
                                mmag   = mag_typi(s->typus->membra[i].typus);
                                bitpos = s->typus->membra[i].campus_positus;
                                bitwd  = s->typus->membra[i].campus_bitorum;
                                mem_t  = s->typus->membra[i].typus;
                            }
                            esym_movi(17, -moff);
                            esym_sub(17, FP, 17);
                            if (bitwd > 0) {
                                int rtmp = reg_alloca();
                                int ra0  = 0;
                                int rat  = reg_arm(rtmp);
                                esym_ldr32(rat, 17, 0);
                                esym_bfi(rat, ra0, bitpos, bitwd);
                                esym_str32(rat, 17, 0);
                                reg_libera(rtmp);
                            } else if (mem_t && typus_est_fluat(mem_t)) {
                                esym_fstore_to_addr(0, 17, mem_t);
                            } else if (
                                mem_t && mem_t->genus == TY_STRUCT
                                && mem_t->magnitudo > 0
                            ) {
                                /* init struct-membrī ex struct-valōre:
                                 * x0 = adresse fontis, x17 = adresse dēst. */
                                int mag = mem_t->magnitudo;
                                int k;
                                for (k = 0; k + 8 <= mag; k += 8) {
                                    esym_ldr64(16, 0, k);
                                    esym_str64(16, 17, k);
                                }
                                while (k + 4 <= mag) {
                                    esym_ldr32(16, 0, k);
                                    esym_str32(16, 17, k);
                                    k += 4;
                                }
                                while (k < mag) {
                                    esym_ldrb(16, 0, k);
                                    esym_strb(16, 17, k);
                                    k++;
                                }
                            } else {
                                esym_store(0, 17, 0, mmag > 8 ? 8 : mmag);
                            }
                            reg_vertex = 0;
                        }
                    } else {
                        /* tabula (fortasse structurarum): utere init_offset
                         * quod parser_sent.c iam computavit pro quoque scalari. */
                        /* leaf typus destinationis: descendit per ARRAY/STRUCT
                         * ad typum scalarem. Adhibetur ad decidendum si
                         * conversio integer→float necessaria est. */
                        typus_t *dest_fol = s->typus;
                        while (
                            dest_fol && (
                                dest_fol->genus == TY_ARRAY
                                || dest_fol->genus == TY_STRUCT
                            )
                        ) {
                            if (dest_fol->genus == TY_ARRAY && dest_fol->basis)
                                dest_fol = dest_fol->basis;
                            else if (
                                dest_fol->genus == TY_STRUCT && dest_fol->membra
                                && dest_fol->num_membrorum > 0
                            )
                                dest_fol = dest_fol->membra[0].typus;
                            else
                                break;
                        }
                        for (int i = 0; i < n->num_membrorum; i++) {
                            nodus_t *elem = n->membra[i];
                            if (elem->init_offset < 0)
                                erratum_ad(
                                    elem->linea,
                                    "initializator tabulae sine offset computato"
                                );
                            if (elem->init_size < 1)
                                erratum_ad(
                                    elem->linea,
                                    "initializator tabulae sine magnitudine"
                                );
                            if (!elem->typus)
                                erratum_ad(
                                    elem->linea,
                                    "initializator sine typo"
                                );
                            reg_vertex = 0;
                            genera_expr(elem, 0);
                            int elem_off = off_basis + elem->init_offset;
                            esym_movi(17, -elem_off);
                            esym_sub(17, FP, 17);
                            /* §6.7.8: initiālizātor convertitur ad typum
                             * destinātiōnis. Si dest est float et fons est
                             * integer, converte ante scripturam. */
                            int dest_est_fluat = dest_fol
                                && typus_est_fluat(dest_fol);
                            if (
                                dest_est_fluat
                                && typus_est_integer(elem->typus)
                            ) {
                                esym_int_to_double(0, elem->typus);
                                if (elem->init_size == 4) {
                                    esym_fcvt_ds(0, 0);
                                    esym_fstr32(0, 17, 0);
                                } else {
                                    esym_fstr64(0, 17, 0);
                                }
                            } else if (typus_est_fluat(elem->typus)) {
                                /* magnitudinem destinationis (init_size)
                                 * adhibemus, non typum fontis: e.g. float
                                 * slot 4-octeti requiret str s0, etiam si
                                 * expressio fontis est double. */
                                if (elem->init_size == 4) {
                                    esym_fcvt_ds(0, 0);
                                    esym_fstr32(0, 17, 0);
                                } else {
                                    esym_fstr64(0, 17, 0);
                                }
                            } else if (
                                elem->typus
                                && elem->typus->genus == TY_STRUCT
                                && elem->init_size > 0
                            ) {
                                /* init tabulae cum valore structurae:
                                 * x0 = adresse fontis, x17 = destinationis. */
                                esym_memcpy_bytes(
                                    17, 0, 0, 0,
                                    elem->init_size, 16
                                );
                            } else
                                esym_store(0, 17, 0, elem->init_size > 8 ? 8 : elem->init_size);
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
                int off         = s->offset;
                typus_t     *st = n->sinister->typus;
                int tot_mag     = st->magnitudo > 0 ? st->magnitudo : mag_typi(st);
                esym_imple_zeris(off, tot_mag);
                if (st->genus == TY_STRUCT && st->membra) {
                    for (int i = 0; i < n->sinister->num_membrorum && i < st->num_membrorum; i++) {
                        reg_vertex = 0;
                        genera_expr(n->sinister->membra[i], 0);
                        int moff = off + st->membra[i].offset;
                        int mmag = mag_typi(st->membra[i].typus);
                        esym_movi(17, -moff);
                        esym_sub(17, FP, 17);
                        if (typus_est_fluat(st->membra[i].typus))
                            esym_fstore_to_addr(0, 17, st->membra[i].typus);
                        else
                            esym_store(0, 17, 0, mmag > 8 ? 8 : mmag);
                        reg_vertex = 0;
                    }
                } else {
                    typus_t  *elem_t = st->basis ? st->basis : ty_int;
                    int emag         = typus_magnitudo(elem_t);
                    if (emag < 1)
                        erratum_ad(n->linea, "magnitudo elementi tabulae invalida");
                    for (int i = 0; i < n->sinister->num_membrorum; i++) {
                        reg_vertex = 0;
                        genera_expr(n->sinister->membra[i], 0);
                        esym_movi(17, -(off + i * emag));
                        esym_sub(17, FP, 17);
                        if (typus_est_fluat(elem_t))
                            esym_fstore_to_addr(0, 17, elem_t);
                        else
                            esym_store(0, 17, 0, emag > 8 ? 8 : emag);
                        reg_vertex = 0;
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
                int off     = s->offset;
                int arr_mag = s->typus->magnitudo;
                esym_imple_zeris(off, arr_mag);
                const char *str = n->sinister->chorda;
                int slen        = n->sinister->lon_chordae;
                for (int i = 0; i <= slen && i < arr_mag; i++) {
                    esym_movi(0, (unsigned char)str[i]);
                    esym_movi(17, -(off + i));
                    esym_sub(17, FP, 17);
                    esym_strb(0, 17, 0);
                }
                reg_vertex = 0;
            } else if (n->sinister) {
                reg_vertex = 0;
                genera_expr(n->sinister, 0);
                if (s) {
                    int mag = mag_typi(s->typus);
                    int off = s->offset;
                    if (s->typus && s->typus->genus == TY_STRUCT) {
                        esym_movi(17, -off);
                        esym_sub(17, FP, 17);
                        esym_memcpy_bytes(17, 0, 0, 0, mag, 16);
                    } else if (typus_est_fluat(s->typus)) {
                        if (!typus_est_fluat(n->sinister->typus))
                            esym_int_to_double(0, n->sinister->typus);
                        esym_movi(17, -off);
                        esym_sub(17, FP, 17);
                        esym_fstore_to_addr(0, 17, s->typus);
                    } else {
                        esym_movi(17, -off);
                        esym_sub(17, FP, 17);
                        esym_store(0, 17, 0, mag > 8 ? 8 : mag);
                    }
                }
                reg_vertex = 0;
            }
            break;
        }

    case N_IF:
        {
            int l_else = gsym_label_novus(), l_end = gsym_label_novus();
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            esym_cbz_label(0, n->tertius ? l_else : l_end);
            reg_vertex = 0;
            genera_sententia(n->dexter);
            if (n->tertius) {
                esym_b_label(l_end);
                esym_label_pone(l_else);
                genera_sententia(n->tertius);
            }
            esym_label_pone(l_end);
            break;
        }

    case N_WHILE:
        {
            int l_cond = gsym_label_novus(), l_end = gsym_label_novus();
            int l_cont = l_cond;
            if (break_vertex >= MAX_BREAK)
                erratum("nimis profunda nidificatio iterationum");
            break_labels[break_vertex]    = l_end;
            continue_labels[break_vertex] = l_cont;
            break_vertex++;
            esym_label_pone(l_cond);
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            esym_cbz_label(0, l_end);
            reg_vertex = 0;
            genera_sententia(n->dexter);
            esym_b_label(l_cond);
            esym_label_pone(l_end);
            break_vertex--;
            break;
        }

    case N_DOWHILE:
        {
            int l_top = gsym_label_novus(), l_cond = gsym_label_novus(), l_end = gsym_label_novus();
            if (break_vertex >= MAX_BREAK)
                erratum("nimis profunda nidificatio iterationum");
            break_labels[break_vertex]    = l_end;
            continue_labels[break_vertex] = l_cond;
            break_vertex++;
            esym_label_pone(l_top);
            genera_sententia(n->dexter);
            esym_label_pone(l_cond);
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            esym_cbnz_label(0, l_top);
            esym_label_pone(l_end);
            break_vertex--;
            break;
        }

    case N_FOR:
        {
            int l_cond = gsym_label_novus(), l_inc = gsym_label_novus(), l_end = gsym_label_novus();
            if (break_vertex >= MAX_BREAK)
                erratum("nimis profunda nidificatio iterationum");
            break_labels[break_vertex]    = l_end;
            continue_labels[break_vertex] = l_inc;
            break_vertex++;
            if (n->sinister) {
                reg_vertex = 0;
                genera_sententia(n->sinister);
            }
            esym_label_pone(l_cond);
            if (n->dexter) {
                reg_vertex = 0;
                genera_expr(n->dexter, 0);
                esym_cbz_label(0, l_end);
            }
            reg_vertex = 0;
            genera_sententia(n->quartus);
            esym_label_pone(l_inc);
            if (n->tertius) {
                reg_vertex = 0;
                genera_expr(n->tertius, 0);
            }
            esym_b_label(l_cond);
            esym_label_pone(l_end);
            break_vertex--;
            reg_vertex = 0;
            break;
        }

    case N_SWITCH:
        {
            int l_end         = gsym_label_novus();
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

            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            esym_str64(0, FP, -(cur_frame_mag - 16 - 16 * 15 * 8 - 8));

            int l_dispatch = gsym_label_novus();
            esym_b_label(l_dispatch);

            reg_vertex = 0;
            genera_sententia(n->dexter);
            esym_b_label(l_end);

            esym_label_pone(l_dispatch);
            esym_ldr64(0, FP, -(cur_frame_mag - 16 - 16 * 15 * 8 - 8));
            for (int i = 0; i < switch_num_casuum; i++) {
                esym_movi(17, switch_casus[i].valor);
                esym_cmp(0, 17, 8);
                esym_bcond_label(GSYM_COND_EQ, switch_casus[i].label);
            }
            if (switch_default_label >= 0)
                esym_b_label(switch_default_label);
            else
                esym_b_label(l_end);

            esym_label_pone(l_end);
            break_vertex--;

            in_switch = old_in_switch;
            memcpy(switch_casus, old_casus, sizeof(old_casus));
            switch_num_casuum    = old_num;
            switch_default_label = old_default;
            break;
        }

    case N_CASE:
        {
            int l = gsym_label_novus();
            if (switch_num_casuum >= MAX_CASUS)
                erratum_ad(n->linea, "nimis multi casus in switch");
            switch_casus[switch_num_casuum] .valor = n->valor;
            switch_casus[switch_num_casuum] .label = l;
            switch_num_casuum++;
            esym_label_pone(l);
            if (n->dexter)
                genera_sententia(n->dexter);
            break;
        }

    case N_DEFAULT:
        {
            int l = gsym_label_novus();
            switch_default_label = l;
            esym_label_pone(l);
            if (n->dexter)
                genera_sententia(n->dexter);
            break;
        }

    case N_RETURN:
        if (n->sinister) {
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            if (
                cur_func_typus && cur_func_typus->reditus
                && cur_func_typus->reditus->genus == TY_FLOAT
            )
                esym_fcvt_ds(0, 0);
            else if (
                cur_func_typus && cur_func_typus->reditus
                && cur_func_typus->reditus->genus == TY_STRUCT
            ) {
                int rhfa_n = 0, rhfa_typ = 0;
                if (typus_hfa(cur_func_typus->reditus, &rhfa_n, &rhfa_typ)) {
                    /* AAPCS64 §5.9.5: HFA redit in d/s 0..N-1. x0 habet
                     * adresse structī; extrahimus elementa. */
                    int esz = (rhfa_typ == TY_FLOAT) ? 4 : 8;
                    for (int k = 0; k < rhfa_n; k++) {
                        if (rhfa_typ == TY_FLOAT)
                            esym_fldr32(k, 0, k * esz);
                        else
                            esym_fldr64(k, 0, k * esz);
                    }
                } else if (
                    cur_func_typus->reditus->magnitudo <= 16
                    && cur_func_typus->reditus->magnitudo > 0
                ) {
                    /* AAPCS64: struct ≤16 non-HFA redit in x0 (et x1 si >8).
                     * x0 nunc habet adressum; legimus bytes. */
                    int mag = cur_func_typus->reditus->magnitudo;
                    if (mag > 8) {
                        esym_ldr64(1, 0, 8);
                        esym_ldr64(0, 0, 0);
                    } else {
                        esym_ldr64(0, 0, 0);
                    }
                } else {
                    /* x0 continet adresse fontis; x8 scrinium destinationis. */
                    int mag = cur_func_typus->reditus->magnitudo;
                    esym_ldr64(8, FP, -8);
                    for (int off = 0; off < mag; off += 8) {
                        int rem = mag - off;
                        if (rem >= 8) {
                            esym_ldr64(16, 0, off);
                            esym_str64(16, 8, off);
                        } else if (rem >= 4) {
                            esym_ldr32(16, 0, off);
                            esym_str32(16, 8, off);
                        } else {
                            esym_ldrb(16, 0, off);
                            esym_strb(16, 8, off);
                        }
                    }
                    /* AAPCS64: x0 == x8 post reditum. */
                    esym_mov(0, 8);
                }
            }
        }
        esym_addi(SP, FP, 0);
        esym_ldp_post(FP, LR, SP, 16);
        esym_ret();
        break;

    case N_BREAK:
        if (break_vertex <= 0)
            erratum_ad(n->linea, "break extra iterationem vel switch");
        esym_b_label(break_labels[break_vertex - 1]);
        break;

    case N_CONTINUE:
        if (break_vertex <= 0)
            erratum_ad(n->linea, "continue extra iterationem");
        esym_b_label(continue_labels[break_vertex - 1]);
        break;

    case N_GOTO:
        {
            int lab = goto_label_quaere_vel_crea(n->nomen);
            esym_b_label(lab);
        }
        break;

    case N_LABEL:
        {
            int lab = goto_label_quaere_vel_crea(n->nomen);
            esym_label_pone(lab);
            if (n->sinister)
                genera_sententia(n->sinister);
        }
        break;

    case N_FUNC_DEF:
        break;

    default:
        reg_vertex = 0;
        genera_expr(n, 0);
        reg_vertex = 0;
        break;
    }
}

/* ================================================================
 * generatio functionis
 * ================================================================ */

void genera_functio(nodus_t *n)
{
    int nparams      = (int)n->sinister->valor;
    int locals_depth = n->op > 0 ? n->op : 256;
    cur_frame_mag    = 16 + nparams * 8 + 16 * 15 * 8 + 16 + locals_depth + 512;
    cur_frame_mag    = (cur_frame_mag + 15) & ~15;
    cur_param_num    = nparams;
    cur_func_typus   = n->typus;

    /* functio header: .globl + .p2align + _nomen: */
    esym_func_header(n->nomen, n->est_staticus);

    /* prologus */
    esym_stp_pre(FP, LR, SP, -16);
    esym_addi(FP, SP, 0);
    {
        int frame = cur_frame_mag - 16;
        if (frame <= 4095) {
            esym_subi(SP, FP, frame);
        } else {
            esym_movi(16, frame);
            esym_sub(16, FP, 16);
            esym_addi(SP, 16, 0);
        }
    }
    /* serva SP-basim in [FP-16] ut reset post sententiam expressionis
     * tractet VLAs recte. Actualizatur post omnem N_VAR_DECL cum VLA. */
    esym_addi(16, SP, 0);
    esym_str64(16, FP, -16);

    /* AAPCS64: functiones reddentes structuras > 16 byte accipiunt
     * indicatorem scrinium reditus in x8. Servamus ad [FP-8]. */
    if (
        cur_func_typus && cur_func_typus->reditus
        && cur_func_typus->reditus->genus == TY_STRUCT
        && cur_func_typus->reditus->magnitudo > 16
    )
        esym_str64(8, FP, -8);

    {
        int salva_n = nparams < 8 ? nparams : 8;
        if (cur_func_typus && cur_func_typus->est_variadicus && salva_n < 8)
            salva_n = 8;
        int gp_reg   = 0;
        int fp_reg   = 0;
        int slot_cur = -16 - 8;
        for (int i = 0; i < salva_n; i++) {
            typus_t *pt  = NULL;
            if (cur_func_typus && i < cur_func_typus->num_parametrorum)
                pt = cur_func_typus->parametri[i];
            int hfa_n   = 0, hfa_typ = 0;
            int est_hfa = typus_hfa(pt, &hfa_n, &hfa_typ);
            if (est_hfa && fp_reg + hfa_n <= 8) {
                int num_regs = (pt->magnitudo + 7) / 8;
                int base_off = slot_cur - (num_regs - 1) * 8;
                int esz      = (hfa_typ == TY_FLOAT) ? 4 : 8;
                for (int k = 0; k < hfa_n; k++) {
                    if (hfa_typ == TY_FLOAT)
                        esym_fstr32(fp_reg + k, FP, base_off + k * esz);
                    else
                        esym_fstr64(fp_reg + k, FP, base_off + k * esz);
                }
                fp_reg   += hfa_n;
                slot_cur -= num_regs * 8;
            } else if (est_hfa) {
                /* NSRN exhaustum — arg in acervō vocantis (FP+), nihil
                 * faciendum in prologō. */
                fp_reg = 8;
            } else if (pt && typus_est_fluat(pt) && fp_reg >= 8) {
                /* NSRN exhaustum — arg in acervō vocantis */
            } else if (pt && typus_est_fluat(pt)) {
                esym_movi(17, -slot_cur);
                esym_sub(17, FP, 17);
                esym_fstr64(fp_reg, 17, 0);
                fp_reg++;
                slot_cur -= 8;
            } else if (
                pt && pt->genus == TY_STRUCT
                && pt->magnitudo <= 16 && pt->magnitudo > 0
            ) {
                int num_regs = (pt->magnitudo + 7) / 8;
                int base_off = slot_cur - (num_regs - 1) * 8;
                for (int k = 0; k < num_regs; k++) {
                    if (gp_reg + k >= 8)
                        break;
                    esym_str64(gp_reg + k, FP, base_off + k * 8);
                }
                gp_reg += num_regs;
                slot_cur -= num_regs * 8;
            } else {
                esym_str64(gp_reg, FP, slot_cur);
                gp_reg++;
                slot_cur -= 8;
            }
        }
    }

    reg_vertex      = 0;
    break_vertex    = 0;
    num_goto_labels = 0;
    genera_sententia(n->dexter);

    /* epilogus implicitus */
    if (
        cur_func_typus && cur_func_typus->reditus
        && cur_func_typus->reditus->genus == TY_STRUCT
        && cur_func_typus->reditus->magnitudo > 16
    )
        esym_ldr64(0, FP, -8);
    else
        esym_movi(0, 0);
    esym_addi(SP, FP, 0);
    esym_ldp_post(FP, LR, SP, 16);
    esym_ret();
}
