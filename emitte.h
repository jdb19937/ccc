/*
 * emitte.h — declarationes emissionis ARM64 et status communis
 *
 * Omnes alvei et functiones emissionis quae inter genera.c,
 * emitte.c et scribo.c communicantur.
 */

#ifndef EMITTE_H
#define EMITTE_H

#include <stdint.h>

#include "typus.h"

/* ================================================================
 * limites emissionis
 * ================================================================ */

#define MAX_CODEX       (4*1048576)
#define MAX_DATA        (16*1048576)
#define MAX_CHORDAE_LIT 16384
#define MAX_GOT         512
#define MAX_FIXUPS      262144
#define MAX_LABELS      65536
#define MAX_GLOBALES    4096  /* §5.2.4.1: minimum 4095 external identifiers */

/* ================================================================
 * fixup (relocationes)
 * ================================================================ */

enum {
    FIX_ADRP,              /* ADRP ad chorda/datum */
    FIX_ADD_LO12,          /* ADD #lo12 ad chorda/datum */
    FIX_ADRP_GOT,          /* ADRP ad intransum GOT */
    FIX_LDR_GOT_LO12,     /* LDR [x, #lo12] ad intransum GOT */
    FIX_BRANCH,            /* B ad label */
    FIX_BCOND,             /* B.cond ad label */
    FIX_BL,                /* BL ad label */
    FIX_BL_EXT,            /* BL ad symbolum externum (target = GOT index) */
    FIX_TBZ,               /* TBZ Xt,#bit,label (target=label, mag=bit) */
    FIX_TBNZ,              /* TBNZ Xt,#bit,label */
    FIX_CBZ,               /* CBZ ad label */
    FIX_CBNZ,              /* CBNZ ad label */
    FIX_ADRP_DATA,         /* ADRP ad datum globale */
    FIX_ADD_LO12_DATA,     /* ADD #lo12 ad datum globale */
    FIX_LDR_LO12_DATA,    /* LDR [x, #lo12] ad datum globale */
    FIX_STR_LO12_DATA,    /* STR [x, #lo12] ad datum globale */
    FIX_ADR_LABEL,         /* ADR Xn, label (adresse codicis) */
    FIX_ADRP_TEXT,         /* ADRP ad offset in __text (target = codex offset) */
    FIX_ADD_LO12_TEXT,     /* ADD #lo12 ad offset in __text */
    FIX_ADRP_IDATA,        /* ADRP ad datum initiatum (target = offset in init_data) */
    FIX_ADD_LO12_IDATA,    /* ADD #lo12 ad datum initiatum */
    FIX_LDR_LO12_IDATA,    /* LDR [x, #lo12] ad datum initiatum */
    FIX_STR_LO12_IDATA,    /* STR [x, #lo12] ad datum initiatum */
};

typedef struct {
    int genus;
    int offset;             /* offset in codice */
    int target;             /* label / chorda_id / got_id / glob_id */
    int magnitudo_accessus; /* pro LDR/STR: 1,2,4,8 */
} fixup_t;

/* ================================================================
 * relocationes datorum (indices in init_data qui adresses continent)
 *
 * ARM64_RELOC_UNSIGNED in sectionibus __data/__const:
 * octo octeti in init_data qui adressam finalem continere debent.
 * ================================================================ */

enum {
    DR_CSTRING,     /* scopus in __cstring */
    DR_TEXT,        /* scopus in __text */
    DR_IDATA,       /* scopus in init_data */
    DR_EXT_FUNC,    /* scopus: functiō externa (target = GOT index) */
};

typedef struct {
    int idata_offset;   /* offset in init_data ubi 8 octeti scribendi sunt */
    int genus;          /* DR_CSTRING, DR_TEXT, DR_IDATA, DR_EXT_FUNC */
    int target;         /* offset in sectione scopi vel index GOT */
} data_reloc_t;

#define MAX_DATA_RELOCS 4096

extern data_reloc_t *data_relocs;
extern int num_data_relocs;

void data_reloc_adde(int idata_offset, int genus, int target);

/* ================================================================
 * ligātiōnēs dātōrum ad symbola externa (dylib)
 *
 * Offset in init_data quem dyld implēbit cum adresse symboli
 * externī post onerātiōnem. Ūsus typicus: __objc_classrefs.
 * ================================================================ */

