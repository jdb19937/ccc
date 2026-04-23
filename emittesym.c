/*
 * emittesym.c — emittens symbolicus (textus assembly)
 */

#include "emittesym.h"
#include "utilia.h"
#include "typus.h"

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

static FILE *out = NULL;

/* ================================================================
 * utilitates
 * ================================================================ */

static void L(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputc('\t', out);
    vfprintf(out, fmt, ap);
    fputc('\n', out);
    va_end(ap);
}

/* nomen registri x-reg.
 * - 29 → x29, 30 → x30
 * - 31: depends — SP in memory operands, XZR in arithmetic.
 *   Utimur sp si ex contextū (mem-op base, mov-to-sp). Alter
 *   consul xzr per xn_z. */
/* alveī rotantēs — plūrēs valōrēs per ūnam invocātiōnem L() */
static char reg_bufs[8][8];
static int  reg_buf_idx = 0;
static char *reg_buf(void)
{
    char        *b = reg_bufs[reg_buf_idx];
    reg_buf_idx    = (reg_buf_idx + 1) & 7;
    return b;
}
static const char *xn(int r)
{
    char *b = reg_buf();
    switch (r) {
    case 29: strcpy(b, "x29"); return b;
    case 30: strcpy(b, "x30"); return b;
    case 31: strcpy(b, "sp");  return b;
    default: snprintf(b, 8, "x%d", r); return b;
    }
}
static const char *xn_z(int r)
{
    char *b = reg_buf();
    if (r == 31)      {
        strcpy(b, "xzr");
        return b;
    }else if (r == 29) {
        strcpy(b, "x29");
        return b;
    }else if (r == 30) {
        strcpy(b, "x30");
        return b;
    }
    snprintf(b, 8, "x%d", r);
    return b;
}
static const char *wn(int r)
{
    char *b = reg_buf();
    if (r == 31) {
        strcpy(b, "wzr");
        return b;
    }
    snprintf(b, 8, "w%d", r);
    return b;
}
static const char *dn(int r)
{
    char *b = reg_buf();
    snprintf(b, 8, "d%d", r);
    return b;
}
static const char *sn(int r)
{
    char *b = reg_buf();
    snprintf(b, 8, "s%d", r);
    return b;
}

static const char *cond_str(int c)
{
    switch (c) {
    case 0x0: return "eq";
    case 0x1: return "ne";
    case 0x2: return "hs";
    case 0x3: return "lo";
    case 0x4: return "mi";
    case 0x5: return "pl";
    case 0x6: return "vs";
    case 0x7: return "vc";
    case 0x8: return "hi";
    case 0x9: return "ls";
    case 0xA: return "ge";
    case 0xB: return "lt";
    case 0xC: return "gt";
    case 0xD: return "le";
    case 0xE: return "al";
    }
    erratum("esym: conditio ignota: %d", c);
    return "?";
}

/* ================================================================
 * inceptio et finis
 * ================================================================ */

void esym_initia(FILE *f)
{
    out = f;
    fprintf(out, "\t.section\t__TEXT,__text,regular,pure_instructions\n");
}

void esym_finit(void)
{
    fprintf(out, ".subsections_via_symbols\n");
}

/* ================================================================
 * sectiones et symbola
 * ================================================================ */

void esym_sectio_text(void)    { fprintf(out, "\t.section\t__TEXT,__text,regular,pure_instructions\n"); }
void esym_sectio_data(void)    { fprintf(out, "\t.section\t__DATA,__data\n"); }
void esym_sectio_const(void)   { fprintf(out, "\t.section\t__TEXT,__const\n"); }
void esym_sectio_cstring(void) { fprintf(out, "\t.section\t__TEXT,__cstring,cstring_literals\n"); }

void esym_globl(const char *nomen)     { fprintf(out, "\t.globl\t_%s\n", nomen); }
void esym_p2align(int n)               { fprintf(out, "\t.p2align\t%d\n", n); }

void esym_func_header(const char *nomen, int est_staticus)
{
    if (!est_staticus)
        esym_globl(nomen);
    esym_p2align(2);
    fprintf(out, "_%s:\n", nomen);
}

void esym_label_pone(int id)              { fprintf(out, "Lccc_%d:\n", id); }
void esym_str_label_pone(int id)          { fprintf(out, "lccc_str_%d:\n", id); }
void esym_glob_label_pone(const char *nm) { fprintf(out, "_%s:\n", nm); }

/* ================================================================
 * data directivae
 * ================================================================ */

