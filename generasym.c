/*
 * generasym.c — generator symbolicus (emittit assembly textum cum imm)
 *
 * Forma per genera.c ductus, sed emittit textum cc -S compatibile
 * loco binarii. Delegat ad imm assemblatorem pro:
 *   - resolutione labelorum et sectionum
 *   - relocationibus (ADRP/ADD/LDR @PAGE/@PAGEOFF/@GOTPAGE/@GOTPAGEOFF)
 *   - ordinatione .data/.bss/.cstring
 *
 * Referentiae ISO C99: citantur ubi logica semantica tractatur.
 */

#include "utilia.h"
#include "parser.h"
#include "generasym.h"
#include "emittesym.h"
#include "emitte.h"    /* pro typis: globalis_t, chorda_lit_t, data_reloc_t; constantibus SP/FP/LR/XZR */
#include "fluat.h"

#include <string.h>
#include <stdlib.h>

/* ================================================================
 * status localis — parallelus cum emitte.c, sed hic generasym.c
 * solus (nullus collīgātus cum emitte.o)
 * ================================================================ */

/* chordae litterales */
static chorda_lit_t *gsym_chordae     = NULL;
static int           gsym_num_chordarum = 0;

/* globales (data, bss, cstring labels) */
static globalis_t   *gsym_globales    = NULL;
static int           gsym_num_globalium = 0;

/* init_data — byti initializatorum ante emissionem textualem */
static uint8_t      *gsym_init_data   = NULL;
static int           gsym_init_data_lon = 0;

/* relocationes in init_data (adresses intra data) */
static data_reloc_t *gsym_data_relocs = NULL;
static int           gsym_num_data_relocs = 0;

/* labels — simplex alveus ad memorandum an definita sit */
static int *gsym_labels_def = NULL;     /* 0 = nondum, 1 = posita */
static int  gsym_num_labels = 0;

/* func_loc — functiones localis: label numero → nomen */
typedef struct {
    char nomen[256];
    int  label;
    int  est_staticus;
} gsym_func_loc_t;

static gsym_func_loc_t *gsym_func_loci  = NULL;
static int              gsym_num_func_loc = 0;

/* ================================================================
 * alveorum helpers
 * ================================================================ */

static int gsym_chorda_adde(const char *data, int lon)
{
    /* cc -S consolidat chordas identicas — imitamur per quaestionem */
    for (int i = 0; i < gsym_num_chordarum; i++) {
        if (
            gsym_chordae[i].longitudo == lon
            && memcmp(gsym_chordae[i].data, data, lon) == 0
        )
            return i;
    }
    if (gsym_num_chordarum >= MAX_CHORDAE_LIT)
        erratum("generasym: nimis multae chordae");
    int id = gsym_num_chordarum++;
    gsym_chordae[id].data = malloc(lon + 1);
    if (!gsym_chordae[id].data)
        erratum("generasym: memoria exhausta");
    memcpy(gsym_chordae[id].data, data, lon);
    gsym_chordae[id].data[lon] = 0;
    gsym_chordae[id].longitudo = lon;
    gsym_chordae[id].offset = 0;  /* non adhibetur in modo symbolico */
    return id;
}

static int gsym_globalis_adde(const char *nomen, typus_t *typus, int est_staticus, long valor)
{
    /* quaere ne duplicemus — crucial pro tentativas declarationes */
    for (int i = 0; i < gsym_num_globalium; i++)
        if (strcmp(gsym_globales[i].nomen, nomen) == 0)
            return i;
    if (gsym_num_globalium >= MAX_GLOBALES)
        erratum("generasym: nimis multae globales");
    int id = gsym_num_globalium++;
    strncpy(gsym_globales[id].nomen, nomen, 255);
    gsym_globales[id].nomen[255] = 0;
    gsym_globales[id].typus = typus;
    gsym_globales[id].magnitudo = typus ? typus_magnitudo(typus) : 0;
    int col = typus ? typus_colineatio(typus) : 1;
    if (col < 1)
        col = 1;
    gsym_globales[id].colineatio = col;
    gsym_globales[id].est_bss = 1;  /* defalta: in BSS nisi initiatur */
    gsym_globales[id].bss_offset = 0;
    gsym_globales[id].data_offset = 0;
    gsym_globales[id].est_staticus = est_staticus;
    gsym_globales[id].valor_initialis = valor;
    gsym_globales[id].habet_valorem = 0;
    return id;
}

static void gsym_data_reloc_adde(int idata_offset, int genus, int target)
{
    if (gsym_num_data_relocs >= MAX_DATA_RELOCS)
        erratum("generasym: nimis multae relocationes datorum");
    int id = gsym_num_data_relocs++;
    gsym_data_relocs[id].idata_offset = idata_offset;
    gsym_data_relocs[id].genus = genus;
    gsym_data_relocs[id].target = target;
}

/* est referentia ad externum (functio vel variabilis non definita)
 * — nomen cum _ praefix. */
#define MAX_EXT 1024
static char gsym_externa[MAX_EXT][256];
static int  gsym_num_externa = 0;

static int gsym_extern_adde(const char *nomen)
{
    /* nomen cum praefixo _; conservamus sic */
    for (int i = 0; i < gsym_num_externa; i++)
        if (strcmp(gsym_externa[i], nomen) == 0)
            return i;
    if (gsym_num_externa >= MAX_EXT)
        erratum("generasym: nimis multa externa");
    int id = gsym_num_externa++;
    strncpy(gsym_externa[id], nomen, 255);
    gsym_externa[id][255] = 0;
    return id;
}

static int gsym_label_novus(void)
{
    if (gsym_num_labels >= MAX_LABELS)
        erratum("generasym: nimis multa labels");
    int id = gsym_num_labels++;
    gsym_labels_def[id] = 0;
    return id;
}

static int gsym_func_loc_quaere(const char *nomen)
{
    for (int i = 0; i < gsym_num_func_loc; i++)
        if (strcmp(gsym_func_loci[i].nomen, nomen) == 0)
            return gsym_func_loci[i].label;
    return -1;
}