typedef struct {
    int  idata_offset;
    char sym_nomen[256];
} data_bind_t;

#define MAX_DATA_BINDS 1024

extern data_bind_t *data_binds;
extern int num_data_binds;

void data_bind_adde(int idata_offset, const char *sym_nomen);

/* ================================================================
 * intransus GOT
 * ================================================================ */

typedef struct {
    char nomen[256];        /* nomen symboli (cum _ praefixo) */
} got_intrans_t;

/* ================================================================
 * chorda litteralis
 * ================================================================ */

typedef struct {
    char *data;
    int longitudo;
    int offset;             /* offset in sectione __cstring */
} chorda_lit_t;

/* ================================================================
 * variabilis globalis
 * ================================================================ */

typedef struct {
    char nomen[256];
    typus_t *typus;
    int magnitudo;
    int colineatio;
    int est_bss;
    int bss_offset;
    int data_offset;
    int est_staticus;
    long valor_initialis;   /* pro simplicibus initialibus */
    int habet_valorem;
} globalis_t;

/* ================================================================
 * registra ARM64
 * ================================================================ */

#define SP  31
#define XZR 31
#define FP  29
#define LR  30

/* ================================================================
 * emmerae sectionum init_data
 *
 * Sectiones __DATA a clang emissae (imprimis __objc_*) debent in
 * executabili servare nomina propria, ut runtime ObjC classes
 * inveniat et registret. Ergo init_data in emmeras dividitur per
 * nomen sectionis. Omnes in uno alveo init_data iacent, sed
 * scribo.c plures sectiones Mach-O emittit, quaeque in emmeram
 * propriam pointens.
 * ================================================================ */

#define N_IDATA_EMM 18

typedef struct {
    const char *sectname;   /* nomen sectionis in executabili */
    uint32_t    flags;      /* S_REGULAR, S_LITERAL_POINTERS, etc. */
    int         align_log2;
    int         dedup;      /* 1 = solum prima instantia inclūditur */
    int         start;      /* offset in init_data (positus post liga) */
    int         size;       /* magnitudo in octetis */
} idata_emm_t;

extern idata_emm_t idata_emm[N_IDATA_EMM];

/* resolve nomen sectionis ad indicem emmerae. Sectiones ignotae
 * (e.g. __compact_unwind) in emmeram __data pertinent. */
int idata_emm_pro_nomine(const char *sectname);

/* ================================================================
 * alvei communes
 * ================================================================ */

extern uint8_t *codex;
extern int codex_lon;
extern int data_lon;

extern uint8_t *init_data;
extern int init_data_lon;

extern chorda_lit_t *chordae;
extern int num_chordarum;
extern uint8_t *chordae_data;
extern int chordae_lon;

extern got_intrans_t *got;
extern int num_got;

extern fixup_t *fixups;
extern int num_fixups;

extern int *labels;
extern int num_labels;

extern globalis_t *globales;
extern int num_globalium;

/* ================================================================
 * emissio instructionum ARM64
 * ================================================================ */

void emitte_initia(void);
void emit32(uint32_t v);
int  label_novus(void);
void label_pone(int id);

/* registra et moti */
void emit_movi(int rd, long imm);
void emit_mov(int rd, int rn);
void emit_movz(int rd, uint16_t imm, int shift);
void emit_movk(int rd, uint16_t imm, int shift);
void emit_movn(int rd, uint16_t imm, int shift);

/* arithmetica */
void emit_add(int rd, int rn, int rm);
void emit_addi(int rd, int rn, int imm);
void emit_sub(int rd, int rn, int rm);
void emit_subi(int rd, int rn, int imm);
void emit_mul(int rd, int rn, int rm);
void emit_sdiv(int rd, int rn, int rm);
void emit_udiv(int rd, int rn, int rm);

/* logica */
void emit_and(int rd, int rn, int rm);
void emit_orr(int rd, int rn, int rm);
void emit_eor(int rd, int rn, int rm);
void emit_lsl(int rd, int rn, int rm);
void emit_lsr(int rd, int rn, int rm);
void emit_asr(int rd, int rn, int rm);
void emit_mvn(int rd, int rm);

/* comparatio */
void emit_cmp(int rn, int rm);
void emit_cmpi(int rn, int imm);
void emit_cset(int rd, int cond);