void esym_byte(int v)   { L(".byte\t%d", v & 0xff); }
void esym_short(int v)  { L(".short\t%d", v & 0xffff); }
void esym_long(long v)  { L(".long\t%ld", (long)(int32_t)v); }
void esym_quad(long v)  { L(".quad\t%ld", v); }

void esym_quad_sym(const char *nomen, int addendum)
{
    if (addendum)
        L(".quad\t_%s+%d", nomen, addendum);
    else
        L(".quad\t_%s", nomen);
}

void esym_quad_str(int str_id)  { L(".quad\tlccc_str_%d", str_id); }
void esym_quad_label(int label) { L(".quad\tLccc_%d", label); }
void esym_space(int n)          { L(".space\t%d", n); }

/* §5.2.1: characteres scapabiles. scribimus ut cc -S: octal escapes pro
 * non-printābilibus, cum certīs litterālibus nōminātim. */
void esym_asciz(const char *data, int lon)
{
    fprintf(out, "\t.asciz\t\"");
    for (int i = 0; i < lon; i++) {
        unsigned char c = (unsigned char)data[i];
        if (c == '"' || c == '\\')
            fprintf(out, "\\%c", c);
        else if (c == '\n')
            fprintf(out, "\\n");
        else if (c == '\t')
            fprintf(out, "\\t");
        else if (c == '\r')
            fprintf(out, "\\r");
        else if (c >= 32 && c < 127)
            fputc(c, out);
        else
            fprintf(out, "\\%03o", c);
    }
    fprintf(out, "\"\n");
}

void esym_zerofill(const char *seg, const char *sct, const char *sym, int mag, int p2align)
{
    fprintf(out, "\t.zerofill\t%s,%s,_%s,%d,%d\n", seg, sct, sym, mag, p2align);
}

/* ================================================================
 * instructiones
 * ================================================================ */

void esym_raw(const char *line) { fprintf(out, "%s\n", line); }

/* motio et constantes */

void esym_movi(int rd, long imm)
{
    /* §6.2.5: integri magnitudinis 64 bitorum (long long). Utimur
     * movz/movk sequentiam si immediatum non cadit in 16 bit. */
    uint64_t v = (uint64_t)imm;
    /* casus faciles */
    if (imm == 0) {
        L("mov\t%s, #0", xn(rd));
        return;
    }
    if (imm > 0 && imm <= 0xFFFF) {
        L("mov\t%s, #%ld", xn(rd), imm);
        return;
    }
    if (imm < 0 && imm >= -0x10000) {
        /* MOVN Xd, #~imm16 */
        uint16_t v16 = (uint16_t)(~(uint64_t)imm);
        L("movn\t%s, #%u", xn(rd), (unsigned)v16);
        return;
    }
    /* sequentia movz/movk — emitte 4 segmenta 16-bit */
    uint16_t h0 = (uint16_t)(v & 0xFFFF);
    uint16_t h1 = (uint16_t)((v >> 16) & 0xFFFF);
    uint16_t h2 = (uint16_t)((v >> 32) & 0xFFFF);
    uint16_t h3 = (uint16_t)((v >> 48) & 0xFFFF);
    L("movz\t%s, #%u", xn(rd), (unsigned)h0);
    if (h1)
        L("movk\t%s, #%u, lsl #16", xn(rd), (unsigned)h1);
    if (h2)
        L("movk\t%s, #%u, lsl #32", xn(rd), (unsigned)h2);
    if (h3)
        L("movk\t%s, #%u, lsl #48", xn(rd), (unsigned)h3);
}

void esym_mov(int rd, int rn)                   { L("mov\t%s, %s", xn(rd), xn_z(rn)); }
void esym_movz(int rd, uint16_t v, int shift)   {
    if (shift)
        L("movz\t%s, #%u, lsl #%d", xn(rd), v, shift);
    else
        L("movz\t%s, #%u", xn(rd), v);
}
void esym_movk(int rd, uint16_t v, int shift)   {
    if (shift)
        L("movk\t%s, #%u, lsl #%d", xn(rd), v, shift);
    else
        L("movk\t%s, #%u", xn(rd), v);
}
void esym_movn(int rd, uint16_t v, int shift)   {
    if (shift)
        L("movn\t%s, #%u, lsl #%d", xn(rd), v, shift);
    else
        L("movn\t%s, #%u", xn(rd), v);
}

/* §6.2.5, Annex F: constans fluitans — carricat bit-pattern doubli
 * per movz/movk in x17, deinde fmov d, x. */