static int gsym_func_loc_adde(const char *nomen, int est_staticus)
{
    int lab = gsym_label_novus();
    if (gsym_num_func_loc >= MAX_GLOBALES)
        erratum("generasym: nimis multae functiones locales");
    int i = gsym_num_func_loc++;
    strncpy(gsym_func_loci[i].nomen, nomen, 255);
    gsym_func_loci[i].nomen[255] = 0;
    gsym_func_loci[i].label = lab;
    gsym_func_loci[i].est_staticus = est_staticus;
    return lab;
}

/* ================================================================
 * status functionis currentis — idem ut in genera.c
 * ================================================================ */

static nodus_t *staticae_locales[1024];
static int num_staticarum_localium = 0;

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

static int cur_frame_mag;
static int cur_param_num;
static typus_t *cur_func_typus;
static int profunditas_vocationis = 0;

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
        return slot;
    return slot + 1;
}

/* ================================================================
 * generatio expressionum
 * ================================================================ */

static void genera_expr(nodus_t *n, int dest);
static void genera_sententia(nodus_t *n);

/* §6.7.2.1: lectio campi bitorum — SBFX/UBFX */
static void genera_lectio_campi_bitorum(int r, membrum_t *mb)
{
    esym_ldr32(r, r, 0);
    if (mb->campus_signatus)
        esym_sbfx(r, r, mb->campus_positus, mb->campus_bitorum);
    else
        esym_ubfx(r, r, mb->campus_positus, mb->campus_bitorum);
}

