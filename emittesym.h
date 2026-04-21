/*
 * emittesym.h — emittens symbolicus (textus assembly pro imm)
 *
 * Scribit textum ARM64 compatibile cum cc -S ad FILE *. Status:
 * solum FILE * ipsum — omnis alia status (globales, chordae,
 * labels, init_data) in generasym.c servatur.
 *
 * Consilium: imm assemblator omnes concerns tractat — sectiones,
 * labels, relocationes, GOT @PAGE/@PAGEOFF, chordas litterales,
 * .data/.bss/.cstring directivas. Hic modulus solum emittit
 * textum quem imm parsat.
 */

#ifndef EMITTESYM_H
#define EMITTESYM_H

#include <stdio.h>
#include <stdint.h>

#include "typus.h"
#include "emitte.h"  /* pro constantibus SP, FP, LR, XZR */

/* ================================================================
 * initia et finis
 * ================================================================ */

void esym_initia(FILE *out);
void esym_finit(void);

/* ================================================================
 * sectiones et symbola (directivae)
 * ================================================================ */

void esym_sectio_text(void);
void esym_sectio_data(void);
void esym_sectio_const(void);
void esym_sectio_cstring(void);

void esym_globl(const char *nomen);
void esym_p2align(int log2);

/* functio: emittit .globl (si non staticus) + .p2align 2 + _nomen: */
void esym_func_header(const char *nomen, int est_staticus);

/* labels internae: "Lccc_<id>:" */
void esym_label_pone(int id);
void esym_str_label_pone(int id);    /* "lccc_str_<id>:" */
void esym_glob_label_pone(const char *nomen);  /* "_nomen:" */

/* ================================================================
 * data directivae
 * ================================================================ */

void esym_byte(int v);
void esym_short(int v);
void esym_long(long v);
void esym_quad(long v);
void esym_quad_sym(const char *nomen, int addendum);    /* .quad _nomen [+ addendum] */
void esym_quad_str(int str_id);                          /* .quad lccc_str_<id> */
void esym_quad_label(int label);                         /* .quad Lccc_<label> */
void esym_space(int n);
void esym_asciz(const char *data, int lon);              /* chorda cum nul final */
void esym_zerofill(
    const char *sectio_seg, const char *sectio_nom,
    const char *sym, int mag, int p2align
);

/* ================================================================
 * instructiones — omnes x-reg nisi w-reg notatum
 * ================================================================ */

void esym_raw(const char *line);  /* linea textus sine prefixo (non-instructio) */

/* motio et constantes */
void esym_movi(int rd, long imm);
void esym_mov(int rd, int rn);
void esym_movz(int rd, uint16_t imm, int shift);
void esym_movk(int rd, uint16_t imm, int shift);
void esym_movn(int rd, uint16_t imm, int shift);
void esym_fconst(int dd, double val);     /* carricat bit-pattern in d<dd> */

/* arithmetica */
void esym_add(int rd, int rn, int rm);
void esym_addi(int rd, int rn, int imm);
void esym_sub(int rd, int rn, int rm);
void esym_subi(int rd, int rn, int imm);
void esym_mul(int rd, int rn, int rm);
void esym_sdiv(int rd, int rn, int rm);
void esym_udiv(int rd, int rn, int rm);

/* logica */
void esym_and(int rd, int rn, int rm);
void esym_orr(int rd, int rn, int rm);
void esym_eor(int rd, int rn, int rm);
void esym_lsl(int rd, int rn, int rm);
void esym_lsr(int rd, int rn, int rm);
void esym_asr(int rd, int rn, int rm);
void esym_neg(int rd, int rm);
void esym_mvn(int rd, int rm);

/* comparatio — w-reg (pro genera.c usu) */
void esym_cmp(int rn, int rm, int sz);
void esym_cmpi(int rn, int imm, int sz);
void esym_cset(int rd, int cond);

/* rami */
void esym_b_label(int label);
void esym_bl_label(int label);
void esym_bl_sym(const char *nomen);
void esym_bcond_label(int cond, int label);
void esym_blr(int rn);
void esym_ret(void);
void esym_cbz_label(int rt, int label);
void esym_cbnz_label(int rt, int label);

/* carrica et salva */
void esym_stp_pre(int rt1, int rt2, int rn, int imm);
void esym_ldp_post(int rt1, int rt2, int rn, int imm);
void esym_ldr64(int rt, int rn, int imm);
void esym_ldr32(int rt, int rn, int imm);
void esym_ldrsw(int rt, int rn, int imm);
void esym_ldrsh(int rt, int rn, int imm);
void esym_ldrsb(int rt, int rn, int imm);
void esym_ldrh(int rt, int rn, int imm);
void esym_ldrb(int rt, int rn, int imm);
void esym_str64(int rt, int rn, int imm);
void esym_str32(int rt, int rn, int imm);
void esym_strh(int rt, int rn, int imm);
void esym_strb(int rt, int rn, int imm);
void esym_store(int rt, int rn, int offset, int mag);
void esym_load(int rd, int rn, int offset, int mag);
void esym_load_unsigned(int rd, int rn, int offset, int mag);

/* extensiones */
void esym_sxtw(int rd, int rn);
void esym_sxtb(int rd, int rn);
void esym_sxth(int rd, int rn);
void esym_uxtb(int rd, int rn);
void esym_uxth(int rd, int rn);
void esym_uxtw(int rd, int rn);

/* FP — numeri absoluti registri (0..31). Contextus operationis format
 * (s vs d) — functiones d-reg adhibent. */
void esym_fldr64(int dt, int rn, int imm);
void esym_fstr64(int dt, int rn, int imm);
void esym_fldr32(int dt, int rn, int imm);
void esym_fstr32(int dt, int rn, int imm);
void esym_fadd(int rd, int rn, int rm);
void esym_fsub(int rd, int rn, int rm);
void esym_fmul(int rd, int rn, int rm);
void esym_fdiv(int rd, int rn, int rm);
void esym_fneg(int rd, int rn);
void esym_fcmp(int rn, int rm);
void esym_fcvt_sd(int rd, int rn);   /* float → double */
void esym_fcvt_ds(int rd, int rn);   /* double → float */
void esym_fmov_dd(int dd, int dn);   /* pro reditu FP */
void esym_int_to_double(int r, typus_t *src);
void esym_double_to_int(int r);
void esym_fload_from_addr(int rd, int rn, typus_t *t);
void esym_fstore_to_addr(int rs, int rn, typus_t *t);
void esym_load_from_addr(int r, typus_t *t);

/* campus bitōrum */
void esym_bfi(int wd, int wn, int lsb, int width);
void esym_ubfx(int wd, int wn, int lsb, int width);
void esym_sbfx(int wd, int wn, int lsb, int width);

/* ADRP/ADD et ADRP/LDR per symbolum — imm relocationes tractat */
void esym_adrp_add_sym(int rd, const char *nomen);          /* adrp xR, _sym@PAGE ; add xR, xR, _sym@PAGEOFF */
void esym_adrp_add_str(int rd, int str_id);                 /* eadem, sed chorda litteralis */
void esym_adrp_ldr_got(int rd, const char *nomen);          /* adrp xR, _sym@GOTPAGE ; ldr xR, [xR, _sym@GOTPAGEOFF] */
void esym_adr_label(int rd, int label);                     /* adr xR, Lccc_<label> */

/* zerōs ad spatium in acervō */
void esym_imple_zeris(int off_basis, int magnitudo);

#endif /* EMITTESYM_H */
