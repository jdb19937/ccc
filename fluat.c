/*
 * fluat.c — sustentatio typorum fluitantium (float, double)
 *
 * Implementat:
 *   - typos praefinitos ty_float et ty_double
 *   - praedicata typorum fluitantium
 *   - emissionem instructionum ARM64 FP (FADD, FSUB, FMUL, etc.)
 *   - conversiones inter typos fluitantes et integros
 *   - carricationes et salvationes FP
 *
 * Referentiae normae ISO/IEC 9899:1999:
 *   §6.2.5¶10–11  — definitio typorum fluitantium
 *   §6.3.1.4      — conversiones inter fluitantes et integros
 *   §6.3.1.5      — conversiones inter typos fluitantes
 *   §6.3.1.8      — conversiones arithmeticae usitae
 *   §6.4.4.2      — constantes fluitantes
 *   §6.5.5        — operatores multiplicativi (*, /)
 *   §6.5.6        — operatores additivi (+, -)
 *   §6.5.8–9      — operatores relationales et aequalitatis
 *   Annex F       — IEC 60559 vinculum
 */

#include <stddef.h>
#include "fluat.h"
#include "emitte.h"

/* macro prō instructiōnibus FP tribus registrīs: op Fd, Fn, Fm */
#define EMIT_FP3(nomen, opcode)                                         \
    void nomen(int fd, int fn, int fm)                                  \
    {                                                                   \
        emit32((opcode) | ((fm & 0x1F) << 16) | ((fn & 0x1F) << 5)     \
               | (fd & 0x1F));                                          \
    }

/* ================================================================
 * typi praefiniti
 *
 * §6.2.5¶10: "There are three real floating types, designated
 *  as float, double, and long double."
 *
 * Annex F §F.2:
 *   float  = IEC 60559 singularis (32-bit, 4 octeti)
 *   double = IEC 60559 duplex (64-bit, 8 octeti)
 *
 * long double non sustentatur (tractatur ut double).
 * ================================================================ */

typus_t *ty_float;
typus_t *ty_double;

void fluat_initia(void)
{
    /* §6.2.5¶10, Annex F §F.2: float = IEC 60559 singularis */
    ty_float = typus_novus(TY_FLOAT);
    ty_float->magnitudo = 4;    /* 32-bit */
    ty_float->colineatio = 4;

    /* §6.2.5¶10, Annex F §F.2: double = IEC 60559 duplex */
    ty_double = typus_novus(TY_DOUBLE);
    ty_double->magnitudo = 8;   /* 64-bit */
    ty_double->colineatio = 8;
}

/* ================================================================
 * praedicata typorum
 * ================================================================ */

/*
 * §6.2.5¶10: float et double sunt typi "real floating".
 * §6.2.5¶14: integer et real floating = typi arithmetici.
 */
int typus_est_fluat(typus_t *t)
{
    if (!t)
        return 0;
    return t->genus == TY_FLOAT || t->genus == TY_DOUBLE;
}

/*
 * §6.3.1.8: conversiones arithmeticae usitae pro typis fluitantibus:
 *
 *   "First, if the corresponding real type of either operand is
 *    long double, the other operand is converted ... to long double."
 *   "Otherwise, if ... either operand is double, the other operand
 *    is converted ... to double."
 *   "Otherwise, if ... either operand is float, the other operand
 *    is converted ... to float."
 *
 * Si neuter est fluitans, reddit NULL (regulae integrorum applicandae).
 */
typus_t *typus_communis_fluat(typus_t *a, typus_t *b)
{
    int af = typus_est_fluat(a);
    int bf = typus_est_fluat(b);

    if (!af && !bf)
        return NULL; /* ambo integri — regulae integrorum applicandae */

    /* §6.3.1.8: si uterque double (vel alter double, alter minor) → double */
    if ((a && a->genus == TY_DOUBLE) || (b && b->genus == TY_DOUBLE))
        return ty_double;

    /* §6.3.1.8: si uterque float → float */
    if (af && bf)
        return ty_float;

    /* §6.3.1.8: unus fluitans, alter integer →
     * §6.3.1.4¶2: integer convertitur ad typum fluitantem. */
    if (af)
        return a;
    return b;
}

/* ================================================================
 * emissio instructionum ARM64 pro typis fluitantibus
 *
 * Codificatio secundum ARM Architecture Reference Manual (ARMv8-A).
 * Registra: d0–d31 (64-bit duplex), s0–s31 (32-bit singularis,
 *           infima 32-bit pars registri dn).
 *
 * Annex F §F.3: "+, −, *, / operators provide the IEC 60559
 *  add, subtract, multiply, and divide operations."
 * ================================================================ */