static void genera_magnitudo_typi(typus_t *t, int dest_reg)
{
    int constant_mag = 1;
    nodus_t *dims[16];
    int ndims = 0;
    typus_t *cur = t;
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
                /* externum — per GOT */
                gsym_extern_adde(s->nomen);
                esym_adrp_ldr_got(r, s->nomen);
            } else if (s->est_globalis) {
                /* definitum localis — per @PAGE/@PAGEOFF */
                if (s->globalis_index < 0)
                    s->globalis_index = gsym_globalis_adde(s->nomen, s->typus, s->est_staticus, 0);
                esym_adrp_add_sym(r, s->nomen);
            } else {
                /* localis — offset a FP */
                int off = s->offset;
                if (off < 0) {
                    esym_movi(r, -off);
                    esym_sub(r, FP, r);
                } else {
                    esym_addi(r, FP, off);
                }
                /* §6.9.1¶10: parametrus strūctūrae > 16 octētōrum */
                if (
                    s->est_parametrus && s->typus
                    && s->typus->genus == TY_STRUCT
                    && s->typus->magnitudo > 16
                )
                    esym_ldr64(r, r, 0);
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
static void genera_expr(nodus_t *n, int dest)
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
                case T_LT:     esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_LT);
                    break;
                case T_GT:     esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_GT);
                    break;
                case T_LTEQ:   esym_fcmp(ra, rb);
                    esym_cset(ra, GSYM_COND_LE);
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
                {
                    int r2 = reg_alloca();
                    genera_lval(n->sinister, r2);
                    int mag = mag_typi(n->typus);
                    int inc = 1;
                    if (typus_est_index(n->typus))
                        inc = mag_typi(typus_basis_indicis(n->typus));
                    if (est_unsigned(n->typus))
                        esym_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    else
                        esym_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    esym_addi(r, r, inc);
                    esym_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
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
                        esym_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    else
                        esym_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    esym_subi(r, r, dec);
                    esym_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
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
            int salvati = reg_vertex;
            for (int i = 0; i < salvati; i++)
                esym_str64(reg_arm(i), FP, -(cur_frame_mag - 16 - i * 8));

            int nargs = n->num_membrorum;
            int arg_spill_base = cur_frame_mag - 16 - 15 * 8
                + profunditas_vocationis * 8 * 8;
            profunditas_vocationis++;

            int fptr_spill    = arg_spill_base + (nargs + 1) * 8;
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
                if (pt && pt->genus == TY_STRUCT && pt->magnitudo > 16)
                    struct_copy_tot += (pt->magnitudo + 15) & ~15;
            }
            int struct_copy_alloc = (struct_copy_tot + 15) & ~15;
            if (struct_copy_alloc > 0)
                esym_subi(SP, SP, struct_copy_alloc);
            int struct_copy_cur = 0;
            for (int i = 0; i < nargs; i++) {
                reg_vertex = 0;
                genera_expr(n->membra[i], 0);
                if (typus_est_fluat(n->membra[i]->typus)) {
                    int off = arg_spill_base + i * 8;
                    esym_movi(17, off);
                    esym_sub(17, FP, 17);
                    esym_fstr64(0, 17, 0);
                } else if (
                    n->membra[i]->typus
                    && n->membra[i]->typus->genus == TY_STRUCT
                    && n->membra[i]->typus->magnitudo > 16
                ) {
                    int mag    = n->membra[i]->typus->magnitudo;
                    int mag_al = (mag + 15) & ~15;
                    esym_addi(17, SP, struct_copy_cur);
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

            int num_fp_regs = 0;
            int num_gp_regs = 0;

            for (int i = 0; i < num_nominati && i < nargs; i++) {
                typus_t *pt = NULL;
                if (func_typ && i < func_typ->num_parametrorum)
                    pt = func_typ->parametri[i];
                if (!pt)
                    pt = n->membra[i]->typus;
                if (typus_est_fluat(pt)) {
                    if (num_fp_regs < 8) {
                        int off = arg_spill_base + i * 8;
                        esym_movi(17, off);
                        esym_sub(17, FP, 17);
                        esym_fldr64(num_fp_regs, 17, 0);
                        if (pt->genus == TY_FLOAT)
                            esym_fcvt_ds(num_fp_regs, num_fp_regs);
                    }
                    num_fp_regs++;
                } else if (
                    pt && pt->genus == TY_STRUCT
                    && pt->magnitudo <= 16 && pt->magnitudo > 0
                ) {
                    int num_regs = (pt->magnitudo + 7) / 8;
                    if (num_gp_regs + num_regs <= 8) {
                        esym_ldr64(17, FP, -(arg_spill_base + i * 8));
                        for (int k = 0; k < num_regs; k++)
                            esym_ldr64(num_gp_regs + k, 17, k * 8);
                    }
                    num_gp_regs += num_regs;
                } else {
                    if (num_gp_regs < 8)
                        esym_ldr64(num_gp_regs, FP, -(arg_spill_base + i * 8));
                    num_gp_regs++;
                }
            }

            int acervus_args = 0;
            if (est_variadica && nargs > num_nominati) {
                acervus_args    = nargs - num_nominati;
                int acervus_mag = ((acervus_args * 8) + 15) & ~15;
                esym_subi(SP, SP, acervus_mag);
                for (int i = 0; i < acervus_args; i++) {
                    esym_ldr64(17, FP, -(arg_spill_base + (num_nominati + i) * 8));
                    esym_str64(17, SP, i * 8);
                }
            } else if (!est_variadica) {
                for (int i = num_nominati; i < nargs; i++) {
                    typus_t *pt = n->membra[i]->typus;
                    if (typus_est_fluat(pt)) {
                        if (num_fp_regs < 8) {
                            int off = arg_spill_base + i * 8;
                            esym_movi(17, off);
                            esym_sub(17, FP, 17);
                            esym_fldr64(num_fp_regs, 17, 0);
                            num_fp_regs++;
                        }
                    } else {
                        if (num_gp_regs < 8) {
                            esym_ldr64(num_gp_regs, FP, -(arg_spill_base + i * 8));
                            num_gp_regs++;
                        }
                    }
                }
                if (nargs > 8) {
                    int extra     = nargs - 8;
                    int extra_mag = ((extra * 8) + 15) & ~15;
                    esym_subi(SP, SP, extra_mag);
                    for (int i = 8; i < nargs; i++) {
                        esym_ldr64(17, FP, -(arg_spill_base + i * 8));
                        esym_str64(17, SP, (i - 8) * 8);
                    }
                    acervus_args = extra;
                }
            }

            /* voca functionem */
            reg_vertex = 0;
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

            if (acervus_args > 0) {
                int acervus_mag = ((acervus_args * 8) + 15) & ~15;
                esym_addi(SP, SP, acervus_mag);
            }
            if (struct_copy_alloc > 0)
                esym_addi(SP, SP, struct_copy_alloc);

            {
                typus_t *ret_typ = NULL;
                if (func_typ)
                    ret_typ = func_typ->reditus;
                if (typus_est_fluat(ret_typ)) {
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
                esym_ldr64(reg_arm(i), FP, -(cur_frame_mag - 16 - i * 8));
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
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
                esym_load_from_addr(r, n->typus);
            break;
        }

    case N_MEMBER:
        {
            typus_t *st_mem = n->sinister ? n->sinister->typus : NULL;
            membrum_t *mb   = quaere_membrum(st_mem, n->nomen);
            if (mb && mb->campus_bitorum > 0) {
                genera_lval(n, dest);
                genera_lectio_campi_bitorum(r, mb);
            } else {
                genera_lval(n, dest);
                if (
                    n->typus && n->typus->genus != TY_ARRAY
                    && n->typus->genus != TY_STRUCT
                )
                    esym_load_from_addr(r, n->typus);
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
                )
                    esym_load_from_addr(r, n->typus);
            }
            break;
        }

    case N_DEREF:
        genera_expr(n->sinister, dest);
        if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
            esym_load_from_addr(r, n->typus);
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
            int num_camp = n->typus->num_membrorum;
            for (int i = 0; i < n->num_membrorum && i < num_camp; i++) {
                reg_vertex = rv_salvus;
                int r2     = reg_alloca();
                genera_expr(n->membra[i], r2);
                membrum_t *mb = &n->typus->membra[i];
                if (mb->campus_bitorum > 0) {
                    int r3  = reg_alloca();
                    int ra2 = reg_arm(r2);
                    int ra3 = reg_arm(r3);
                    esym_ldr32(ra3, r, mb->offset);
                    esym_bfi(ra3, ra2, mb->campus_positus, mb->campus_bitorum);
                    esym_str32(ra3, r, mb->offset);
                    reg_libera(r3);
                } else {
                    int mmag = mag_typi(mb->typus);
                    esym_store(reg_arm(r2), r, mb->offset, mmag > 8 ? 8 : mmag);
                }
                reg_libera(r2);
            }
            break;
        }

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
                reg_vertex = 0;
                break;
            }
            if (s && (s->est_globalis || s->est_staticus) && !s->est_externus) {
                if (s->globalis_index < 0) {
                    long val = 0;
                    if (n->sinister && n->sinister->genus == N_NUM)
                        val = n->sinister->valor;
                    int gid = gsym_globalis_adde(n->nomen, n->typus_decl, n->est_staticus, val);
                    s->globalis_index = gid;
                }
                if (n->num_membrorum > 0 && n->membra) {
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
                            nodus_t *elem = n->membra[i];
                            reg_vertex    = 0;
                            genera_expr(elem, 0);
                            int moff, mmag;
                            int bitpos = elem->init_bitpos;
                            int bitwd  = elem->init_bitwidth;
                            if (elem->init_offset >= 0) {
                                moff = off_basis + elem->init_offset;
                                mmag = elem->init_size > 0 ? elem->init_size : 8;
                            } else {
                                if (i >= s->typus->num_membrorum)
                                    erratum_ad(n->linea, "elementum %d extrā strūctūram sine designātōre", i);
                                moff = off_basis + s->typus->membra[i].offset;
                                mmag = mag_typi(s->typus->membra[i].typus);
                                bitpos = s->typus->membra[i].campus_positus;
                                bitwd  = s->typus->membra[i].campus_bitorum;
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
                            } else {
                                esym_store(0, 17, 0, mmag > 8 ? 8 : mmag);
                            }
                            reg_vertex = 0;
                        }
                    } else {
                        typus_t *elem_typus = (s->typus && s->typus->genus == TY_ARRAY) ?
                            s->typus->basis : ty_int;
                        while (
                            elem_typus && (
                                elem_typus->genus == TY_ARRAY ||
                                elem_typus->genus == TY_STRUCT
                            )
                        ) {
                            if (elem_typus->genus == TY_ARRAY && elem_typus->basis)
                                elem_typus = elem_typus->basis;
                            else if (
                                elem_typus->genus == TY_STRUCT &&
                                elem_typus->membra &&
                                elem_typus->num_membrorum > 0
                            )
                                elem_typus = elem_typus->membra[0].typus;
                            else
                                break;
                        }
                        int elem_mag = typus_magnitudo(elem_typus);
                        if (elem_mag < 1)
                            erratum_ad(n->linea, "magnitudo elementi structūrae invalida");
                        for (int i = 0; i < n->num_membrorum; i++) {
                            reg_vertex = 0;
                            genera_expr(n->membra[i], 0);
                            int elem_off = off_basis + i * elem_mag;
                            esym_movi(17, -elem_off);
                            esym_sub(17, FP, 17);
                            if (typus_est_fluat(elem_typus))
                                esym_fstore_to_addr(0, 17, elem_typus);
                            else
                                esym_store(0, 17, 0, elem_mag > 8 ? 8 : elem_mag);
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
                int off     = s->offset;
                typus_t *st = n->sinister->typus;
                int tot_mag = st->magnitudo > 0 ? st->magnitudo : mag_typi(st);
                esym_imple_zeris(off, tot_mag);
                if (st->genus == TY_STRUCT && st->membra) {
                    for (int i = 0; i < n->sinister->num_membrorum && i < st->num_membrorum; i++) {
                        reg_vertex = 0;
                        genera_expr(n->sinister->membra[i], 0);
                        int moff = off + st->membra[i].offset;
                        int mmag = mag_typi(st->membra[i].typus);
                        esym_movi(17, -moff);
                        esym_sub(17, FP, 17);
                        esym_store(0, 17, 0, mmag > 8 ? 8 : mmag);
                        reg_vertex = 0;
                    }
                } else {
                    typus_t *elem_t = st->basis ? st->basis : ty_int;
                    int emag        = typus_magnitudo(elem_t);
                    if (emag < 1)
                        erratum_ad(n->linea, "magnitudo elementi tabulae invalida");
                    for (int i = 0; i < n->sinister->num_membrorum; i++) {
                        reg_vertex = 0;
                        genera_expr(n->sinister->membra[i], 0);
                        esym_movi(17, -(off + i * emag));
                        esym_sub(17, FP, 17);
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
                        for (int i = 0; i < mag; i += 8) {
                            int rem = mag - i;
                            if (rem >= 8) {
                                esym_ldr64(16, 0, i);
                                esym_str64(16, 17, i);
                            }else if (rem >= 4) {
                                esym_ldr32(16, 0, i);
                                esym_str32(16, 17, i);
                            }else {
                                esym_ldrb(16, 0, i);
                                esym_strb(16, 17, i);
                            }
                        }
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
            esym_str64(0, FP, -(cur_frame_mag - 16 - 15 * 8 - 8));

            int l_dispatch = gsym_label_novus();
            esym_b_label(l_dispatch);

            reg_vertex = 0;
            genera_sententia(n->dexter);
            esym_b_label(l_end);

            esym_label_pone(l_dispatch);
            esym_ldr64(0, FP, -(cur_frame_mag - 16 - 15 * 8 - 8));
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
            switch_casus[switch_num_casuum].valor = n->valor;
            switch_casus[switch_num_casuum].label = l;
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

static void genera_functio(nodus_t *n)
{
    int nparams = (int)n->sinister->valor;
    int locals_depth = n->op > 0 ? n->op : 256;
    cur_frame_mag    = 16 + nparams * 8 + 15 * 8 + 16 + locals_depth + 512;
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

    {
        int salva_n = nparams < 8 ? nparams : 8;
        if (cur_func_typus && cur_func_typus->est_variadicus && salva_n < 8)
            salva_n = 8;
        int gp_reg = 0;
        int fp_reg = 0;
        int slot_cur = -16 - 8;
        for (int i = 0; i < salva_n; i++) {
            typus_t *pt  = NULL;
            if (cur_func_typus && i < cur_func_typus->num_parametrorum)
                pt = cur_func_typus->parametri[i];
            if (pt && typus_est_fluat(pt)) {
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
    esym_movi(0, 0);
    esym_addi(SP, FP, 0);
    esym_ldp_post(FP, LR, SP, 16);
    esym_ret();
}

/* ================================================================
 * emissio sectionum datorum (post omnes functiones)
 * ================================================================ */

/* nomen chorda_ID vel _globalis_name pro relocatione .quad */
static void emitte_quad_reloc(data_reloc_t *dr)
{
    switch (dr->genus) {
    case DR_CSTRING:
        {
            /* dr->target = index chordae (sid) — conventiō nostra in
             * modō symbolicō; cfr. gsym_data_reloc_adde(..., DR_CSTRING, sid) */
            esym_quad_str(dr->target);
            return;
        }
    case DR_IDATA:
        {
            /* target = data_offset globalis. Quaere quem globalem. */
            for (int i = 0; i < gsym_num_globalium; i++) {
                if (!gsym_globales[i].est_bss && gsym_globales[i].data_offset == dr->target) {
                    esym_quad_sym(gsym_globales[i].nomen, 0);
                    return;
                }
            }
            erratum("DR_IDATA: target %d non invenitur in globalibus", dr->target);
        }
    case DR_TEXT:
        {
            /* target = label functionis. Quaere func_loc. */
            for (int i = 0; i < gsym_num_func_loc; i++)
                if (gsym_func_loci[i].label == dr->target) {
                esym_quad_sym(gsym_func_loci[i].nomen, 0);
                return;
            }
            erratum("DR_TEXT: label %d non invenitur in func_loci", dr->target);
        }
    case DR_EXT_FUNC:
        {
            /* target = index in gsym_externa */
            if (dr->target < 0 || dr->target >= gsym_num_externa)
                erratum("DR_EXT_FUNC: target invalidus: %d", dr->target);
            /* gsym_externa stores nomen sine _ praefix */
            esym_quad_sym(gsym_externa[dr->target], 0);
            return;
        }
    default:
        erratum("relocatio datorum genus ignotum: %d", dr->genus);
    }
}

/* scribe byti range in init_data, intermiscens .quad relocationes */
static void scribe_datum_regionem(int data_off, int mag)
{
    /* collige relocationes in regione ordine offset */
    int end = data_off + mag;
    int cursor = data_off;
    while (cursor < end) {
        /* quaere proximam relocationem */
        int next_r = -1;
        int next_off = end;
        for (int i = 0; i < gsym_num_data_relocs; i++) {
            int o = gsym_data_relocs[i].idata_offset;
            if (o >= cursor && o < end && o < next_off) {
                next_off = o;
                next_r = i;
            }
        }
        /* scribe byti ante relocationem ut .byte */
        while (cursor < next_off) {
            esym_byte(gsym_init_data[cursor]);
            cursor++;
        }
        if (next_r < 0)
            break;
        /* emitte .quad ad symbolum */
        emitte_quad_reloc(&gsym_data_relocs[next_r]);
        cursor += 8;
    }
}

static int log2_floor(int n)
{
    int k = 0;
    while ((1 << k) < n)
        k++;
    return k;
}

static void emitte_sectiones_datorum(void)
{
    /* ordo: __cstring primum, deinde __data, deinde __bss.
     * imm requirit ut offsetos chordarum nota sint ante
     * processandum .quad relocationes in __data. */

    /* __cstring */
    if (gsym_num_chordarum > 0) {
        esym_sectio_cstring();
        for (int i = 0; i < gsym_num_chordarum; i++) {
            esym_str_label_pone(i);
            esym_asciz(gsym_chordae[i].data, gsym_chordae[i].longitudo);
        }
    }

    /* __data */
    int habet_data = 0;
    for (int i = 0; i < gsym_num_globalium; i++) {
        globalis_t *g = &gsym_globales[i];
        if (g->est_bss)
            continue;
        if (!habet_data) {
            esym_sectio_data();
            habet_data = 1;
        }
        int col = g->colineatio > 0 ? g->colineatio : 1;
        int log2a = log2_floor(col);
        if (!g->est_staticus)
            esym_globl(g->nomen);
        if (log2a > 0)
            esym_p2align(log2a);
        esym_glob_label_pone(g->nomen);
        scribe_datum_regionem(g->data_offset, g->magnitudo);
    }

    /* __bss — utamur .zerofill directivā */
    for (int i = 0; i < gsym_num_globalium; i++) {
        globalis_t *g = &gsym_globales[i];
        if (!g->est_bss)
            continue;
        int col = g->colineatio > 0 ? g->colineatio : 1;
        int log2a = log2_floor(col);
        if (!g->est_staticus)
            esym_globl(g->nomen);
        esym_zerofill("__DATA", "__bss", g->nomen, g->magnitudo, log2a);
    }
}

/* ================================================================
 * API publica
 * ================================================================ */

void generasym_initia(void)
{
    gsym_chordae     = calloc(MAX_CHORDAE_LIT, sizeof(chorda_lit_t));
    gsym_globales    = calloc(MAX_GLOBALES, sizeof(globalis_t));
    gsym_init_data   = calloc(MAX_DATA, 1);
    gsym_data_relocs = calloc(MAX_DATA_RELOCS, sizeof(data_reloc_t));
    gsym_labels_def  = calloc(MAX_LABELS, sizeof(int));
    gsym_func_loci   = calloc(MAX_GLOBALES, sizeof(gsym_func_loc_t));
    if (
        !gsym_chordae || !gsym_globales || !gsym_init_data
        || !gsym_data_relocs || !gsym_labels_def || !gsym_func_loci
    )
        erratum("generasym: memoria exhausta");
    gsym_num_chordarum = 0;
    gsym_num_globalium = 0;
    gsym_init_data_lon = 0;
    gsym_num_data_relocs = 0;
    gsym_num_labels = 0;
    gsym_num_func_loc = 0;
    gsym_num_externa = 0;
    break_vertex           = 0;
    in_switch              = 0;
    num_goto_labels        = 0;
    reg_vertex             = 0;
    profunditas_vocationis = 0;
    num_staticarum_localium = 0;
}

/* evalua expressionem constantem integer — §6.6.  Hic minima
 * implementatio pro usu in initializatoribus. Praefertur ut
 * parser hoc iam computaverit in valor; sed retinemus. */
static long evalua_constans_local(nodus_t *n)
{
    if (!n)
        erratum("evalua: nodus nullus");
    switch (n->genus) {
    case N_NUM: return n->valor;
    case N_CAST: return evalua_constans_local(n->sinister);
    case N_UNOP:
        switch (n->op) {
        case T_MINUS: return -evalua_constans_local(n->sinister);
        case T_TILDE: return ~evalua_constans_local(n->sinister);
        case T_BANG:  return !evalua_constans_local(n->sinister);
        case T_PLUS:  return evalua_constans_local(n->sinister);
        }
        break;
    case N_BINOP:
        {
            long a = evalua_constans_local(n->sinister);
            long b = evalua_constans_local(n->dexter);
            switch (n->op) {
            case T_PLUS:    return a + b;
            case T_MINUS:   return a - b;
            case T_STAR:    return a * b;
            case T_SLASH:   if (!b)
                    erratum("divisio per 0 in constante");
                return a / b;
            case T_PERCENT: if (!b)
                    erratum("divisio per 0 in constante");
                return a % b;
            case T_AMP:     return a & b;
            case T_PIPE:    return a | b;
            case T_CARET:   return a ^ b;
            case T_LTLT:    return a << b;
            case T_GTGT:    return a >> b;
            }
            break;
        }
    }
    erratum_ad(n->linea, "expressio non constans in initializatore");
    return 0;
}

void generasym_translatio(nodus_t *radix, FILE *out)
{
    esym_initia(out);

    /* prima passu: colligā globalēs */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_VAR_DECL && !n->est_externus) {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            long val      = 0;
            if (n->sinister && n->sinister->genus == N_NUM)
                val = n->sinister->valor;
            int gid = gsym_globalis_adde(n->nomen, n->typus_decl, n->est_staticus, val);
            if (s)
                s->globalis_index = gid;
        }
    }

    /* secunda passu: registrā functionēs */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF)
            gsym_func_loc_adde(n->nomen, n->est_staticus);
    }

    /* tertia passu: generā functionēs */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF)
            genera_functio(n);
    }

    /* quarta passu: initiā globālēs (§6.7.8) — aedificā init_data bytes
     * et relocationēs intrā gsym_init_data. */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus != N_VAR_DECL || n->est_externus)
            continue;
        symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
        if (!s || s->globalis_index < 0)
            continue;
        int gid = s->globalis_index;

        int habet_init_data = 0;
        if (n->num_membrorum > 0 && n->membra)
            habet_init_data = 1;
        if (
            n->sinister && n->sinister->genus == N_STR &&
            n->typus_decl && (
                n->typus_decl->genus == TY_PTR
                || (
                    n->typus_decl->genus == TY_ARRAY
                    && n->typus_decl->basis
                    && (
                        n->typus_decl->basis->genus == TY_CHAR
                        || n->typus_decl->basis->genus == TY_UCHAR
                    )
                )
            )
        )
            habet_init_data = 1;
        if (n->sinister && n->sinister->genus == N_IDENT && n->sinister->nomen)
            habet_init_data = 1;
        if (n->sinister && typus_est_integer(n->typus_decl))
            habet_init_data = 1;
        if (
            n->sinister
            && (
                n->sinister->genus == N_NUM_FLUAT
                || (
                    n->sinister->genus == N_UNOP
                    && n->sinister->op == T_MINUS
                    && n->sinister->sinister
                    && n->sinister->sinister->genus == N_NUM_FLUAT
                )
            )
        )
            habet_init_data = 1;
        /* staticae sine initiāle: zerō-initiā in init_data */
        if (!habet_init_data && n->est_staticus) {
            int mag0 = gsym_globales[gid].magnitudo;
            if (mag0 < 1)
                continue;
            int col0 = gsym_globales[gid].colineatio;
            if (col0 < 1)
                col0 = 1;
            gsym_init_data_lon = (gsym_init_data_lon + col0 - 1) & ~(col0 - 1);
            int doff0 = gsym_init_data_lon;
            if (gsym_init_data_lon + mag0 > MAX_DATA)
                erratum("gsym_init_data nimis magna");
            memset(gsym_init_data + doff0, 0, mag0);
            gsym_init_data_lon += mag0;
            gsym_globales[gid].est_bss     = 0;
            gsym_globales[gid].data_offset = doff0;
            continue;
        }
        if (!habet_init_data)
            continue;

        int mag = gsym_globales[gid].magnitudo;
        if (mag < 1)
            erratum("globalis '%s' magnitudo invalida: %d", n->nomen, mag);
        int col = gsym_globales[gid].colineatio;
        if (col < 1)
            erratum("globalis '%s' colineatio invalida: %d", n->nomen, col);
        gsym_init_data_lon = (gsym_init_data_lon + col - 1) & ~(col - 1);
        int data_off  = gsym_init_data_lon;
        if (gsym_init_data_lon + mag > MAX_DATA)
            erratum("gsym_init_data nimis magna");
        memset(gsym_init_data + data_off, 0, mag);
        gsym_init_data_lon += mag;

        gsym_globales[gid].est_bss     = 0;
        gsym_globales[gid].data_offset = data_off;

        if (n->num_membrorum > 0 && n->membra) {
            /* §6.7.8: tabula vel strūctūra */
            typus_t *elem_t;
            int singula_structura = 0;
            if (
                n->typus_decl && n->typus_decl->genus == TY_ARRAY
                && n->typus_decl->basis
            )
                elem_t = n->typus_decl->basis;
            else if (n->typus_decl && n->typus_decl->genus == TY_STRUCT) {
                elem_t = n->typus_decl;
                singula_structura = 1;
            } else
                elem_t = ty_int;
            int elem_mag = typus_magnitudo(elem_t);
            if (elem_mag < 1)
                erratum("elementum tabulae '%s' magnitudo invalida", n->nomen);

            typus_t *folium_t = elem_t;
            if (singula_structura && folium_t->genus == TY_STRUCT) {
                if (folium_t->membra && folium_t->num_membrorum > 0)
                    folium_t = folium_t->membra[0].typus;
            }
            while (folium_t->genus == TY_ARRAY && folium_t->basis)
                folium_t = folium_t->basis;
            int folium_mag = typus_magnitudo(folium_t);
            if (folium_mag < 1)
                erratum("magnitudo folii invalida");

            int est_struct = (
                elem_t->genus == TY_STRUCT &&
                elem_t->membra && elem_t->num_membrorum > 0
            );
            int num_camp = est_struct ? elem_t->num_membrorum : 1;
            for (int j = 0; j < n->num_membrorum; j++) {
                nodus_t *elem = n->membra[j];
                int elem_off;
                int store_mag;
                int est_char_array = 0;
                if (elem->init_offset >= 0 && elem->init_size > 0) {
                    elem_off  = data_off + elem->init_offset;
                    store_mag = elem->init_size;
                    if (elem->genus == N_STR && elem->init_size > 8)
                        est_char_array = 1;
                } else if (singula_structura) {
                    elem_off  = data_off + j * folium_mag;
                    store_mag = folium_mag;
                } else if (est_struct) {
                    int idx_struct  = j / num_camp;
                    int idx_camp    = j % num_camp;
                    typus_t *camp_t = elem_t->membra[idx_camp].typus;
                    typus_t *camp_fol = camp_t;
                    while (
                        camp_fol && camp_fol->genus == TY_ARRAY
                        && camp_fol->basis
                    )
                        camp_fol = camp_fol->basis;
                    int camp_fol_mag = typus_magnitudo(camp_fol);
                    if (camp_fol_mag < 1)
                        erratum("magnitudo folii campi invalida");
                    if (camp_t->genus == TY_ARRAY) {
                        int scal_per_struct = 0;
                        for (int mi = 0; mi < num_camp; mi++)
                            scal_per_struct += numera_elementa_init(elem_t->membra[mi].typus);
                        if (scal_per_struct < 1)
                            erratum("numerus scalarium per structuram invalidus");
                        int si = j / scal_per_struct;
                        int sj = j % scal_per_struct;
                        int run     = 0;
                        int mem_off = 0;
                        int sub_idx = sj;
                        for (int mi = 0; mi < num_camp; mi++) {
                            typus_t *mt = elem_t->membra[mi].typus;
                            int cnt     = numera_elementa_init(mt);
                            int is_ch_arr = (
                                mt->genus == TY_ARRAY && mt->basis
                                && (
                                    mt->basis->genus == TY_CHAR
                                    || mt->basis->genus == TY_UCHAR
                                )
                            );
                            if (sj < run + cnt) {
                                sub_idx = sj - run;
                                mem_off = elem_t->membra[mi].offset;
                                if (is_ch_arr) {
                                    camp_fol_mag   = typus_magnitudo(mt);
                                    est_char_array = 1;
                                } else if (mt->genus == TY_STRUCT) {
                                    typus_t *sf = mt;
                                    while (
                                        sf->membra && sf->num_membrorum > 0
                                        && sf->genus == TY_STRUCT
                                    )
                                        sf = sf->membra[0].typus;
                                    while (sf->genus == TY_ARRAY && sf->basis)
                                        sf = sf->basis;
                                    camp_fol_mag = typus_magnitudo(sf);
                                    if (camp_fol_mag < 1)
                                        erratum("magnitudo folii sub-strūctūrae invalida");
                                } else {
                                    camp_fol_mag = (mt->genus == TY_ARRAY)
                                        ? typus_magnitudo(camp_fol) : mag_typi(mt);
                                }
                                break;
                            }
                            run += cnt;
                        }
                        elem_off = data_off + si * elem_mag
                            + mem_off + sub_idx * camp_fol_mag;
                        store_mag = camp_fol_mag;
                    } else {
                        elem_off = data_off + idx_struct * elem_mag
                            + elem_t->membra[idx_camp].offset;
                        store_mag = mag_typi(camp_t);
                        if (store_mag < 1)
                            erratum("campus structūrae magnitudo invalida");
                    }
                } else if (elem_t->genus == TY_ARRAY) {
                    elem_off  = data_off + j * folium_mag;
                    store_mag = folium_mag;
                } else {
                    elem_off  = data_off + j * elem_mag;
                    store_mag = elem_mag;
                }
                if (elem->genus == N_STR) {
                    if (est_struct && est_char_array) {
                        int lon = elem->lon_chordae < store_mag
                            ? elem->lon_chordae : store_mag;
                        memcpy(gsym_init_data + elem_off, elem->chorda, lon);
                        if (lon < store_mag)
                            memset(gsym_init_data + elem_off + lon, 0, store_mag - lon);
                    } else {
                        int sid = gsym_chorda_adde(elem->chorda, elem->lon_chordae);
                        gsym_data_reloc_adde(elem_off, DR_CSTRING, sid);
                    }
                } else if (elem->genus == N_NUM) {
                    long v = elem->valor;
                    if (elem->init_bitwidth > 0) {
                        unsigned int mask = elem->init_bitwidth >= 32
                            ? 0xffffffffu : ((1u << elem->init_bitwidth) - 1u);
                        unsigned int u;
                        memcpy(&u, gsym_init_data + elem_off, 4);
                        u &= ~(mask << elem->init_bitpos);
                        u |= ((unsigned int)v & mask) << elem->init_bitpos;
                        memcpy(gsym_init_data + elem_off, &u, 4);
                    } else {
                        memcpy(gsym_init_data + elem_off, &v, store_mag);
                    }
                } else if (
                    elem->genus == N_UNOP && elem->op == T_MINUS
                    && elem->sinister && elem->sinister->genus == N_NUM
                ) {
                    long v = -elem->sinister->valor;
                    memcpy(gsym_init_data + elem_off, &v, store_mag);
                } else if (elem->genus == N_NUM_FLUAT) {
                    if (store_mag == 4) {
                        float fv = (float)elem->valor_f;
                        memcpy(gsym_init_data + elem_off, &fv, 4);
                    } else {
                        double dv = elem->valor_f;
                        memcpy(gsym_init_data + elem_off, &dv, store_mag);
                    }
                } else if (elem->genus == N_IDENT && elem->nomen) {
                    int flabel = gsym_func_loc_quaere(elem->nomen);
                    if (flabel >= 0) {
                        gsym_data_reloc_adde(elem_off, DR_TEXT, flabel);
                    } else if (
                        elem->sym && elem->sym->globalis_index >= 0
                        && !gsym_globales[elem->sym->globalis_index].est_bss
                    ) {
                        gsym_data_reloc_adde(
                            elem_off, DR_IDATA,
                            gsym_globales[elem->sym->globalis_index].data_offset
                        );
                    } else {
                        int ei = gsym_extern_adde(elem->nomen);
                        gsym_data_reloc_adde(elem_off, DR_EXT_FUNC, ei);
                    }
                } else if (elem->typus && typus_est_integer(elem->typus)) {
                    long v = evalua_constans_local(elem);
                    if (elem->init_bitwidth > 0) {
                        unsigned int mask = elem->init_bitwidth >= 32
                            ? 0xffffffffu : ((1u << elem->init_bitwidth) - 1u);
                        unsigned int u;
                        memcpy(&u, gsym_init_data + elem_off, 4);
                        u &= ~(mask << elem->init_bitpos);
                        u |= ((unsigned int)v & mask) << elem->init_bitpos;
                        memcpy(gsym_init_data + elem_off, &u, 4);
                    } else {
                        memcpy(gsym_init_data + elem_off, &v, store_mag);
                    }
                }
            }
        } else if (
            n->sinister && n->sinister->genus == N_STR
            && n->typus_decl && n->typus_decl->genus == TY_ARRAY
            && n->typus_decl->basis
            && (
                n->typus_decl->basis->genus == TY_CHAR
                || n->typus_decl->basis->genus == TY_UCHAR
            )
        ) {
            int lon    = n->sinister->lon_chordae;
            int cap    = gsym_globales[gid].magnitudo;
            int n_copy = lon < cap ? lon : cap;
            memcpy(gsym_init_data + data_off, n->sinister->chorda, n_copy);
            if (n_copy < cap)
                memset(gsym_init_data + data_off + n_copy, 0, cap - n_copy);
        } else if (n->sinister && n->sinister->genus == N_STR) {
            int sid = gsym_chorda_adde(n->sinister->chorda, n->sinister->lon_chordae);
            gsym_data_reloc_adde(data_off, DR_CSTRING, sid);
        } else if (
            n->sinister && n->sinister->genus == N_IDENT
            && n->sinister->nomen
        ) {
            int flabel = gsym_func_loc_quaere(n->sinister->nomen);
            if (flabel >= 0) {
                gsym_data_reloc_adde(data_off, DR_TEXT, flabel);
            } else if (
                n->sinister->sym && n->sinister->sym->globalis_index >= 0
                && !gsym_globales[n->sinister->sym->globalis_index].est_bss
            ) {
                gsym_data_reloc_adde(
                    data_off, DR_IDATA,
                    gsym_globales[n->sinister->sym->globalis_index].data_offset
                );
            } else {
                int ei = gsym_extern_adde(n->sinister->nomen);
                gsym_data_reloc_adde(data_off, DR_EXT_FUNC, ei);
            }
        } else if (n->sinister && n->sinister->genus == N_NUM_FLUAT) {
            int store_mag = gsym_globales[gid].magnitudo;
            if (store_mag == 4) {
                float fv = (float)n->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &fv, 4);
            } else {
                double dv = n->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &dv, store_mag > 8 ? 8 : store_mag);
            }
        } else if (
            n->sinister && n->sinister->genus == N_UNOP
            && n->sinister->op == T_MINUS && n->sinister->sinister
            && n->sinister->sinister->genus == N_NUM_FLUAT
        ) {
            int store_mag = gsym_globales[gid].magnitudo;
            if (store_mag == 4) {
                float fv = (float)-n->sinister->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &fv, 4);
            } else {
                double dv = -n->sinister->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &dv, store_mag > 8 ? 8 : store_mag);
            }
        } else if (n->sinister && typus_est_integer(n->typus_decl)) {
            long v = evalua_constans_local(n->sinister);
            int store_mag = gsym_globales[gid].magnitudo;
            if (store_mag > 8)
                store_mag = 8;
            memcpy(gsym_init_data + data_off, &v, store_mag);
        } else {
            erratum("initiālizātor globālis nōn tractātus prō '%s'", n->nomen);
        }
    }

    /* staticae locālēs cum initiālizātōribus */
    for (int si = 0; si < num_staticarum_localium; si++) {
        nodus_t *n    = staticae_locales[si];
        symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
        if (!s || s->globalis_index < 0)
            continue;
        int gid = s->globalis_index;

        if (!n->num_membrorum || !n->membra) {
            int mag0 = gsym_globales[gid].magnitudo;
            if (mag0 < 1)
                continue;
            int col0 = gsym_globales[gid].colineatio;
            if (col0 < 1)
                col0 = 1;
            gsym_init_data_lon = (gsym_init_data_lon + col0 - 1) & ~(col0 - 1);
            int doff0 = gsym_init_data_lon;
            if (gsym_init_data_lon + mag0 > MAX_DATA)
                erratum("gsym_init_data nimis magna (static local BSS)");
            memset(gsym_init_data + doff0, 0, mag0);
            gsym_init_data_lon += mag0;
            gsym_globales[gid].est_bss     = 0;
            gsym_globales[gid].data_offset = doff0;
            continue;
        }

        int mag = gsym_globales[gid].magnitudo;
        if (mag < 1)
            continue;
        int col = gsym_globales[gid].colineatio;
        if (col < 1)
            col = 1;
        gsym_init_data_lon = (gsym_init_data_lon + col - 1) & ~(col - 1);
        int data_off  = gsym_init_data_lon;
        if (gsym_init_data_lon + mag > MAX_DATA)
            erratum("gsym_init_data nimis magna (static local)");
        memset(gsym_init_data + data_off, 0, mag);
        gsym_init_data_lon += mag;

        gsym_globales[gid].est_bss     = 0;
        gsym_globales[gid].data_offset = data_off;

        typus_t *sl_t   = n->typus_decl;
        typus_t *sl_fol = sl_t;
        while (sl_fol && (sl_fol->genus == TY_ARRAY || sl_fol->genus == TY_STRUCT)) {
            if (sl_fol->genus == TY_ARRAY && sl_fol->basis)
                sl_fol = sl_fol->basis;
            else if (
                sl_fol->genus == TY_STRUCT && sl_fol->membra
                && sl_fol->num_membrorum > 0
            )
                sl_fol = sl_fol->membra[0].typus;
            else
                break;
        }
        int sl_mag = typus_magnitudo(sl_fol);
        if (sl_mag < 1)
            erratum("magnitudo folii staticae localis invalida");

        for (int j = 0; j < n->num_membrorum; j++) {
            nodus_t *elem = n->membra[j];
            int elem_off  = data_off + j * sl_mag;
            if (elem_off + sl_mag > data_off + mag)
                break;
            if (elem->genus == N_STR) {
                int sid = gsym_chorda_adde(elem->chorda, elem->lon_chordae);
                gsym_data_reloc_adde(elem_off, DR_CSTRING, sid);
            } else if (elem->genus == N_NUM) {
                long v = elem->valor;
                memcpy(gsym_init_data + elem_off, &v, sl_mag);
            } else if (
                elem->genus == N_UNOP && elem->op == T_MINUS
                && elem->sinister && elem->sinister->genus == N_NUM
            ) {
                long v = -elem->sinister->valor;
                memcpy(gsym_init_data + elem_off, &v, sl_mag);
            } else if (elem->genus == N_IDENT && elem->nomen) {
                int flabel = gsym_func_loc_quaere(elem->nomen);
                if (flabel >= 0)
                    gsym_data_reloc_adde(elem_off, DR_TEXT, flabel);
            }
        }
    }

    /* finaliter: emitte sectionēs datōrum ad fīnem plicae */
    emitte_sectiones_datorum();
    esym_finit();
}