/* rami */
void emit_b_label(int label);
void emit_bl_label(int label);
void emit_bcond_label(int cond, int label);
void emit_blr(int rn);
void emit_ret(void);
void emit_cbnz_label(int rt, int label);

/* carrica et salva */
void emit_stp_pre(int rt1, int rt2, int rn, int imm);
void emit_ldp_post(int rt1, int rt2, int rn, int imm);
void emit_ldr64(int rt, int rn, int imm);
void emit_ldr32(int rt, int rn, int imm);
void emit_ldrsw(int rt, int rn, int imm);
void emit_ldrsh(int rt, int rn, int imm);
void emit_ldrsb(int rt, int rn, int imm);
void emit_ldrh(int rt, int rn, int imm);
void emit_ldrb(int rt, int rn, int imm);
void emit_str64(int rt, int rn, int imm);
void emit_str32(int rt, int rn, int imm);
void emit_strh(int rt, int rn, int imm);
void emit_strb(int rt, int rn, int imm);
void emit_sxtw(int rd, int rn);
void emit_sxtb(int rd, int rn);
void emit_sxth(int rd, int rn);
void emit_uxtb(int rd, int rn);
void emit_uxth(int rd, int rn);
void emit_uxtw(int rd, int rn);

/* fixups et data */
void emit_adrp_fixup(int rd, int genus, int target);
void fixup_adde(int genus, int offset, int target, int mag);
int  chorda_adde(const char *data, int lon);
int  got_adde(const char *nomen);

/* ================================================================
 * emissio instructionum ARM64 pro typis fluitantibus
 *
 * ARM64 SIMD/FP registra: d0–d31 (64-bit double),
 *                          s0–s31 (32-bit float, infima pars dn).
 *
 * Annex F §F.3: +, -, *, / praebent operationes IEC 60559.
 * ================================================================ */

/* FMOV Sd, Wn — transfer 32-bit integrum ad FP singulare */
void emit_fmov_sw(int sd, int wn);

/* FMOV Wn, Sd — transfer FP singulare ad 32-bit integrum */
void emit_fmov_ws(int wd, int sn);

/* FADD Sd, Sn, Sm — additio singularis */
void emit_fadds(int fd, int fn, int fm);

/* FSUB Sd, Sn, Sm — subtractio singularis */
void emit_fsubs(int fd, int fn, int fm);

/* FMUL Sd, Sn, Sm — multiplicatio singularis */
void emit_fmuls(int fd, int fn, int fm);

/* FDIV Sd, Sn, Sm — divisio singularis */
void emit_fdivs(int fd, int fn, int fm);

/* FNEG Sd, Sn — negatio singularis */
void emit_fnegs(int fd, int fn);

/* FCMP Sn, Sm — comparatio singularis */
void emit_fcmps(int fn, int fm);

/* LDR Dt, [Xn, #imm] — carrica 64-bit duplum ex memoria */
void emit_fldr64(int ft, int xn, int imm);

/* STR Dt, [Xn, #imm] — salva 64-bit duplum in memoriam */
void emit_fstr64(int ft, int xn, int imm);

/* LDR St, [Xn, #imm] — carrica 32-bit singulare ex memoria */
void emit_fldr32(int ft, int xn, int imm);

/* STR St, [Xn, #imm] — salva 32-bit singulare in memoriam */
void emit_fstr32(int ft, int xn, int imm);

/* §6.3.1.5¶1: FCVT Dd, Sn — promove float ad double */
void emit_fcvt_ds(int dd, int sn);

/* §6.3.1.5¶2: FCVT Sd, Dn — demove double ad float */
void emit_fcvt_sd(int sd, int dn);

/* §6.3.1.4¶1: FCVTZS Wd, Sn — converte singulare ad 32-bit integrum signatum */
void emit_fcvtzs_wd(int wd, int sn);

/* §6.3.1.4¶2: SCVTF Sd, Wn — converte 32-bit integrum signatum ad singulare */
void emit_scvtf_sw(int sd, int wn);


typedef struct {
    char nomen[256];
    int label;
    int est_staticus;
} func_loc_t;

extern func_loc_t *func_loci;
extern int num_func_loc;

int  func_loc_quaere(const char *nomen);
int  func_loc_adde(const char *nomen, int est_staticus);

#endif /* EMITTE_H */