/* ----------------------------------------------------------------
 * FMOV — transfert inter registra integra et FP
 *
 * Necessarium pro:
 *   §6.4.4.2: carricatio constantium fluitantium (per registrum integrum)
 *   §6.5.16:  assignatio ad/ex variabilibus fluitantibus
 * ---------------------------------------------------------------- */

/* FMOV Dd, Xn — codificatio: 0x9E670000 | (Xn << 5) | Dd */
void emit_fmov_dx(int fd, int xn)
{
    emit32(0x9E670000 | ((xn & 0x1F) << 5) | (fd & 0x1F));
}

/* FMOV Xn, Dd — codificatio: 0x9E660000 | (Dn << 5) | Xd */
void emit_fmov_xd(int xd, int fn)
{
    emit32(0x9E660000 | ((fn & 0x1F) << 5) | (xd & 0x1F));
}

/* FMOV Sd, Wn — codificatio: 0x1E270000 | (Wn << 5) | Sd */
void emit_fmov_sw(int sd, int wn)
{
    emit32(0x1E270000 | ((wn & 0x1F) << 5) | (sd & 0x1F));
}

/* FMOV Wn, Sd — codificatio: 0x1E260000 | (Sn << 5) | Wd */
void emit_fmov_ws(int wd, int sn)
{
    emit32(0x1E260000 | ((sn & 0x1F) << 5) | (wd & 0x1F));
}

/* ----------------------------------------------------------------
 * arithmetica duplex (64-bit)
 *
 * §6.5.6: "+, − operatores" / §6.5.5: "*, / operatores"
 * Annex F §F.3: operationes conformant IEC 60559.
 * ---------------------------------------------------------------- */

/* FADD Dd, Dn, Dm */
EMIT_FP3(emit_fadd, 0x1E602800)
/* FSUB Dd, Dn, Dm */
EMIT_FP3(emit_fsub, 0x1E603800)
/* FMUL Dd, Dn, Dm */
EMIT_FP3(emit_fmul, 0x1E600800)
/* FDIV Dd, Dn, Dm */
EMIT_FP3(emit_fdiv, 0x1E601800)

/* FNEG Dd, Dn — codificatio: 0x1E614000 | (Dn << 5) | Dd */
void emit_fneg(int fd, int fn)
{
    emit32(0x1E614000 | ((fn & 0x1F) << 5) | (fd & 0x1F));
}

/* ----------------------------------------------------------------
 * arithmetica singularis (32-bit)
 *
 * Eaedem operationes, sed pro typo float (§6.2.5¶10).
 * ---------------------------------------------------------------- */

/* FADD Sd, Sn, Sm */
EMIT_FP3(emit_fadds, 0x1E202800)
/* FSUB Sd, Sn, Sm */
EMIT_FP3(emit_fsubs, 0x1E203800)
/* FMUL Sd, Sn, Sm */
EMIT_FP3(emit_fmuls, 0x1E200800)
/* FDIV Sd, Sn, Sm */
EMIT_FP3(emit_fdivs, 0x1E201800)

/* FNEG Sd, Sn — codificatio: 0x1E214000 */
void emit_fnegs(int fd, int fn)
{
    emit32(0x1E214000 | ((fn & 0x1F) << 5) | (fd & 0x1F));
}

/* ----------------------------------------------------------------
 * comparatio
 *
 * §6.5.8: relationales (<, >, <=, >=) — "if ... real floating
 *  type, the usual arithmetic conversions are performed."
 * §6.5.9: aequalitas (==, !=) — idem.
 *
 * FCMP ponit NZCV; post FCMP, CSET cum COND_EQ/NE/LT/GT/
 * LE/GE ūtitur (eaedem conditiones ac integer CMP).
 *
 * Annex F §F.3: "The relational and equality operators provide
 *  IEC 60559 comparisons."
 * ---------------------------------------------------------------- */

/* FCMP Dn, Dm — codificatio: 0x1E602000 | (Dm << 16) | (Dn << 5) */
void emit_fcmp(int fn, int fm)
{
    emit32(0x1E602000 | ((fm & 0x1F) << 16) | ((fn & 0x1F) << 5));
}

/* FCMP Sn, Sm — codificatio: 0x1E202000 */
void emit_fcmps(int fn, int fm)
{
    emit32(0x1E202000 | ((fm & 0x1F) << 16) | ((fn & 0x1F) << 5));
}

/* ----------------------------------------------------------------
 * carricatio et salvatio
 *
 * §6.5.2.2, §6.5.3.2: lvalue ad variabilem fluitantem carricatur/
 * salvatur per instructiones LDR/STR ad registra FP.
 *
 * LDR Dt, [Xn, #imm] — carricatio 64-bit duplex
 * STR Dt, [Xn, #imm] — salvatio 64-bit duplex
 * LDR St, [Xn, #imm] — carricatio 32-bit singularis
 * STR St, [Xn, #imm] — salvatio 32-bit singularis
 * ---------------------------------------------------------------- */

