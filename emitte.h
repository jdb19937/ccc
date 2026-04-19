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
void emit_neg(int rd, int rm);
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
void emit_cbz_label(int rt, int label);
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
void emit_store(int rt, int rn, int offset, int mag);
void emit_sxtw(int rd, int rn);
void emit_sxtb(int rd, int rn);
void emit_sxth(int rd, int rn);
void emit_uxtb(int rd, int rn);
void emit_uxth(int rd, int rn);
void emit_uxtw(int rd, int rn);
void emit_load(int rd, int rn, int offset, int mag);
void emit_load_unsigned(int rd, int rn, int offset, int mag);

void emit_imple_zeris(int off_basis, int magnitudo);

/* fixups et data */
void emit_adrp_fixup(int rd, int genus, int target);
void fixup_adde(int genus, int offset, int target, int mag);
int  chorda_adde(const char *data, int lon);
int  got_adde(const char *nomen);
int  globalis_adde(const char *nomen, typus_t *typus, int est_staticus, long valor);

#endif /* EMITTE_H */
