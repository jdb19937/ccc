/*
 * emitte.h — declarationes emissionis ARM64 et status communis
 *
 * Omnes alvei et functiones emissionis quae inter genera.c,
 * emitte.c et scribo.c communicantur.
 */

#ifndef EMITTE_H
#define EMITTE_H

#include "ccc.h"

/* ================================================================
 * alvei communes
 * ================================================================ */

extern uint8_t *codex;
extern int codex_lon;
extern int data_lon;

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
void emit_load(int rd, int rn, int offset, int mag);
void emit_load_unsigned(int rd, int rn, int offset, int mag);

/* fixups et data */
void emit_adrp_fixup(int rd, int genus, int target);
void fixup_adde(int genus, int offset, int target, int mag);
int  chorda_adde(const char *data, int lon);
int  got_adde(const char *nomen);
int  globalis_adde(const char *nomen, typus_t *typus, int est_staticus, long valor);

/* ================================================================
 * declarationes — scribo.c
 * ================================================================ */

void scribo_macho(const char *plica_exitus, int main_offset);
void scribo_obiectum(const char *plica_exitus);

#endif /* EMITTE_H */
