/*
 * emitte.c — emissio instructionum ARM64 et alvei communes
 *
 * Alvei pro codice, chordis, GOT, fixups, globalibus.
 * Functiones emissionis pro omnibus instructionibus ARM64.
 */

#include "emitte.h"
#include "typus.h"

/* ================================================================
 * alvei et status
 * ================================================================ */

uint8_t *codex;
int codex_lon = 0;

int data_lon = 0;

uint8_t *init_data = NULL;
int init_data_lon  = 0;

chorda_lit_t *chordae;
int num_chordarum = 0;
uint8_t *chordae_data;
int chordae_lon = 0;

got_intrans_t *got;
int num_got = 0;

fixup_t *fixups;
int num_fixups = 0;

int *labels;
int num_labels = 0;

globalis_t *globales;
int num_globalium = 0;

/* ================================================================
 * labels
 * ================================================================ */

int label_novus(void)
{
    if (num_labels >= MAX_LABELS)
        erratum("nimis multa labels");
    labels[num_labels] = -1;
    return num_labels++;
}

void label_pone(int id)
{
    labels[id] = codex_lon;
}

/* ================================================================
 * chordae litterales
 * ================================================================ */

int chorda_adde(const char *data, int lon)
{
    int id = num_chordarum;
    chordae[id].data = (char *)&chordae_data[chordae_lon];
    memcpy(&chordae_data[chordae_lon], data, lon);
    chordae_data[chordae_lon + lon] = '\0';
    chordae[id].longitudo = lon + 1;
    chordae[id].offset = chordae_lon;
    chordae_lon += lon + 1;
    num_chordarum++;
    return id;
}

/* ================================================================
 * GOT
 * ================================================================ */

int got_adde(const char *nomen)
{
    for (int i = 0; i < num_got; i++)
        if (strcmp(got[i].nomen, nomen) == 0)
            return i;
    if (num_got >= MAX_GOT)
        erratum("nimis multi GOT intrantes");
    int id = num_got++;
    snprintf(got[id].nomen, 256, "%s", nomen);
    return id;
}

/* ================================================================
 * globales
 * ================================================================ */

int globalis_adde(
    const char *nomen, typus_t *typus,
    int est_staticus, long valor
) {
    if (!est_staticus) {
        for (int i = 0; i < num_globalium; i++)
            if (strcmp(globales[i].nomen, nomen) == 0)
                return i;
    }
    if (num_globalium >= MAX_GLOBALES)
        erratum("nimis multae globales");
    int id = num_globalium++;
    strncpy(globales[id].nomen, nomen, 255);
    globales[id].typus = typus;
    globales[id].magnitudo = typus_magnitudo(typus);
    globales[id].colineatio = typus_colineatio(typus);
    globales[id].est_bss = 1;
    globales[id].est_staticus = est_staticus;
    globales[id].valor_initialis = valor;
    globales[id].habet_valorem = (valor != 0);
    return id;
}

/* ================================================================
 * fixups
 * ================================================================ */

void fixup_adde(int genus, int offset, int target, int mag)
{
    if (num_fixups >= MAX_FIXUPS)
        erratum("nimis multi fixups");
    fixups[num_fixups].genus = genus;
    fixups[num_fixups].offset = offset;
    fixups[num_fixups].target = target;
    fixups[num_fixups].magnitudo_accessus = mag;
    num_fixups++;
}

/* ================================================================ */

void emitte_initia(void)
{
    if (!codex)
        codex        = malloc(MAX_CODEX);
    if (!chordae)
        chordae      = malloc(MAX_CHORDAE_LIT * sizeof(chorda_lit_t));
    if (!chordae_data)
        chordae_data = malloc(MAX_DATA);
    if (!got)
        got          = malloc(MAX_GOT * sizeof(got_intrans_t));
    if (!fixups)
        fixups       = malloc(MAX_FIXUPS * sizeof(fixup_t));
    if (!labels)
        labels       = malloc(MAX_LABELS * sizeof(int));
    if (!globales)
        globales     = malloc(MAX_GLOBALES * sizeof(globalis_t));
    if (!init_data)
        init_data    = malloc(MAX_DATA);
    codex_lon     = 0;
    data_lon      = 0;
    init_data_lon = 0;
    chordae_lon   = 0;
    num_chordarum = 0;
    num_got       = 0;
    num_fixups    = 0;
    num_labels    = 0;
    num_globalium = 0;
}

