/*
 * fluat.h — sustentatio typorum fluitantium (float, double)
 *
 * ISO/IEC 9899:1999 (C99) §6.2.5¶10–11: tres typi reales fluitantes:
 *   float, double, long double.
 * §6.7.2: specificatores typorum includunt 'float' et 'double'.
 *
 * Annex F (IEC 60559): float = IEC 60559 singularis (32-bit),
 *                      double = IEC 60559 duplex (64-bit).
 *
 * ARM64 ABI: argumenta fluitantia in d0–d7, reditus in d0.
 * Registra SIMD/FP: d0–d31 (64-bit), s0–s31 (32-bit visu).
 */

#ifndef FLUAT_H
#define FLUAT_H

#include "typus.h"

/* ================================================================
 * typi praefiniti — float et double
 *
 * §6.2.5¶10: float ⊂ double ⊂ long double
 * Annex F §F.2: float = IEC 60559 singularis (4 octeti, col 4)
 *               double = IEC 60559 duplex (8 octeti, col 8)
 * ================================================================ */

extern typus_t *ty_float;
extern typus_t *ty_double;

/* ================================================================
 * praedicata typorum
 * ================================================================ */

/* §6.2.5¶10: estne typus fluitans? (float vel double) */
int typus_est_fluat(const typus_t *t);

/* §6.3.1.8: conversiones arithmeticae usitae —
 * si uterque operandus fluitans, communis typus est maior;
 * si unus fluitans et alter integer, integer convertitur */
typus_t *typus_communis_fluat(typus_t *a, typus_t *b);

/* ================================================================
 * signum — T_NUM_FLUAT
 *
 * §6.4.4.2: constans fluitans:
 *   decimal-floating-constant | hexadecimal-floating-constant
 *
 * Valor servatur ut 64-bit IEEE 754 double in campo
 * valor_f signi (vel per punning in campo 'valor' longi).
 * ================================================================ */

/* nota: T_NUM_FLUAT definitus est in ccc.h, inter T_NUM et T_STR */

/* ================================================================
 * nodus AST — N_NUM_FLUAT
 *
 * §6.4.4.2¶4: constans fluitans sine suffixo habet typum double.
 *             cum suffixo f/F habet typum float.
 *             cum suffixo l/L habet typum long double.
 *
 * Valor servatur in nodo per type-punning:
 *   double d; memcpy(&n->valor, &d, 8);
 * ================================================================ */

/* nota: N_NUM_FLUAT definitus est in ccc.h */

/* ================================================================
 * emissio instructionum ARM64 pro typis fluitantibus
 *
 * ARM64 SIMD/FP registra: d0–d31 (64-bit double),
 *                          s0–s31 (32-bit float, infima pars dn).
 *
 * Annex F §F.3: +, -, *, / praebent operationes IEC 60559.
 * ================================================================ */

/* FMOV Dd, Xn — transfer registrum integrum ad registrum FP
 * (ARM64: 0x9E670000 | (rn << 5) | rd) */
void emit_fmov_dx(int fd, int xn);

/* FMOV Xn, Dd — transfer registrum FP ad registrum integrum
 * (ARM64: 0x9E660000 | (rn << 5) | rd) */
void emit_fmov_xd(int xd, int fn);

/* FMOV Sd, Wn — transfer 32-bit integrum ad FP singulare */
void emit_fmov_sw(int sd, int wn);

/* FMOV Wn, Sd — transfer FP singulare ad 32-bit integrum */
void emit_fmov_ws(int wd, int sn);

/* §6.5.6: FADD Dd, Dn, Dm — additio duplex */
void emit_fadd(int fd, int fn, int fm);

/* §6.5.6: FSUB Dd, Dn, Dm — subtractio duplex */
void emit_fsub(int fd, int fn, int fm);

/* §6.5.5: FMUL Dd, Dn, Dm — multiplicatio duplex */
void emit_fmul(int fd, int fn, int fm);

/* §6.5.5: FDIV Dd, Dn, Dm — divisio duplex */
void emit_fdiv(int fd, int fn, int fm);

/* §6.5.3.3: FNEG Dd, Dn — negatio duplex */
void emit_fneg(int fd, int fn);

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

/* §6.5.8, §6.5.9: FCMP Dn, Dm — comparatio, ponit NZCV */
void emit_fcmp(int fn, int fm);

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

/* §6.3.1.4¶1: FCVTZS Xd, Dn — converte duplum ad integrum signatum (truncatio) */
void emit_fcvtzs_xd(int xd, int dn);

/* §6.3.1.4¶1: FCVTZS Wd, Sn — converte singulare ad 32-bit integrum signatum */
void emit_fcvtzs_wd(int wd, int sn);

/* §6.3.1.4¶2: SCVTF Dd, Xn — converte integrum signatum ad duplum */
void emit_scvtf_dx(int dd, int xn);

/* §6.3.1.4¶2: SCVTF Sd, Wn — converte 32-bit integrum signatum ad singulare */
void emit_scvtf_sw(int sd, int wn);

/* §6.3.1.4: UCVTF Dd, Xn — converte integrum sine signo ad duplum */
void emit_ucvtf_dx(int dd, int xn);

/* ================================================================
 * initiatio
 * ================================================================ */

void fluat_initia(void);

void emit_load_from_addr(int dest, typus_t *t);
void emit_fconst(int dreg, double val);
void emit_fload_from_addr(int dreg, int addr_reg, typus_t *t);
void emit_fstore_to_addr(int dreg, int addr_reg, typus_t *t);
void emit_int_to_double(int reg, typus_t *src_type);
void emit_double_to_int(int reg);

#endif /* FLUAT_H */