/* LDR Dt, [Xn, #imm] — codificatio: 0xFD400000 | (imm/8 << 10) | (Xn << 5) | Dt */
void emit_fldr64(int ft, int xn, int imm)
{
    uint32_t uimm = ((uint32_t)imm / 8) & 0xFFF;
    emit32(0xFD400000 | (uimm << 10) | ((xn & 0x1F) << 5) | (ft & 0x1F));
}

/* STR Dt, [Xn, #imm] — codificatio: 0xFD000000 | (imm/8 << 10) | (Xn << 5) | Dt */
void emit_fstr64(int ft, int xn, int imm)
{
    uint32_t uimm = ((uint32_t)imm / 8) & 0xFFF;
    emit32(0xFD000000 | (uimm << 10) | ((xn & 0x1F) << 5) | (ft & 0x1F));
}

/* LDR St, [Xn, #imm] — codificatio: 0xBD400000 | (imm/4 << 10) | (Xn << 5) | St */
void emit_fldr32(int ft, int xn, int imm)
{
    uint32_t uimm = ((uint32_t)imm / 4) & 0xFFF;
    emit32(0xBD400000 | (uimm << 10) | ((xn & 0x1F) << 5) | (ft & 0x1F));
}

/* STR St, [Xn, #imm] — codificatio: 0xBD000000 | (imm/4 << 10) | (Xn << 5) | St */
void emit_fstr32(int ft, int xn, int imm)
{
    uint32_t uimm = ((uint32_t)imm / 4) & 0xFFF;
    emit32(0xBD000000 | (uimm << 10) | ((xn & 0x1F) << 5) | (ft & 0x1F));
}

/* ----------------------------------------------------------------
 * conversiones inter typos fluitantes
 *
 * §6.3.1.5¶1: "When a float is promoted to double ...,
 *  its value is unchanged."
 * §6.3.1.5¶2: "When a double is demoted to float ...,
 *  if the value ... is in the range ... the result is the
 *  nearest representable value."
 * ---------------------------------------------------------------- */

/* FCVT Dd, Sn — float → double (promotio)
 * codificatio: 0x1E22C000 | (Sn << 5) | Dd */
void emit_fcvt_ds(int dd, int sn)
{
    emit32(0x1E22C000 | ((sn & 0x1F) << 5) | (dd & 0x1F));
}

/* FCVT Sd, Dn — double → float (demotio)
 * codificatio: 0x1E624000 | (Dn << 5) | Sd */
void emit_fcvt_sd(int sd, int dn)
{
    emit32(0x1E624000 | ((dn & 0x1F) << 5) | (sd & 0x1F));
}

/* ----------------------------------------------------------------
 * conversiones inter typos fluitantes et integros
 *
 * §6.3.1.4¶1: "When a finite value of real floating type is
 *  converted to an integer type ..., the fractional part is
 *  discarded (i.e., the value is truncated toward zero)."
 *
 * §6.3.1.4¶2: "When a value of integer type is converted to a
 *  real floating type, if the value ... can be represented
 *  exactly ..., it is unchanged."
 * ---------------------------------------------------------------- */

/* FCVTZS Xd, Dn — double → signed long (truncatio ad zero)
 * codificatio: 0x9E780000 | (Dn << 5) | Xd */
void emit_fcvtzs_xd(int xd, int dn)
{
    emit32(0x9E780000 | ((dn & 0x1F) << 5) | (xd & 0x1F));
}

/* FCVTZS Wd, Sn — float → signed int (truncatio ad zero)
 * codificatio: 0x1E380000 | (Sn << 5) | Wd */
void emit_fcvtzs_wd(int wd, int sn)
{
    emit32(0x1E380000 | ((sn & 0x1F) << 5) | (wd & 0x1F));
}

/* SCVTF Dd, Xn — signed long → double
 * codificatio: 0x9E620000 | (Xn << 5) | Dd */
void emit_scvtf_dx(int dd, int xn)
{
    emit32(0x9E620000 | ((xn & 0x1F) << 5) | (dd & 0x1F));
}

/* SCVTF Sd, Wn — signed int → float
 * codificatio: 0x1E220000 | (Wn << 5) | Sd */
void emit_scvtf_sw(int sd, int wn)
{
    emit32(0x1E220000 | ((wn & 0x1F) << 5) | (sd & 0x1F));
}

/* UCVTF Dd, Xn — unsigned long → double
 * codificatio: 0x9E630000 | (Xn << 5) | Dd */
void emit_ucvtf_dx(int dd, int xn)
{
    emit32(0x9E630000 | ((xn & 0x1F) << 5) | (dd & 0x1F));
}