void emit32(uint32_t inst)
{
    if (codex_lon + 4 > MAX_CODEX)
        erratum("codex nimis longus");
    memcpy(&codex[codex_lon], &inst, 4);
    codex_lon += 4;
}

/* patch32 — reservata pro usu futuro */

/* ================================================================
 * instructiones ARM64 — helpers
 * ================================================================ */

/* MOV Xd, Xn (= ORR Xd, XZR, Xn) */
void emit_mov(int rd, int rn)
{
    emit32(0xAA0003E0 | (rn << 16) | rd);
}

/* MOV Wd, Wn */
/* MOVZ Xd, #imm16, LSL #shift */
void emit_movz(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0xD2800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}

/* MOVK Xd, #imm16, LSL #shift */
void emit_movk(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0xF2800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}

/* MOVN Xd, #imm16, LSL #shift */
void emit_movn(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0x92800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}

/* carrica immediatum arbitrarium in registrum */
void emit_movi(int rd, long imm)
{
    uint64_t val = (uint64_t)imm;
    if (val == 0) {
        emit_movz(rd, 0, 0);
        return;
    }
    if (imm > 0 && imm < 65536) {
        emit_movz(rd, (uint16_t)val, 0);
        return;
    }
    if (imm < 0 && imm >= -65536) {
        emit_movn(rd, (uint16_t)(~val), 0);
        return;
    }
    /* general: MOVZ + MOVK */
    int first = 1;
    for (int i = 0; i < 4; i++) {
        uint16_t chunk = (val >> (i * 16)) & 0xFFFF;
        if (chunk != 0 || (i == 0 && first)) {
            if (first) {
                emit_movz(rd, chunk, i * 16);
                first = 0;
            }else
                emit_movk(rd, chunk, i * 16);
        }
    }
    if (first)
        emit_movz(rd, 0, 0);
}

/* declarationes ante */
void emit_addi(int rd, int rn, int imm);
void emit_subi(int rd, int rn, int imm);

/* ADD Xd, Xn, Xm */
void emit_add(int rd, int rn, int rm)
{
    emit32(0x8B000000 | (rm << 16) | (rn << 5) | rd);
}

/* ADD Xd, Xn, #imm12 */
void emit_addi(int rd, int rn, int imm)
{
    if (imm < 0) {
        emit_subi(rd, rn, -imm);
        return;
    }
    if (imm > 4095) {
        emit_movi(17, imm);
        emit_add(rd, rn, 17);
        return;
    }
    emit32(0x91000000 | (imm << 10) | (rn << 5) | rd);
}

/* SUB Xd, Xn, Xm */
void emit_sub(int rd, int rn, int rm)
{
    emit32(0xCB000000 | (rm << 16) | (rn << 5) | rd);
}

/* SUB Xd, Xn, #imm12 */
void emit_subi(int rd, int rn, int imm)
{
    if (imm < 0) {
        emit_addi(rd, rn, -imm);
        return;
    }
    if (imm > 4095) {
        emit_movi(17, imm);
        emit_sub(rd, rn, 17);
        return;
    }
    emit32(0xD1000000 | (imm << 10) | (rn << 5) | rd);
}

/* MUL Xd, Xn, Xm */
void emit_mul(int rd, int rn, int rm)
{
    emit32(0x9B007C00 | (rm << 16) | (rn << 5) | rd);
}

/* SDIV Xd, Xn, Xm */
void emit_sdiv(int rd, int rn, int rm)
{
    emit32(0x9AC00C00 | (rm << 16) | (rn << 5) | rd);
}

/* UDIV Xd, Xn, Xm */
void emit_udiv(int rd, int rn, int rm)
{
    emit32(0x9AC00800 | (rm << 16) | (rn << 5) | rd);
}

/* AND Xd, Xn, Xm */
void emit_and(int rd, int rn, int rm)
{
    emit32(0x8A000000 | (rm << 16) | (rn << 5) | rd);
}

/* ORR Xd, Xn, Xm */
void emit_orr(int rd, int rn, int rm)
{
    emit32(0xAA000000 | (rm << 16) | (rn << 5) | rd);
}