void esym_fconst(int dd, double val)
{
    uint64_t bits;
    memcpy(&bits, &val, 8);
    if (bits == 0) {
        L("fmov\t%s, xzr", dn(dd));
        return;
    }
    uint16_t h0 = (uint16_t)(bits & 0xFFFF);
    uint16_t h1 = (uint16_t)((bits >> 16) & 0xFFFF);
    uint16_t h2 = (uint16_t)((bits >> 32) & 0xFFFF);
    uint16_t h3 = (uint16_t)((bits >> 48) & 0xFFFF);
    L("movz\tx17, #%u", (unsigned)h0);
    if (h1)
        L("movk\tx17, #%u, lsl #16", (unsigned)h1);
    if (h2)
        L("movk\tx17, #%u, lsl #32", (unsigned)h2);
    if (h3)
        L("movk\tx17, #%u, lsl #48", (unsigned)h3);
    L("fmov\t%s, x17", dn(dd));
}

/* arithmetica */

void esym_add(int rd, int rn, int rm)   { L("add\t%s, %s, %s", xn(rd), xn(rn), xn_z(rm)); }
void esym_addi(int rd, int rn, int imm)
{
    if (imm < 0) {
        L("sub\t%s, %s, #%d", xn(rd), xn(rn), -imm);
        return;
    }
    L("add\t%s, %s, #%d", xn(rd), xn(rn), imm);
}
void esym_sub(int rd, int rn, int rm)   { L("sub\t%s, %s, %s", xn(rd), xn(rn), xn_z(rm)); }
void esym_subi(int rd, int rn, int imm)
{
    if (imm < 0) {
        L("add\t%s, %s, #%d", xn(rd), xn(rn), -imm);
        return;
    }
    L("sub\t%s, %s, #%d", xn(rd), xn(rn), imm);
}
void esym_mul(int rd, int rn, int rm)   { L("mul\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_sdiv(int rd, int rn, int rm)  { L("sdiv\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_udiv(int rd, int rn, int rm)  { L("udiv\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }

void esym_and(int rd, int rn, int rm)   { L("and\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_orr(int rd, int rn, int rm)   { L("orr\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_eor(int rd, int rn, int rm)   { L("eor\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_lsl(int rd, int rn, int rm)   { L("lsl\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_lsr(int rd, int rn, int rm)   { L("lsr\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_asr(int rd, int rn, int rm)   { L("asr\t%s, %s, %s", xn(rd), xn_z(rn), xn_z(rm)); }
void esym_neg(int rd, int rm)           { L("neg\t%s, %s", xn(rd), xn_z(rm)); }
void esym_mvn(int rd, int rm)           { L("mvn\t%s, %s", xn(rd), xn_z(rm)); }

/* comparatio — w-reg ad concordandam cum genera.c usū */
void esym_cmp(int rn, int rm, int sz)
{
    if (sz >= 8)
        L("cmp\t%s, %s", xn(rn), xn(rm));
    else
        L("cmp\t%s, %s", wn(rn), wn(rm));
}
void esym_cmpi(int rn, int imm, int sz)
{
    const char *r = (sz >= 8) ? xn(rn) : wn(rn);
    if (imm < 0) {
        L("cmn\t%s, #%d", r, -imm);
        return;
    }
    L("cmp\t%s, #%d", r, imm);
}
void esym_cset(int rd, int cond)        { L("cset\t%s, %s", wn(rd), cond_str(cond)); }

/* rami */
void esym_b_label(int label)            { L("b\tLccc_%d", label); }
void esym_bl_label(int label)           { L("bl\tLccc_%d", label); }  /* NB:
    pro functionibus localibus praeferimus bl _nomen; hoc pro rara usu */
void esym_bl_sym(const char *nomen)     { L("bl\t_%s", nomen); }
void esym_bcond_label(int c, int label) { L("b.%s\tLccc_%d", cond_str(c), label); }
void esym_blr(int rn)                   { L("blr\t%s", xn_z(rn)); }
void esym_ret(void)                     { L("ret"); }
void esym_cbz_label(int rt, int label)  { L("cbz\t%s, Lccc_%d", xn_z(rt), label); }
void esym_cbnz_label(int rt, int label) { L("cbnz\t%s, Lccc_%d", xn_z(rt), label); }

/* carrica et salva — pre/post index */
void esym_stp_pre(int t1, int t2, int rn, int imm)
{
    L("stp\t%s, %s, [%s, #%d]!", xn(t1), xn(t2), xn(rn), imm);
}
void esym_ldp_post(int t1, int t2, int rn, int imm)
{
    L("ldp\t%s, %s, [%s], #%d", xn(t1), xn(t2), xn(rn), imm);
}

/* LDR/STR: pro negativis vel non-alineatis utimur ldur/stur.
 * Positivus multiplex magnitudinis → ldr/str. */
static int est_imm_ldr(int imm, int step)
{
    return imm >= 0 && (imm % step) == 0 && (imm / step) <= 4095;
}

void esym_ldr64(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 8)) {
        if (imm)
            L("ldr\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
        else
            L("ldr\t%s, [%s]", xn(rt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("ldur\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldr\t%s, [x16]", xn(rt));
    }
}
void esym_ldr32(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 4)) {
        if (imm)
            L("ldr\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
        else
            L("ldr\t%s, [%s]", wn(rt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("ldur\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldr\t%s, [x16]", wn(rt));
    }
}
void esym_ldrsw(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 4))
        L("ldrsw\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
    else if (imm >= -256 && imm <= 255)
        L("ldursw\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldrsw\t%s, [x16]", xn(rt));
    }
}
void esym_ldrsh(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 2))
        L("ldrsh\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
    else if (imm >= -256 && imm <= 255)
        L("ldursh\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldrsh\t%s, [x16]", xn(rt));
    }
}
void esym_ldrsb(int rt, int rn, int imm)
{
    if (imm >= 0 && imm <= 4095)
        L("ldrsb\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
    else if (imm >= -256 && imm <= 255)
        L("ldursb\t%s, [%s, #%d]", xn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldrsb\t%s, [x16]", xn(rt));
    }
}
void esym_ldrh(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 2))
        L("ldrh\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else if (imm >= -256 && imm <= 255)
        L("ldurh\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldrh\t%s, [x16]", wn(rt));
    }
}
void esym_ldrb(int rt, int rn, int imm)
{
    if (imm >= 0 && imm <= 4095)
        L("ldrb\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else if (imm >= -256 && imm <= 255)
        L("ldurb\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldrb\t%s, [x16]", wn(rt));
    }
}

void esym_str64(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 8)) {
        if (imm)
            L("str\t%s, [%s, #%d]", xn_z(rt), xn(rn), imm);
        else
            L("str\t%s, [%s]", xn_z(rt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("stur\t%s, [%s, #%d]", xn_z(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("str\t%s, [x16]", xn_z(rt));
    }
}
void esym_str32(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 4)) {
        if (imm)
            L("str\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
        else
            L("str\t%s, [%s]", wn(rt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("stur\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("str\t%s, [x16]", wn(rt));
    }
}
void esym_strh(int rt, int rn, int imm)
{
    if (est_imm_ldr(imm, 2))
        L("strh\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else if (imm >= -256 && imm <= 255)
        L("sturh\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("strh\t%s, [x16]", wn(rt));
    }
}
void esym_strb(int rt, int rn, int imm)
{
    if (imm >= 0 && imm <= 4095)
        L("strb\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else if (imm >= -256 && imm <= 255)
        L("sturb\t%s, [%s, #%d]", wn(rt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("strb\t%s, [x16]", wn(rt));
    }
}

void esym_store(int rt, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: esym_strb(rt, rn, offset); break;
    case 2: esym_strh(rt, rn, offset); break;
    case 4: esym_str32(rt, rn, offset); break;
    case 8: esym_str64(rt, rn, offset); break;
    default: erratum("esym_store: magnitudo invalida: %d", mag);
    }
}

/* esym_memcpy_bytes: copia mag bytes ex [rsrc+src_off] ad [rdst+dst_off]
 * per chunks 8/4/1. Utitur r_tmp ut registro temporalis. */
void esym_memcpy_bytes(
    int rdst, int dst_off, int rsrc, int src_off,
    int mag, int r_tmp
) {
    int k = 0;
    while (k + 8 <= mag) {
        esym_ldr64(r_tmp, rsrc, src_off + k);
        esym_str64(r_tmp, rdst, dst_off + k);
        k += 8;
    }
    while (k + 4 <= mag) {
        esym_ldr32(r_tmp, rsrc, src_off + k);
        esym_str32(r_tmp, rdst, dst_off + k);
        k += 4;
    }
    while (k < mag) {
        esym_ldrb(r_tmp, rsrc, src_off + k);
        esym_strb(r_tmp, rdst, dst_off + k);
        k++;
    }
}

void esym_load(int rd, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: esym_ldrsb(rd, rn, offset); break;
    case 2: esym_ldrsh(rd, rn, offset); break;
    case 4: esym_ldrsw(rd, rn, offset); break;
    case 8: esym_ldr64(rd, rn, offset); break;
    default: erratum("esym_load: magnitudo invalida: %d", mag);
    }
}

void esym_load_unsigned(int rd, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: esym_ldrb(rd, rn, offset); break;
    case 2: esym_ldrh(rd, rn, offset); break;
    case 4: esym_ldr32(rd, rn, offset); break;
    case 8: esym_ldr64(rd, rn, offset); break;
    default: erratum("esym_load_unsigned: magnitudo invalida: %d", mag);
    }
}

/* extensiones */
void esym_sxtw(int rd, int rn)  { L("sxtw\t%s, %s", xn(rd), wn(rn)); }
void esym_sxtb(int rd, int rn)  { L("sxtb\t%s, %s", xn(rd), wn(rn)); }
void esym_sxth(int rd, int rn)  { L("sxth\t%s, %s", xn(rd), wn(rn)); }
void esym_uxtb(int rd, int rn)  { L("uxtb\t%s, %s", wn(rd), wn(rn)); }
void esym_uxth(int rd, int rn)  { L("uxth\t%s, %s", wn(rd), wn(rn)); }
void esym_uxtw(int rd, int rn)  { L("mov\t%s, %s", wn(rd), wn(rn)); }  /* MOV Wd, Wn cleares upper */

/* FP */
void esym_fldr64(int dt, int rn, int imm)
{
    if (est_imm_ldr(imm, 8)) {
        if (imm)
            L("ldr\t%s, [%s, #%d]", dn(dt), xn(rn), imm);
        else
            L("ldr\t%s, [%s]", dn(dt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("ldur\t%s, [%s, #%d]", dn(dt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldr\t%s, [x16]", dn(dt));
    }
}
void esym_fstr64(int dt, int rn, int imm)
{
    if (est_imm_ldr(imm, 8)) {
        if (imm)
            L("str\t%s, [%s, #%d]", dn(dt), xn(rn), imm);
        else
            L("str\t%s, [%s]", dn(dt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("stur\t%s, [%s, #%d]", dn(dt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("str\t%s, [x16]", dn(dt));
    }
}
void esym_fldr32(int dt, int rn, int imm)
{
    if (est_imm_ldr(imm, 4)) {
        if (imm)
            L("ldr\t%s, [%s, #%d]", sn(dt), xn(rn), imm);
        else
            L("ldr\t%s, [%s]", sn(dt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("ldur\t%s, [%s, #%d]", sn(dt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("ldr\t%s, [x16]", sn(dt));
    }
}
void esym_fstr32(int dt, int rn, int imm)
{
    if (est_imm_ldr(imm, 4)) {
        if (imm)
            L("str\t%s, [%s, #%d]", sn(dt), xn(rn), imm);
        else
            L("str\t%s, [%s]", sn(dt), xn(rn));
    } else if (imm >= -256 && imm <= 255)
        L("stur\t%s, [%s, #%d]", sn(dt), xn(rn), imm);
    else {
        L("mov\tx16, #%d", imm);
        L("add\tx16, %s, x16", xn(rn));
        L("str\t%s, [x16]", sn(dt));
    }
}
void esym_fadd(int rd, int rn, int rm) { L("fadd\t%s, %s, %s", dn(rd), dn(rn), dn(rm)); }
void esym_fsub(int rd, int rn, int rm) { L("fsub\t%s, %s, %s", dn(rd), dn(rn), dn(rm)); }
void esym_fmul(int rd, int rn, int rm) { L("fmul\t%s, %s, %s", dn(rd), dn(rn), dn(rm)); }
void esym_fdiv(int rd, int rn, int rm) { L("fdiv\t%s, %s, %s", dn(rd), dn(rn), dn(rm)); }
void esym_fneg(int rd, int rn)         { L("fneg\t%s, %s", dn(rd), dn(rn)); }
void esym_fcmp(int rn, int rm)         { L("fcmp\t%s, %s", dn(rn), dn(rm)); }
void esym_fcvt_sd(int rd, int rn)      { L("fcvt\t%s, %s", dn(rd), sn(rn)); }
void esym_fcvt_ds(int rd, int rn)      { L("fcvt\t%s, %s", sn(rd), dn(rn)); }
void esym_fmov_dd(int rd, int rn)      { L("fmov\t%s, %s", dn(rd), dn(rn)); }

/* §6.3.1.4¶2: integer → fluitans. scvtf cum x- vel w-reg secundum magnitudinem.
 * Signed vs unsigned per est_sine_signo. Integer ante in r sub typū src. */
void esym_int_to_double(int r, typus_t *src)
{
    int mag        = typus_magnitudo(src);
    int sgn        = !src->est_sine_signo;
    const char *op = sgn ? "scvtf" : "ucvtf";
    if (mag == 8)
        L("%s\t%s, %s", op, dn(r), xn(r));
    else
        L("%s\t%s, %s", op, dn(r), wn(r));
}

/* §6.3.1.4¶1: fluitans → integer (truncatio) */
void esym_double_to_int(int r)
{
    L("fcvtzs\t%s, %s", xn(r), dn(r));
}

void esym_fload_from_addr(int rd, int rn, typus_t *t)
{
    /* genera.c utitur d-reg internē — si typus est float, carricat
     * in s-reg ET convertit ad d-reg. */
    if (t->genus == TY_FLOAT) {
        L("ldr\t%s, [%s]", sn(rd), xn(rn));
        L("fcvt\t%s, %s", dn(rd), sn(rd));
    } else
        L("ldr\t%s, [%s]", dn(rd), xn(rn));
}

void esym_fstore_to_addr(int rs, int rn, typus_t *t)
{
    if (t->genus == TY_FLOAT) {
        L("fcvt\t%s, %s", sn(rs), dn(rs));
        L("str\t%s, [%s]", sn(rs), xn(rn));
    } else
        L("str\t%s, [%s]", dn(rs), xn(rn));
}

void esym_load_from_addr(int r, typus_t *t)
{
    int mag = typus_magnitudo(t);
    if (t->est_sine_signo)
        esym_load_unsigned(r, r, 0, mag > 8 ? 8 : mag);
    else
        esym_load(r, r, 0, mag > 8 ? 8 : mag);
}

/* campus bitōrum */
void esym_bfi(int wd, int wn_, int lsb, int width)  { L("bfi\t%s, %s, #%d, #%d", wn(wd), wn(wn_), lsb, width); }
void esym_ubfx(int wd, int wn_, int lsb, int width) { L("ubfx\t%s, %s, #%d, #%d", wn(wd), wn(wn_), lsb, width); }
void esym_sbfx(int wd, int wn_, int lsb, int width) { L("sbfx\t%s, %s, #%d, #%d", wn(wd), wn(wn_), lsb, width); }

/* ADRP/ADD/LDR per symbolum */
void esym_adrp_add_sym(int rd, const char *nomen)
{
    L("adrp\t%s, _%s@PAGE", xn(rd), nomen);
    L("add\t%s, %s, _%s@PAGEOFF", xn(rd), xn(rd), nomen);
}
void esym_adrp_add_str(int rd, int str_id)
{
    L("adrp\t%s, lccc_str_%d@PAGE", xn(rd), str_id);
    L("add\t%s, %s, lccc_str_%d@PAGEOFF", xn(rd), xn(rd), str_id);
}
void esym_adrp_ldr_got(int rd, const char *nomen)
{
    L("adrp\t%s, _%s@GOTPAGE", xn(rd), nomen);
    L("ldr\t%s, [%s, _%s@GOTPAGEOFF]", xn(rd), xn(rd), nomen);
}
void esym_adr_label(int rd, int label)
{
    L("adr\t%s, Lccc_%d", xn(rd), label);
}

/* zerōs ad spatium in acervō. off_basis est offset signatus relativus
 * ad FP (plerumque NEGATIVUS pro localibus). Scribimus ad
 * [FP, off_basis], [FP, off_basis+8], ... in ordine crescenti memoriae
 * ut emitte.c emit_imple_zeris (quae eundem layout producit). */
void esym_imple_zeris(int off_basis, int magnitudo)
{
    if (magnitudo <= 0)
        return;
    int rem = magnitudo;
    int cur = off_basis;
    while (rem >= 8) {
        esym_str64(31 /*xzr*/, 29 /*FP*/, cur);
        cur += 8;
        rem -= 8;
    }
    while (rem >= 4) {
        esym_str32(31 /*wzr*/, 29 /*FP*/, cur);
        cur += 4;
        rem -= 4;
    }
    while (rem >= 2) {
        esym_strh(31, 29, cur);
        cur += 2;
        rem -= 2;
    }
    while (rem >= 1) {
        esym_strb(31, 29, cur);
        cur += 1;
        rem -= 1;
    }
}