/* EOR Xd, Xn, Xm */
void emit_eor(int rd, int rn, int rm)
{
    emit32(0xCA000000 | (rm << 16) | (rn << 5) | rd);
}

/* LSL Xd, Xn, Xm */
void emit_lsl(int rd, int rn, int rm)
{
    emit32(0x9AC02000 | (rm << 16) | (rn << 5) | rd);
}

/* LSR Xd, Xn, Xm */
void emit_lsr(int rd, int rn, int rm)
{
    emit32(0x9AC02400 | (rm << 16) | (rn << 5) | rd);
}

/* ASR Xd, Xn, Xm */
void emit_asr(int rd, int rn, int rm)
{
    emit32(0x9AC02800 | (rm << 16) | (rn << 5) | rd);
}

/* NEG Xd, Xm (= SUB Xd, XZR, Xm) */
void emit_neg(int rd, int rm)
{
    emit_sub(rd, XZR, rm);
}

/* MVN Xd, Xm (= ORN Xd, XZR, Xm) */
void emit_mvn(int rd, int rm)
{
    emit32(0xAA200000 | (rm << 16) | (XZR << 5) | rd);
}

/* CMP Xn, Xm (= SUBS XZR, Xn, Xm) */
void emit_cmp(int rn, int rm)
{
    emit32(0xEB00001F | (rm << 16) | (rn << 5));
}

/* CMP Xn, #imm */
void emit_cmpi(int rn, int imm)
{
    if (imm >= 0 && imm <= 4095) {
        emit32(0xF100001F | (imm << 10) | (rn << 5));
    } else {
        emit_movi(17, imm);
        emit_cmp(rn, 17);
    }
}

/* CSET Xd, cond (= CSINC Xd, XZR, XZR, !cond) */
void emit_cset(int rd, int cond)
{
    int cond_inv = cond ^ 1;
    emit32(0x9A9F07E0 | (cond_inv << 12) | rd);
}

/* B offset (in instructions) — placeholder, fixup resolves */
void emit_b_label(int label)
{
    fixup_adde(FIX_BRANCH, codex_lon, label, 0);
    emit32(0x14000000); /* placeholder */
}

/* BL label (vocationem directam) */
void emit_bl_label(int label)
{
    fixup_adde(FIX_BL, codex_lon, label, 0);
    emit32(0x94000000); /* placeholder */
}

/* B.cond label */
void emit_bcond_label(int cond, int label)
{
    fixup_adde(FIX_BCOND, codex_lon, label, cond);
    emit32(0x54000000 | cond); /* placeholder */
}

/* BLR Xn */
void emit_blr(int rn)
{
    emit32(0xD63F0000 | (rn << 5));
}

/* RET */
void emit_ret(void)
{
    emit32(0xD65F03C0);
}

/* CBZ Xt, label */
void emit_cbz_label(int rt, int label)
{
    fixup_adde(FIX_CBZ, codex_lon, label, 0);
    emit32(0xB4000000 | rt);
}

/* CBNZ Xt, label */
void emit_cbnz_label(int rt, int label)
{
    fixup_adde(FIX_CBNZ, codex_lon, label, 0);
    emit32(0xB5000000 | rt);
}

/* STP Xt1, Xt2, [Xn, #imm]! (pre-index) */
void emit_stp_pre(int rt1, int rt2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA9800000 | (imm7 << 15) | (rt2 << 10) | (rn << 5) | rt1);
}

/* LDP Xt1, Xt2, [Xn], #imm (post-index) */
void emit_ldp_post(int rt1, int rt2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA8C00000 | (imm7 << 15) | (rt2 << 10) | (rn << 5) | rt1);
}

/* LDR Xt, [Xn, #imm] (64-bit, unsigned offset) */
void emit_ldr64(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 32760 || (imm & 7)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0xF9400000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0xF9400000 | ((imm / 8) << 10) | (rn << 5) | rt);
}

/* STR Xt, [Xn, #imm] (64-bit, unsigned offset) */
void emit_str64(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 32760 || (imm & 7)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0xF9000000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0xF9000000 | ((imm / 8) << 10) | (rn << 5) | rt);
}

/* LDR Wt, [Xn, #imm] (32-bit) */
void emit_ldr32(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 16380 || (imm & 3)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0xB9400000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0xB9400000 | ((imm / 4) << 10) | (rn << 5) | rt);
}

/* STR Wt, [Xn, #imm] (32-bit) */
void emit_str32(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 16380 || (imm & 3)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0xB9000000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0xB9000000 | ((imm / 4) << 10) | (rn << 5) | rt);
}

/* LDRB Wt, [Xn, #imm] */
void emit_ldrb(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 4095) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0x39400000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0x39400000 | (imm << 10) | (rn << 5) | rt);
}

/* STRB Wt, [Xn, #imm] */
void emit_strb(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 4095) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0x39000000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0x39000000 | (imm << 10) | (rn << 5) | rt);
}

/* LDRH Wt, [Xn, #imm] */
void emit_ldrh(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 8190 || (imm & 1)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0x79400000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0x79400000 | ((imm / 2) << 10) | (rn << 5) | rt);
}

/* STRH Wt, [Xn, #imm] */
void emit_strh(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 8190 || (imm & 1)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0x79000000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0x79000000 | ((imm / 2) << 10) | (rn << 5) | rt);
}

/* LDRSB Xt, [Xn, #imm] (sign-extend byte to 64-bit) */
void emit_ldrsb(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 4095) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0x39800000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0x39800000 | (imm << 10) | (rn << 5) | rt);
}

/* LDRSH Xt, [Xn, #imm] */
void emit_ldrsh(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 8190 || (imm & 1)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0x79800000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0x79800000 | ((imm / 2) << 10) | (rn << 5) | rt);
}

/* LDRSW Xt, [Xn, #imm] */
void emit_ldrsw(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 16380 || (imm & 3)) {
        emit_movi(17, imm);
        emit_add(17, rn, 17);
        emit32(0xB9800000 | (0 << 10) | (17 << 5) | rt);
        return;
    }
    emit32(0xB9800000 | ((imm / 4) << 10) | (rn << 5) | rt);
}

/* SXTW Xd, Wn (= SBFM Xd, Xn, #0, #31) */
void emit_sxtw(int rd, int rn)
{
    emit32(0x93407C00 | (rn << 5) | rd);
}

/* SXTB Xd, Wn */
void emit_sxtb(int rd, int rn)
{
    emit32(0x93401C00 | (rn << 5) | rd);
}

/* SXTH Xd, Wn */
void emit_sxth(int rd, int rn)
{
    emit32(0x93403C00 | (rn << 5) | rd);
}

/* UXTB: AND Wd, Wn, #0xFF */
void emit_uxtb(int rd, int rn)
{
    /* UBFM Wd, Wn, #0, #7 = AND with 0xFF */
    emit32(0x53001C00 | (rn << 5) | rd);
}

/* UXTH: AND Wd, Wn, #0xFFFF */
void emit_uxth(int rd, int rn)
{
    emit32(0x53003C00 | (rn << 5) | rd);
}

/* ADRP Xd, #0 — placeholder, fixup resolves */
void emit_adrp_fixup(int rd, int fix_genus, int target)
{
    fixup_adde(fix_genus, codex_lon, target, 0);
    emit32(0x90000000 | rd); /* placeholder */
}

/* ================================================================
 * carrica/salva per magnitudinem
 * ================================================================ */

void emit_load(int rd, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: emit_ldrsb(rd, rn, offset); break;
    case 2: emit_ldrsh(rd, rn, offset); break;
    case 4: emit_ldrsw(rd, rn, offset); break;
    case 8: emit_ldr64(rd, rn, offset); break;
    default: emit_ldr64(rd, rn, offset); break;
    }
}

void emit_load_unsigned(int rd, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: emit_ldrb(rd, rn, offset); break;
    case 2: emit_ldrh(rd, rn, offset); break;
    case 4: emit_ldr32(rd, rn, offset); break;
    case 8: emit_ldr64(rd, rn, offset); break;
    default: emit_ldr64(rd, rn, offset); break;
    }
}

void emit_store(int rt, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: emit_strb(rt, rn, offset); break;
    case 2: emit_strh(rt, rn, offset); break;
    case 4: emit_str32(rt, rn, offset); break;
    case 8: emit_str64(rt, rn, offset); break;
    default: emit_str64(rt, rn, offset); break;
    }
}
