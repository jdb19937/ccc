/*
 * genera.c — CCC generans codicis ARM64 + scriptor Mach-O
 *
 * Generat instructiones ARM64 directe in alveum.
 * Scribit executabile Mach-O sine assemblatrice vel ligatore.
 * Post scripturam, codesign -s - vocat ad signandum.
 */

#include "ccc.h"

/* ================================================================
 * alvei et status
 * ================================================================ */

static uint8_t codex[MAX_CODEX];
static int codex_lon = 0;

/* data_sec reservata pro usu futuro */
static int data_lon = 0;

static chorda_lit_t chordae[MAX_CHORDAE_LIT];
static int num_chordarum = 0;
static uint8_t chordae_data[MAX_DATA];
static int chordae_lon = 0;

static got_intrans_t got[MAX_GOT];
static int num_got = 0;

static fixup_t fixups[MAX_FIXUPS];
static int num_fixups = 0;

static int labels[MAX_LABELS];
static int num_labels = 0;

static globalis_t globales[MAX_GLOBALES];
static int num_globalium = 0;

/* acervus break/continue labels */
static int break_labels[MAX_BREAK];
static int continue_labels[MAX_BREAK];
static int break_vertex = 0;

/* switch context */
static casus_t switch_casus[MAX_CASUS];
static int switch_num_casuum;
static int switch_default_label;
static int in_switch = 0;

static int label_novus(void);

/* tabula functionum localium (nomen -> label) */
typedef struct {
    char nomen[256];
    int label;
} func_loc_t;

static func_loc_t func_loci[MAX_GLOBALES];
static int num_func_loc = 0;

static int func_loc_quaere(const char *nomen)
{
    for (int i = 0; i < num_func_loc; i++)
        if (strcmp(func_loci[i].nomen, nomen) == 0)
            return func_loci[i].label;
    return -1;
}

static int func_loc_adde(const char *nomen)
{
    int lab = label_novus();
    strncpy(func_loci[num_func_loc].nomen, nomen, 255);
    func_loci[num_func_loc].label = lab;
    num_func_loc++;
    return lab;
}

/* goto labels intra functionem */
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
    int lab = label_novus();
    strncpy(goto_labels[num_goto_labels].nomen, nomen, 255);
    goto_labels[num_goto_labels].label = lab;
    num_goto_labels++;
    return lab;
}

/* status functionis currentis */
static int cur_frame_mag;     /* magnitudo frame */
static int cur_param_num;     /* numerus parametrorum */
static typus_t *cur_func_typus;
static int profunditas_vocationis = 0; /* profunditas vocationum nestarum */

/* registra temporaria: x0-x7, x9-x15 */
static int reg_vertex = 0;

/* ================================================================
 * registra
 * ================================================================ */

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
        return slot;      /* x0-x7 */
    return slot + 1;                 /* x9-x15 (praetermitte x8) */
}

/* ================================================================
 * labels
 * ================================================================ */

static int label_novus(void)
{
    if (num_labels >= MAX_LABELS)
        erratum("nimis multa labels");
    labels[num_labels] = -1;
    return num_labels++;
}

static void label_pone(int id)
{
    labels[id] = codex_lon;
}

/* ================================================================
 * chordae litterales
 * ================================================================ */

static int chorda_adde(const char *data, int lon)
{
    int id = num_chordarum;
    chordae[id].data = (char *)&chordae_data[chordae_lon];
    memcpy(&chordae_data[chordae_lon], data, lon);
    chordae_data[chordae_lon + lon] = '\0';
    chordae[id].longitudo = lon + 1;
    chordae[id].offset = chordae_lon;
    chordae_lon += lon + 1;
    /* allinea ad 1 — chordae non necessitant colineationem */
    num_chordarum++;
    return id;
}

/* ================================================================
 * GOT
 * ================================================================ */

static int got_adde(const char *nomen)
{
    /* quaere existentem */
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

static int globalis_adde(
    const char *nomen, typus_t *typus,
    int est_staticus, long valor
) {
    for (int i = 0; i < num_globalium; i++)
        if (strcmp(globales[i].nomen, nomen) == 0)
            return i;
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

static void fixup_adde(int genus, int offset, int target, int mag)
{
    if (num_fixups >= MAX_FIXUPS)
        erratum("nimis multi fixups");
    fixups[num_fixups].genus = genus;
    fixups[num_fixups].offset = offset;
    fixups[num_fixups].target = target;
    fixups[num_fixups].magnitudo_accessus = mag;
    num_fixups++;
}

/* ================================================================
 * emissio instructionum
 * ================================================================ */

static void emit32(uint32_t inst)
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
static void emit_mov(int rd, int rn)
{
    emit32(0xAA0003E0 | (rn << 16) | rd);
}

/* MOV Wd, Wn */
/* MOVZ Xd, #imm16, LSL #shift */
static void emit_movz(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0xD2800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}

/* MOVK Xd, #imm16, LSL #shift */
static void emit_movk(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0xF2800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}

/* MOVN Xd, #imm16, LSL #shift */
static void emit_movn(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0x92800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}

/* carrica immediatum arbitrarium in registrum */
static void emit_movi(int rd, long imm)
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
static void emit_addi(int rd, int rn, int imm);
static void emit_subi(int rd, int rn, int imm);

/* ADD Xd, Xn, Xm */
static void emit_add(int rd, int rn, int rm)
{
    emit32(0x8B000000 | (rm << 16) | (rn << 5) | rd);
}

/* ADD Xd, Xn, #imm12 */
static void emit_addi(int rd, int rn, int imm)
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
static void emit_sub(int rd, int rn, int rm)
{
    emit32(0xCB000000 | (rm << 16) | (rn << 5) | rd);
}

/* SUB Xd, Xn, #imm12 */
static void emit_subi(int rd, int rn, int imm)
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
static void emit_mul(int rd, int rn, int rm)
{
    emit32(0x9B007C00 | (rm << 16) | (rn << 5) | rd);
}

/* SDIV Xd, Xn, Xm */
static void emit_sdiv(int rd, int rn, int rm)
{
    emit32(0x9AC00C00 | (rm << 16) | (rn << 5) | rd);
}

/* UDIV Xd, Xn, Xm */
static void emit_udiv(int rd, int rn, int rm)
{
    emit32(0x9AC00800 | (rm << 16) | (rn << 5) | rd);
}

/* AND Xd, Xn, Xm */
static void emit_and(int rd, int rn, int rm)
{
    emit32(0x8A000000 | (rm << 16) | (rn << 5) | rd);
}

/* ORR Xd, Xn, Xm */
static void emit_orr(int rd, int rn, int rm)
{
    emit32(0xAA000000 | (rm << 16) | (rn << 5) | rd);
}

/* EOR Xd, Xn, Xm */
static void emit_eor(int rd, int rn, int rm)
{
    emit32(0xCA000000 | (rm << 16) | (rn << 5) | rd);
}

/* LSL Xd, Xn, Xm */
static void emit_lsl(int rd, int rn, int rm)
{
    emit32(0x9AC02000 | (rm << 16) | (rn << 5) | rd);
}

/* LSR Xd, Xn, Xm */
static void emit_lsr(int rd, int rn, int rm)
{
    emit32(0x9AC02400 | (rm << 16) | (rn << 5) | rd);
}

/* ASR Xd, Xn, Xm */
static void emit_asr(int rd, int rn, int rm)
{
    emit32(0x9AC02800 | (rm << 16) | (rn << 5) | rd);
}

/* NEG Xd, Xm (= SUB Xd, XZR, Xm) */
static void emit_neg(int rd, int rm)
{
    emit_sub(rd, XZR, rm);
}

/* MVN Xd, Xm (= ORN Xd, XZR, Xm) */
static void emit_mvn(int rd, int rm)
{
    emit32(0xAA200000 | (rm << 16) | (XZR << 5) | rd);
}

/* CMP Xn, Xm (= SUBS XZR, Xn, Xm) */
static void emit_cmp(int rn, int rm)
{
    emit32(0xEB00001F | (rm << 16) | (rn << 5));
}

/* CMP Xn, #imm */
static void emit_cmpi(int rn, int imm)
{
    if (imm >= 0 && imm <= 4095) {
        emit32(0xF100001F | (imm << 10) | (rn << 5));
    } else {
        emit_movi(17, imm);
        emit_cmp(rn, 17);
    }
}

/* CSET Xd, cond (= CSINC Xd, XZR, XZR, !cond) */
static void emit_cset(int rd, int cond)
{
    int cond_inv = cond ^ 1;
    emit32(0x9A9F07E0 | (cond_inv << 12) | rd);
}

/* B offset (in instructions) — placeholder, fixup resolves */
static void emit_b_label(int label)
{
    fixup_adde(FIX_BRANCH, codex_lon, label, 0);
    emit32(0x14000000); /* placeholder */
}

/* BL label (vocationem directam) */
static void emit_bl_label(int label)
{
    fixup_adde(FIX_BL, codex_lon, label, 0);
    emit32(0x94000000); /* placeholder */
}

/* B.cond label */
static void emit_bcond_label(int cond, int label)
{
    fixup_adde(FIX_BCOND, codex_lon, label, cond);
    emit32(0x54000000 | cond); /* placeholder */
}

/* BLR Xn */
static void emit_blr(int rn)
{
    emit32(0xD63F0000 | (rn << 5));
}

/* RET */
static void emit_ret(void)
{
    emit32(0xD65F03C0);
}

/* CBZ Xt, label */
static void emit_cbz_label(int rt, int label)
{
    fixup_adde(FIX_CBZ, codex_lon, label, 0);
    emit32(0xB4000000 | rt);
}

/* CBNZ Xt, label */
static void emit_cbnz_label(int rt, int label)
{
    fixup_adde(FIX_CBNZ, codex_lon, label, 0);
    emit32(0xB5000000 | rt);
}

/* STP Xt1, Xt2, [Xn, #imm]! (pre-index) */
static void emit_stp_pre(int rt1, int rt2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA9800000 | (imm7 << 15) | (rt2 << 10) | (rn << 5) | rt1);
}

/* LDP Xt1, Xt2, [Xn], #imm (post-index) */
static void emit_ldp_post(int rt1, int rt2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA8C00000 | (imm7 << 15) | (rt2 << 10) | (rn << 5) | rt1);
}

/* LDR Xt, [Xn, #imm] (64-bit, unsigned offset) */
static void emit_ldr64(int rt, int rn, int imm)
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
static void emit_str64(int rt, int rn, int imm)
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
static void emit_ldr32(int rt, int rn, int imm)
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
static void emit_str32(int rt, int rn, int imm)
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
static void emit_ldrb(int rt, int rn, int imm)
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
static void emit_strb(int rt, int rn, int imm)
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
static void emit_ldrh(int rt, int rn, int imm)
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
static void emit_strh(int rt, int rn, int imm)
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
static void emit_ldrsb(int rt, int rn, int imm)
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
static void emit_ldrsh(int rt, int rn, int imm)
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
static void emit_ldrsw(int rt, int rn, int imm)
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
static void emit_sxtw(int rd, int rn)
{
    emit32(0x93407C00 | (rn << 5) | rd);
}

/* SXTB Xd, Wn */
static void emit_sxtb(int rd, int rn)
{
    emit32(0x93401C00 | (rn << 5) | rd);
}

/* SXTH Xd, Wn */
static void emit_sxth(int rd, int rn)
{
    emit32(0x93403C00 | (rn << 5) | rd);
}

/* UXTB: AND Wd, Wn, #0xFF */
static void emit_uxtb(int rd, int rn)
{
    /* UBFM Wd, Wn, #0, #7 = AND with 0xFF */
    emit32(0x53001C00 | (rn << 5) | rd);
}

/* UXTH: AND Wd, Wn, #0xFFFF */
static void emit_uxth(int rd, int rn)
{
    emit32(0x53003C00 | (rn << 5) | rd);
}

/* ADRP Xd, #0 — placeholder, fixup resolves */
static void emit_adrp_fixup(int rd, int fix_genus, int target)
{
    fixup_adde(fix_genus, codex_lon, target, 0);
    emit32(0x90000000 | rd); /* placeholder */
}

/* ================================================================
 * carrica/salva per magnitudinem
 * ================================================================ */

static void emit_load(int rd, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: emit_ldrsb(rd, rn, offset); break;
    case 2: emit_ldrsh(rd, rn, offset); break;
    case 4: emit_ldrsw(rd, rn, offset); break;
    case 8: emit_ldr64(rd, rn, offset); break;
    default: emit_ldr64(rd, rn, offset); break;
    }
}

static void emit_load_unsigned(int rd, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: emit_ldrb(rd, rn, offset); break;
    case 2: emit_ldrh(rd, rn, offset); break;
    case 4: emit_ldr32(rd, rn, offset); break;
    case 8: emit_ldr64(rd, rn, offset); break;
    default: emit_ldr64(rd, rn, offset); break;
    }
}

static void emit_store(int rt, int rn, int offset, int mag)
{
    switch (mag) {
    case 1: emit_strb(rt, rn, offset); break;
    case 2: emit_strh(rt, rn, offset); break;
    case 4: emit_str32(rt, rn, offset); break;
    case 8: emit_str64(rt, rn, offset); break;
    default: emit_str64(rt, rn, offset); break;
    }
}

/* ================================================================
 * generatio expressionum
 * ================================================================ */

static void genera_expr(nodus_t *n, int dest);
static void genera_lval(nodus_t *n, int dest);
static void genera_sententia(nodus_t *n);

static int mag_typi(typus_t *t)
{
    if (!t)
        return 8;
    if (t->genus == TY_PTR || t->genus == TY_ARRAY || t->genus == TY_FUNC)
        return 8;
    if (t->genus == TY_STRUCT)
        return t->magnitudo;
    return t->magnitudo ? t->magnitudo : 4;
}

/* magnitudo vera pro accessu tabulae — non reducit TY_ARRAY ad 8 */
static int mag_typi_verus(typus_t *t)
{
    if (!t)
        return 8;
    if (t->genus == TY_ARRAY)
        return t->magnitudo > 0 ? t->magnitudo : 8;
    return mag_typi(t);
}

static int est_unsigned(typus_t *t)
{
    if (!t)
        return 0;
    return t->est_sine_signo ||
        t->genus == TY_UCHAR || t->genus == TY_USHORT ||
        t->genus == TY_UINT || t->genus == TY_ULONG ||
        t->genus == TY_ULLONG ||
        t->genus == TY_PTR || t->genus == TY_ARRAY;
}

/* carrica valorem ex l-valor adresse in dest */
static void emit_load_from_addr(int dest, typus_t *t)
{
    int mag = mag_typi(t);
    if (t && (t->genus == TY_STRUCT || t->genus == TY_ARRAY))
        return; /* iam adresse */
    if (est_unsigned(t))
        emit_load_unsigned(dest, dest, 0, mag);
    else
        emit_load(dest, dest, 0, mag);
}

/* genera l-valor (adresse in dest) */
static void genera_lval(nodus_t *n, int dest)
{
    if (dest >= reg_vertex)
        reg_vertex = dest + 1;
    int r = reg_arm(dest);

    switch (n->genus) {
    case N_IDENT: {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            if (!s)
                erratum_ad(n->linea, "symbolum ignotum: %s", n->nomen);
            if (s->est_globalis && s->est_externus) {
                /* variabilis externa — adresse per GOT */
                char got_nomen[260];
                snprintf(got_nomen, 260, "_%s", s->nomen);
                int gid = got_adde(got_nomen);
                emit_adrp_fixup(r, FIX_ADRP_GOT, gid);
                fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                emit_ldr64(r, r, 0);
            } else if (s->est_globalis) {
                int gid = s->globalis_index;
                if (gid < 0)
                    gid = globalis_adde(s->nomen, s->typus, s->est_staticus, 0);
                s->globalis_index = gid;
                emit_adrp_fixup(r, FIX_ADRP_DATA, gid);
                fixup_adde(FIX_ADD_LO12_DATA, codex_lon, gid, 0);
                emit32(0x91000000 | (r << 5) | r); /* ADD Xr, Xr, #lo12 — placeholder */
            } else {
            /* localis — offset a FP */
                int off = s->offset;
                if (off < 0) {
                    emit_movi(r, -off);
                    emit_sub(r, FP, r);
                } else {
                    emit_addi(r, FP, off);
                }
            }
            break;
        }
    case N_DEREF:
        genera_expr(n->sinister, dest);
        break;
    case N_INDEX: {
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            int basis_mag = 1;
            if (n->sinister->typus)
                basis_mag = mag_typi_verus(typus_basis_indicis(n->sinister->typus));
            if (basis_mag > 1) {
                emit_movi(17, basis_mag);
                emit_mul(reg_arm(r2), reg_arm(r2), 17);
            }
            emit_add(r, r, reg_arm(r2));
            reg_libera(r2);
            break;
        }
    case N_MEMBER: {
            genera_lval(n->sinister, dest);
            if (n->sinister->typus && n->sinister->typus->genus == TY_STRUCT) {
                typus_t *st = n->sinister->typus;
                for (int i = 0; i < st->num_membrorum; i++) {
                    if (strcmp(st->membra[i].nomen, n->nomen) == 0) {
                        if (st->membra[i].offset > 0)
                            emit_addi(r, r, st->membra[i].offset);
                        break;
                    }
                }
            }
            break;
        }
    case N_ARROW: {
            genera_expr(n->sinister, dest);
            if (
                n->sinister->typus && n->sinister->typus->genus == TY_PTR &&
                n->sinister->typus->basis && n->sinister->typus->basis->genus == TY_STRUCT
            ) {
                typus_t *st = n->sinister->typus->basis;
                for (int i = 0; i < st->num_membrorum; i++) {
                    if (strcmp(st->membra[i].nomen, n->nomen) == 0) {
                        if (st->membra[i].offset > 0)
                            emit_addi(r, r, st->membra[i].offset);
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
    if (!n) {
        emit_movi(reg_arm(dest), 0);
        return;
    }
    /* cura ut reg_vertex superet dest, ne allocator eum redat */
    if (dest >= reg_vertex)
        reg_vertex = dest + 1;
    int r = reg_arm(dest);

    switch (n->genus) {
    case N_NUM:
        emit_movi(r, n->valor);
        break;

    case N_STR: {
            int sid = chorda_adde(n->chorda, n->lon_chordae);
            emit_adrp_fixup(r, FIX_ADRP, sid);
            fixup_adde(FIX_ADD_LO12, codex_lon, sid, 0);
            emit32(0x91000000 | (r << 5) | r); /* ADD placeholder */
            break;
        }

    case N_IDENT: {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            if (!s) {
            /* functio non declarata — pone GOT intransum */
                char got_nomen[260];
                snprintf(got_nomen, 260, "_%s", n->nomen);
                int gid = got_adde(got_nomen);
                emit_adrp_fixup(r, FIX_ADRP_GOT, gid);
                fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                emit_ldr64(r, r, 0); /* placeholder — fixup patches offset */
                break;
            }
            if (s->genus == SYM_FUNC) {
                /* proba si functio est localis */
                int flabel = func_loc_quaere(s->nomen);
                if (flabel >= 0) {
                    /* adresse functionis localis per ADR */
                    fixup_adde(FIX_ADR_LABEL, codex_lon, flabel, 0);
                    emit32(0x10000000 | r); /* ADR Xr, label — placeholder */
                } else {
                    /* functio externa — per GOT */
                    char got_nomen[260];
                    snprintf(got_nomen, 260, "_%s", s->nomen);
                    int gid      = got_adde(got_nomen);
                    s->got_index = gid;
                    emit_adrp_fixup(r, FIX_ADRP_GOT, gid);
                    fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                    emit_ldr64(r, r, 0);
                }
                break;
            }
            genera_lval(n, dest);
            if (s->typus && (s->typus->genus == TY_ARRAY)) {
            /* tabula → adresse iam in r */
            } else if (s->typus && s->typus->genus == TY_STRUCT) {
            /* structura → adresse */
            } else {
                emit_load_from_addr(r, s->typus);
            }
            break;
        }

    case N_BINOP: {
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            int ra = r, rb = reg_arm(r2);

            switch (n->op) {
            case T_PLUS:
                if (typus_est_index(n->sinister->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_mul(rb, rb, 17);
                    }
                }
                emit_add(ra, ra, rb);
                break;
            case T_MINUS:
                if (typus_est_index(n->sinister->typus) && typus_est_integer(n->dexter->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_mul(rb, rb, 17);
                    }
                }
                emit_sub(ra, ra, rb);
                /* si ambo indices, divide per magnitudinem basis */
                if (typus_est_index(n->sinister->typus) && typus_est_index(n->dexter->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_sdiv(ra, ra, 17);
                    }
                }
                break;
            case T_STAR:    emit_mul(ra, ra, rb); break;
            case T_SLASH:
                if (est_unsigned(n->typus))
                    emit_udiv(ra, ra, rb);
                else
                    emit_sdiv(ra, ra, rb);
                break;
            case T_PERCENT:
                if (est_unsigned(n->typus))
                    emit_udiv(17, ra, rb);
                else
                    emit_sdiv(17, ra, rb);
                emit_mul(17, 17, rb);
                emit_sub(ra, ra, 17);
                break;
            case T_AMP:     emit_and(ra, ra, rb); break;
            case T_PIPE:    emit_orr(ra, ra, rb); break;
            case T_CARET:   emit_eor(ra, ra, rb); break;
            case T_LTLT:    emit_lsl(ra, ra, rb); break;
            case T_GTGT:
                if (est_unsigned(n->sinister->typus))
                    emit_lsr(ra, ra, rb);
                else
                    emit_asr(ra, ra, rb);
                break;
            case T_EQEQ:   emit_cmp(ra, rb);
                emit_cset(ra, COND_EQ);
                break;
            case T_BANGEQ:  emit_cmp(ra, rb);
                emit_cset(ra, COND_NE);
                break;
            case T_LT:
                emit_cmp(ra, rb);
                emit_cset(ra, est_unsigned(n->sinister->typus) ? COND_LO : COND_LT);
                break;
            case T_GT:
                emit_cmp(ra, rb);
                emit_cset(ra, est_unsigned(n->sinister->typus) ? COND_HI : COND_GT);
                break;
            case T_LTEQ:
                emit_cmp(ra, rb);
                emit_cset(ra, est_unsigned(n->sinister->typus) ? COND_LS : COND_LE);
                break;
            case T_GTEQ:
                emit_cmp(ra, rb);
                emit_cset(ra, est_unsigned(n->sinister->typus) ? COND_HS : COND_GE);
                break;
            case T_AMPAMP:
                {
                    int l_false = label_novus();
                    int l_end   = label_novus();
                    emit_cbz_label(ra, l_false);
                    emit_cbz_label(rb, l_false);
                    emit_movi(ra, 1);
                    emit_b_label(l_end);
                    label_pone(l_false);
                    emit_movi(ra, 0);
                    label_pone(l_end);
                    break;
                }
            case T_PIPEPIPE:
                {
                    int l_true = label_novus();
                    int l_end  = label_novus();
                    emit_cbnz_label(ra, l_true);
                    emit_cbnz_label(rb, l_true);
                    emit_movi(ra, 0);
                    emit_b_label(l_end);
                    label_pone(l_true);
                    emit_movi(ra, 1);
                    label_pone(l_end);
                    break;
                }
            }
            reg_libera(r2);
            break;
        }

    case N_UNOP: {
            genera_expr(n->sinister, dest);
            switch (n->op) {
            case T_MINUS: emit_neg(r, r); break;
            case T_TILDE: emit_mvn(r, r); break;
            case T_BANG:
                emit_cmpi(r, 0);
                emit_cset(r, COND_EQ);
                break;
            case T_PLUSPLUS:
                /* pre-increment */
                {
                    int r2 = reg_alloca();
                    genera_lval(n->sinister, r2);
                    int mag = mag_typi(n->typus);
                    int inc = 1;
                    if (typus_est_index(n->typus))
                        inc = mag_typi(typus_basis_indicis(n->typus));
                    emit_load_from_addr(r, n->typus);
                    emit_addi(r, r, inc);
                    emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
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
                    emit_load_from_addr(r, n->typus);
                    emit_subi(r, r, dec);
                    emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
                    reg_libera(r2);
                    break;
                }
            }
            break;
        }

    case N_POSTOP: {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            int mag = mag_typi(n->typus);
            if (est_unsigned(n->typus))
                emit_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            else
                emit_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            int inc = 1;
            if (typus_est_index(n->typus))
                inc = mag_typi(typus_basis_indicis(n->typus));
            if (n->op == T_PLUSPLUS)
                emit_addi(17, r, inc);
            else
                emit_subi(17, r, inc);
            emit_store(17, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            reg_libera(r2);
            break;
        }

    case N_ASSIGN: {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            genera_expr(n->dexter, dest);
            int mag = mag_typi(n->sinister->typus);
            if (n->sinister->typus && n->sinister->typus->genus == TY_STRUCT) {
                /* copia structurae */
                for (int i = 0; i < mag; i += 8) {
                    int rem = mag - i;
                    if (rem >= 8) {
                        emit_ldr64(17, r, i);
                        emit_str64(17, reg_arm(r2), i);
                    }else if (rem >= 4) {
                        emit_ldr32(17, r, i);
                        emit_str32(17, reg_arm(r2), i);
                    }else {
                        emit_ldrb(17, r, i);
                        emit_strb(17, reg_arm(r2), i);
                    }
                }
            } else {
                emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            }
            reg_libera(r2);
            break;
        }

    case N_OPASSIGN: {
            int r2 = reg_alloca();
            genera_lval(n->sinister, r2);
            int mag = mag_typi(n->sinister->typus);
            /* carrica valorem currentem */
            if (est_unsigned(n->sinister->typus))
                emit_load_unsigned(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            else
                emit_load(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            int r3 = reg_alloca();
            genera_expr(n->dexter, r3);
            int rb = reg_arm(r3);
            switch (n->op) {
            case T_PLUSEQ:
                if (typus_est_index(n->sinister->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_mul(rb, rb, 17);
                    }
                }
                emit_add(r, r, rb);
                break;
            case T_MINUSEQ:
                if (typus_est_index(n->sinister->typus)) {
                    int bm = mag_typi(typus_basis_indicis(n->sinister->typus));
                    if (bm > 1) {
                        emit_movi(17, bm);
                        emit_mul(rb, rb, 17);
                    }
                }
                emit_sub(r, r, rb);
                break;
            case T_STAREQ:    emit_mul(r, r, rb); break;
            case T_SLASHEQ:   emit_sdiv(r, r, rb); break;
            case T_PERCENTEQ: emit_sdiv(17, r, rb);
                emit_mul(17, 17, rb);
                emit_sub(r, r, 17);
                break;
            case T_AMPEQ:     emit_and(r, r, rb); break;
            case T_PIPEEQ:    emit_orr(r, r, rb); break;
            case T_CARETEQ:   emit_eor(r, r, rb); break;
            case T_LTLTEQ:    emit_lsl(r, r, rb); break;
            case T_GTGTEQ:    emit_asr(r, r, rb); break;
            }
            reg_libera(r3);
            emit_store(r, reg_arm(r2), 0, mag > 8 ? 8 : mag);
            reg_libera(r2);
            break;
        }

    case N_TERNARY: {
            int l_false = label_novus(), l_end = label_novus();
            genera_expr(n->sinister, dest);
            emit_cbz_label(r, l_false);
            genera_expr(n->dexter, dest);
            emit_b_label(l_end);
            label_pone(l_false);
            genera_expr(n->tertius, dest);
            label_pone(l_end);
            break;
        }

    case N_COMMA_EXPR:
        genera_expr(n->sinister, dest);
        genera_expr(n->dexter, dest);
        break;

    case N_CALL:
        /* salva registra viva */
        {
            int salvati = reg_vertex;
            for (int i = 0; i < salvati; i++)
                emit_str64(reg_arm(i), FP, -(cur_frame_mag - 16 - i * 8));

            /* evaluare argumenta et salvare in acervo temporario */
            int nargs = n->num_membrorum;
            int arg_spill_base = cur_frame_mag - 16 - 15 * 8
                + profunditas_vocationis * 8 * 8;
            profunditas_vocationis++;
            for (int i = 0; i < nargs; i++) {
                reg_vertex = 0;
                genera_expr(n->membra[i], 0);
                emit_str64(0, FP, -(arg_spill_base + i * 8));
            }

            /* ABI Apple ARM64: argumenta variadica in acervo, non in registris.
             * determinare quot parametri nominati sint. */

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

            /* nominati in registris x0-x7 */
            int regs_usati = num_nominati < 8 ? num_nominati : 8;
            for (int i = 0; i < regs_usati; i++)
                emit_ldr64(i, FP, -(arg_spill_base + i * 8));

            /* si variadica: reliqua argumenta in acervo */
            int acervus_args = 0;
            if (est_variadica && nargs > num_nominati) {
                acervus_args    = nargs - num_nominati;
                int acervus_mag = ((acervus_args * 8) + 15) & ~15;
                emit_subi(SP, SP, acervus_mag);
                for (int i = 0; i < acervus_args; i++) {
                    emit_ldr64(17, FP, -(arg_spill_base + (num_nominati + i) * 8));
                    emit_str64(17, SP, i * 8);
                }
            } else if (!est_variadica) {
                /* non variadica: omnia in registris */
                for (int i = regs_usati; i < nargs && i < 8; i++)
                    emit_ldr64(i, FP, -(arg_spill_base + i * 8));
                /* superflua in acervo */
                if (nargs > 8) {
                    int extra     = nargs - 8;
                    int extra_mag = ((extra * 8) + 15) & ~15;
                    emit_subi(SP, SP, extra_mag);
                    for (int i = 8; i < nargs; i++) {
                        emit_ldr64(17, FP, -(arg_spill_base + i * 8));
                        emit_str64(17, SP, (i - 8) * 8);
                    }
                    acervus_args = extra;
                }
            }

        /* voca functionem */
            reg_vertex = 0;
            if (n->sinister->genus == N_IDENT) {
                /* proba si functio est localis */
                int flabel = func_loc_quaere(n->sinister->nomen);
                if (flabel >= 0) {
                    /* vocationem directam per BL */
                    emit_bl_label(flabel);
                } else {
                    /* functio externa — per GOT */
                    symbolum_t *s = n->sinister->sym ? n->sinister->sym : ambitus_quaere_omnes(n->sinister->nomen);
                    char got_nomen[260];
                    snprintf(got_nomen, 260, "_%s", n->sinister->nomen);
                    int gid = got_adde(got_nomen);
                    if (s)
                        s->got_index = gid;
                    emit_adrp_fixup(16, FIX_ADRP_GOT, gid);
                    fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                    emit_ldr64(16, 16, 0);
                    emit_blr(16);
                }
            } else {
                genera_expr(n->sinister, 0);
                emit_mov(16, 0);
                emit_blr(16);
            }

            profunditas_vocationis--;

            if (acervus_args > 0) {
                int acervus_mag = ((acervus_args * 8) + 15) & ~15;
                emit_addi(SP, SP, acervus_mag);
            }

            /* resultatum in x0 — move ad dest primo */
            if (r != 0)
                emit_mov(r, 0);

            /* restitue registra — praetermitte dest ne resultatum deleatur */
            reg_vertex = salvati;
            for (int i = 0; i < salvati; i++) {
                if (i == dest)
                    continue; /* hic iam habet resultatum */
                emit_ldr64(reg_arm(i), FP, -(cur_frame_mag - 16 - i * 8));
            }
            break;
        }

    case N_INDEX:
        {
            genera_expr(n->sinister, dest);
            int r2 = reg_alloca();
            genera_expr(n->dexter, r2);
            int basis_mag = mag_typi_verus(n->typus);
            if (basis_mag > 1) {
                emit_movi(17, basis_mag);
                emit_mul(reg_arm(r2), reg_arm(r2), 17);
            }
            emit_add(r, r, reg_arm(r2));
            reg_libera(r2);
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
                emit_load_from_addr(r, n->typus);
            break;
        }

    case N_MEMBER:
        {
            genera_lval(n, dest);
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
                emit_load_from_addr(r, n->typus);
            break;
        }

    case N_ARROW:
        {
            genera_lval(n, dest);
            if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
                emit_load_from_addr(r, n->typus);
            break;
        }

    case N_DEREF:
        genera_expr(n->sinister, dest);
        if (n->typus && n->typus->genus != TY_ARRAY && n->typus->genus != TY_STRUCT)
            emit_load_from_addr(r, n->typus);
        break;

    case N_ADDR:
        genera_lval(n->sinister, dest);
        break;

    case N_CAST:
        genera_expr(n->sinister, dest);
        /* truncatio vel extensio */
        if (n->typus_decl) {
            int tm = mag_typi(n->typus_decl);
            int sm = n->sinister->typus ? mag_typi(n->sinister->typus) : 8;
            if (tm < sm) {
                switch (tm) {
                case 1: if (est_unsigned(n->typus_decl))
                        emit_uxtb(r, r);
                    else
                        emit_sxtb(r, r);
                    break;
                case 2: if (est_unsigned(n->typus_decl))
                        emit_uxth(r, r);
                    else
                        emit_sxth(r, r);
                    break;
                case 4: if (!est_unsigned(n->typus_decl))
                        emit_sxtw(r, r);
                    break;
                }
            } else if (tm > sm && sm == 4 && !est_unsigned(n->sinister->typus)) {
                emit_sxtw(r, r);
            }
        }
        break;

    case N_SIZEOF_TYPE:
        emit_movi(r, n->typus_decl ? typus_magnitudo(n->typus_decl) : 0);
        break;

    case N_SIZEOF_EXPR:
        emit_movi(
            r, n->sinister && n->sinister->typus ?
            typus_magnitudo(n->sinister->typus) : 0
        );
        break;

    case N_NOP:
        break;

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
    case N_NOP: break;

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
            if (s && (s->est_globalis || s->est_staticus)) {
            /* globalis/statica */
                long val = 0;
                if (n->sinister && n->sinister->genus == N_NUM)
                    val = n->sinister->valor;
                int gid = globalis_adde(n->nomen, n->typus_decl, n->est_staticus, val);
                s->globalis_index = gid;
            } else if (n->num_membrorum > 0 && n->membra) {
            /* initiale tabulae { expr, expr, ... } */
                if (s) {
                    int off_basis = s->offset;
                    typus_t *elem_typus = (s->typus && s->typus->genus == TY_ARRAY) ?
                        s->typus->basis : ty_int;
                    int elem_mag = typus_magnitudo(elem_typus);
                    if (elem_mag < 1)
                        elem_mag = 4;
                    /* primo imple totam tabulam cum zeris */
                    int tot_mag = (s->typus && s->typus->genus == TY_ARRAY) ?
                        s->typus->magnitudo : mag_typi(s->typus);
                    if (tot_mag > 0) {
                        emit_movi(0, 0);
                        for (int z = 0; z < tot_mag; z += 8) {
                            int zo = off_basis + z;
                            emit_movi(17, -zo);
                            emit_sub(17, FP, 17);
                            emit_str64(0, 17, 0);
                        }
                    }
                    /* scribe singula elementa */
                    for (int i = 0; i < n->num_membrorum; i++) {
                        reg_vertex = 0;
                        genera_expr(n->membra[i], 0);
                        int elem_off = off_basis + i * elem_mag;
                        emit_movi(17, -elem_off);
                        emit_sub(17, FP, 17);
                        emit_store(0, 17, 0, elem_mag);
                        reg_vertex = 0;
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
            /* compound literal — scribe membra singulariter */
                {
                    int off     = s->offset;
                    typus_t *st = n->sinister->typus;
                    int tot_mag = st->magnitudo > 0 ? st->magnitudo : mag_typi(st);
                    /* primo imple cum zeris */
                    emit_movi(0, 0);
                    for (int z = 0; z < tot_mag; z += 8) {
                        emit_movi(17, -(off + z));
                        emit_sub(17, FP, 17);
                        emit_str64(0, 17, 0);
                    }
                    /* scribe singula elementa */
                    if (st->genus == TY_STRUCT && st->membra) {
                        for (int i = 0; i < n->sinister->num_membrorum && i < st->num_membrorum; i++) {
                            reg_vertex = 0;
                            genera_expr(n->sinister->membra[i], 0);
                            int moff = off + st->membra[i].offset;
                            int mmag = mag_typi(st->membra[i].typus);
                            emit_movi(17, -moff);
                            emit_sub(17, FP, 17);
                            emit_store(0, 17, 0, mmag);
                            reg_vertex = 0;
                        }
                    } else {
                        /* tabula */
                        typus_t *elem_t = st->basis ? st->basis : ty_int;
                        int emag        = typus_magnitudo(elem_t);
                        if (emag < 1)
                            emag = 4;
                        for (int i = 0; i < n->sinister->num_membrorum; i++) {
                            reg_vertex = 0;
                            genera_expr(n->sinister->membra[i], 0);
                            emit_movi(17, -(off + i * emag));
                            emit_sub(17, FP, 17);
                            emit_store(0, 17, 0, emag);
                            reg_vertex = 0;
                        }
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
            /* initiale tabulae char cum chorda litterali */
                {
                    int off     = s->offset;
                    int arr_mag = s->typus->magnitudo;
                    /* primo imple cum zeris */
                    emit_movi(0, 0);
                    for (int z = 0; z < arr_mag; z += 8) {
                        emit_movi(17, -(off + z));
                        emit_sub(17, FP, 17);
                        emit_str64(0, 17, 0);
                    }
                    /* deinde copia characteres chordae */
                    const char *str = n->sinister->chorda;
                    int slen        = n->sinister->lon_chordae;
                    for (int i = 0; i <= slen && i < arr_mag; i++) {
                        emit_movi(0, (unsigned char)str[i]);
                        emit_movi(17, -(off + i));
                        emit_sub(17, FP, 17);
                        emit_strb(0, 17, 0);
                    }
                }
                reg_vertex = 0;
            } else if (n->sinister) {
            /* initiale scalaris */
                reg_vertex = 0;
                genera_expr(n->sinister, 0);
                if (s) {
                    int mag = mag_typi(s->typus);
                    int off = s->offset;
                    if (s->typus && s->typus->genus == TY_STRUCT) {
                    /* copia structurae */
                        emit_movi(17, -off);
                        emit_sub(17, FP, 17);
                        for (int i = 0; i < mag; i += 8) {
                            int rem = mag - i;
                            if (rem >= 8) {
                                emit_ldr64(16, 0, i);
                                emit_str64(16, 17, i);
                            }else if (rem >= 4) {
                                emit_ldr32(16, 0, i);
                                emit_str32(16, 17, i);
                            }else {
                                emit_ldrb(16, 0, i);
                                emit_strb(16, 17, i);
                            }
                        }
                    } else {
                        emit_movi(17, -off);
                        emit_sub(17, FP, 17);
                        emit_store(0, 17, 0, mag > 8 ? 8 : mag);
                    }
                }
                reg_vertex = 0;
            }
            break;
        }

    case N_IF:
        {
            int l_else = label_novus(), l_end = label_novus();
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            emit_cbz_label(0, n->tertius ? l_else : l_end);
            reg_vertex = 0;
            genera_sententia(n->dexter);
            if (n->tertius) {
                emit_b_label(l_end);
                label_pone(l_else);
                genera_sententia(n->tertius);
            }
            label_pone(l_end);
            break;
        }

    case N_WHILE:
        {
            int l_cond = label_novus(), l_end = label_novus();
            int l_cont = l_cond;
            break_labels[break_vertex] = l_end;
            continue_labels[break_vertex] = l_cont;
            break_vertex++;
            label_pone(l_cond);
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            emit_cbz_label(0, l_end);
            reg_vertex = 0;
            genera_sententia(n->dexter);
            emit_b_label(l_cond);
            label_pone(l_end);
            break_vertex--;
            break;
        }

    case N_DOWHILE:
        {
            int l_top = label_novus(), l_cond = label_novus(), l_end = label_novus();
            break_labels[break_vertex] = l_end;
            continue_labels[break_vertex] = l_cond;
            break_vertex++;
            label_pone(l_top);
            genera_sententia(n->dexter);
            label_pone(l_cond);
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            emit_cbnz_label(0, l_top);
            label_pone(l_end);
            break_vertex--;
            break;
        }

    case N_FOR:
        {
            int l_cond = label_novus(), l_inc = label_novus(), l_end = label_novus();
            break_labels[break_vertex] = l_end;
            continue_labels[break_vertex] = l_inc;
            break_vertex++;
            if (n->sinister) {
                reg_vertex = 0;
                genera_sententia(n->sinister);
            }
            label_pone(l_cond);
            if (n->dexter) {
                reg_vertex = 0;
                genera_expr(n->dexter, 0);
                emit_cbz_label(0, l_end);
            }
            reg_vertex = 0;
            genera_sententia(n->quartus);
            label_pone(l_inc);
            if (n->tertius) {
                reg_vertex = 0;
                genera_expr(n->tertius, 0);
            }
            emit_b_label(l_cond);
            label_pone(l_end);
            break_vertex--;
            reg_vertex = 0;
            break;
        }

    case N_SWITCH:
        /* collige casus ex corpore */
        {
            int l_end         = label_novus();
            int old_in_switch = in_switch;
            casus_t old_casus[MAX_CASUS];
            int old_num     = switch_num_casuum;
            int old_default = switch_default_label;
            memcpy(old_casus, switch_casus, sizeof(old_casus));

            switch_num_casuum = 0;
            switch_default_label = -1;
            in_switch = 1;

            break_labels[break_vertex] = l_end;
            continue_labels[break_vertex] = break_vertex > 0 ?
                continue_labels[break_vertex - 1] : l_end;
            break_vertex++;

            /* evaluare expressionem switch */
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
            /* salva in acervo */
            emit_str64(0, FP, -(cur_frame_mag - 16 - 15 * 8 - 8));

            /* pre-scan corpus pro casus et default labels */
            /* genera corpus — casus labels ponentur suis locis */
            int l_dispatch = label_novus();
            emit_b_label(l_dispatch); /* salta ad dispatch */

            /* genera corpus */
            reg_vertex = 0;
            genera_sententia(n->dexter);
            emit_b_label(l_end);

            /* dispatch tabula */
            label_pone(l_dispatch);
            emit_ldr64(0, FP, -(cur_frame_mag - 16 - 15 * 8 - 8));
            for (int i = 0; i < switch_num_casuum; i++) {
                emit_movi(17, switch_casus[i].valor);
                emit_cmp(0, 17);
                emit_bcond_label(COND_EQ, switch_casus[i].label);
            }
            if (switch_default_label >= 0)
                emit_b_label(switch_default_label);
            else
                emit_b_label(l_end);

            label_pone(l_end);
            break_vertex--;

            in_switch = old_in_switch;
            memcpy(switch_casus, old_casus, sizeof(old_casus));
            switch_num_casuum    = old_num;
            switch_default_label = old_default;
            break;
        }

    case N_CASE:
        {
            int l = label_novus();
            if (switch_num_casuum < MAX_CASUS) {
                switch_casus[switch_num_casuum].valor = n->valor;
                switch_casus[switch_num_casuum].label = l;
                switch_num_casuum++;
            }
            label_pone(l);
            if (n->dexter)
                genera_sententia(n->dexter);
            break;
        }

    case N_DEFAULT:
        {
            int l = label_novus();
            switch_default_label = l;
            label_pone(l);
            if (n->dexter)
                genera_sententia(n->dexter);
            break;
        }

    case N_RETURN:
        if (n->sinister) {
            reg_vertex = 0;
            genera_expr(n->sinister, 0);
        }
        /* epilogus */
        emit_addi(SP, FP, 0);
        emit_ldp_post(FP, LR, SP, 16);
        emit_ret();
        break;

    case N_BREAK:
        if (break_vertex > 0)
            emit_b_label(break_labels[break_vertex - 1]);
        break;

    case N_CONTINUE:
        if (break_vertex > 0)
            emit_b_label(continue_labels[break_vertex - 1]);
        break;

    case N_GOTO: {
            int lab = goto_label_quaere_vel_crea(n->nomen);
            emit_b_label(lab);
        }
        break;

    case N_LABEL: {
            int lab = goto_label_quaere_vel_crea(n->nomen);
            label_pone(lab);
            if (n->sinister)
                genera_sententia(n->sinister);
        }
        break;

    case N_FUNC_DEF:
        /* tractatur in genera_translatio */
        break;

    default:
        /* try as expression */
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
    /* computare magnitudinem frame */
    int nparams    = (int)n->sinister->valor;
    int locals_mag = 0;

    /* scan corpus pro variabilibus localibus */
    /* usamus offset iam computatos a parsore */
    /* invenire minimum offset (maximus negativus) */
    /* simpliciter: frame = parametri + 128 spill + 256 extra */
    /* n->op continet profunditatem acervi maximam a parsore */
    int locals_depth = n->op > 0 ? n->op : 256;
    cur_frame_mag    = 16 + nparams * 8 + 15 * 8 + 16 + locals_depth + 512;
    cur_frame_mag    = (cur_frame_mag + 15) & ~15;
    cur_param_num    = nparams;
    cur_func_typus   = n->typus;
    (void)locals_mag;

    /* label pro functione (iam allocatum in genera_translatio) */
    int func_label = func_loc_quaere(n->nomen);
    if (func_label < 0)
        func_label = label_novus();
    label_pone(func_label);

    /* prologus — ADD/SUB imm tractant reg 31 ut SP, non XZR */
    emit_stp_pre(FP, LR, SP, -16);
    emit_addi(FP, SP, 0);            /* MOV x29, sp */
    /* SUB sp, x29, #frame: si frame > 4095, per registrum x16 */
    {
        int frame = cur_frame_mag - 16;
        if (frame <= 4095) {
            emit_subi(SP, FP, frame);
        } else {
            emit_movi(16, frame);
            emit_sub(16, FP, 16);    /* x16 = x29 - frame */
            emit_addi(SP, 16, 0);    /* sp = x16 */
        }
    }

    /* salva parametros in acervo */
    for (int i = 0; i < nparams && i < 8; i++) {
        emit_str64(i, FP, -(16 + (i + 1) * 8));
    }

    /* genera corpus */
    reg_vertex      = 0;
    break_vertex    = 0;
    num_goto_labels = 0;
    genera_sententia(n->dexter);

    /* epilogus implicitum (si non iam reditum) */
    emit_movi(0, 0);
    emit_addi(SP, FP, 0);            /* MOV sp, x29 */
    emit_ldp_post(FP, LR, SP, 16);
    emit_ret();

    /* recorda informationem functionis */
    if (!n->est_staticus) {
        /* exportanda */
    }
}

/* ================================================================
 * ULEB128
 * ================================================================ */

static int encode_uleb128(uint8_t *buf, uint64_t val)
{
    int i = 0;
    do {
        uint8_t byte = val & 0x7F;
        val >>= 7;
        if (val)
            byte |= 0x80;
        buf[i++] = byte;
    } while (val);
    return i;
}

/* ================================================================
 * allinea
 * ================================================================ */

static uint64_t allinea(uint64_t val, uint64_t col)
{
    return (val + col - 1) & ~(col - 1);
}

/* ================================================================
 * scribo Mach-O
 * ================================================================ */

static void write8(FILE *fp, uint8_t v) { fwrite(&v, 1, 1, fp); }
static void write16(FILE *fp, uint16_t v) { fwrite(&v, 2, 1, fp); }
static void write32(FILE *fp, uint32_t v) { fwrite(&v, 4, 1, fp); }
static void write64(FILE *fp, uint64_t v) { fwrite(&v, 8, 1, fp); }
static void write_pad(FILE *fp, int n) { for (int i = 0; i < n; i++)
    write8(fp, 0); }

static void write_str(FILE *fp, const char *s) { fwrite(s, 1, strlen(s) + 1, fp); }

static void write_seg_name(FILE *fp, const char *name)
{
    char buf[16] = {0};
    strncpy(buf, name, 16);
    fwrite(buf, 1, 16, fp);
}

void genera_initia(void)
{
    codex_lon     = 0;
    data_lon      = 0;
    chordae_lon   = 0;
    num_chordarum = 0;
    num_got       = 0;
    num_fixups    = 0;
    num_labels    = 0;
    num_globalium = 0;
    reg_vertex    = 0;
    break_vertex  = 0;
}

void genera_translatio(nodus_t *radix, const char *plica_exitus)
{
    /* registra functionem _main in GOT */
    /* genera omnes functiones */
    int main_offset = -1;

    /* prima passu: collige globales */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_VAR_DECL) {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            long val      = 0;
            if (n->sinister && n->sinister->genus == N_NUM)
                val = n->sinister->valor;
            int gid = globalis_adde(n->nomen, n->typus_decl, n->est_staticus, val);
            if (s)
                s->globalis_index = gid;
        }
    }

    /* secunda passu: registra labels pro omnibus functionibus */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF)
            func_loc_adde(n->nomen);
    }

    /* tertia passu: genera functiones */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF) {
            if (strcmp(n->nomen, "main") == 0)
                main_offset = codex_lon;
            genera_functio(n);
        }
    }

    if (main_offset < 0)
        erratum("functio main non inventa");

    /* ================================================================
     * layout Mach-O
     * ================================================================ */

    /* computare BSS offsets */
    int bss_lon = 0;
    for (int i = 0; i < num_globalium; i++) {
        int col = globales[i].colineatio;
        if (col < 1)
            col = 1;
        int mag = globales[i].magnitudo;
        if (mag < 1)
            mag = 8;
        bss_lon = (int)allinea(bss_lon, col);
        globales[i].bss_offset = bss_lon;
        bss_lon += mag;
    }
    bss_lon = (int)allinea(bss_lon, 8);

    int got_lon = num_got * 8;

    /* load commands sizes */
    int lc_pagezero  = 72;
    int lc_text      = 72 + 2 * 80;     /* __text, __cstring */
    int lc_data      = 72 + 3 * 80;     /* __got, __bss, __data */
    int lc_linkedit  = 72;
    int lc_dyld_info = 48;
    int lc_symtab    = 24;
    int lc_dysymtab  = 80;
    int lc_dylinker  = (int)allinea(12 + 14, 8); /* "/usr/lib/dyld" */
    int lc_main      = 24;
    int lc_build     = 32;
    int lc_dylib     = (int)allinea(24 + 27, 8); /* "/usr/lib/libSystem.B.dylib\0" = 27 */

    int ncmds = 11;
    int sizeofcmds = lc_pagezero + lc_text + lc_data + lc_linkedit +
        lc_dyld_info + lc_symtab + lc_dysymtab +
        lc_dylinker + lc_main + lc_build + lc_dylib;
    int header_size = 32 + sizeofcmds;

    /* relinque spatium pro codesign (LC_CODE_SIGNATURE = 16 octeti) */
    int text_sect_offset  = (int)allinea(header_size + 256, 16);
    int text_sect_size    = codex_lon;
    int cstring_offset    = (int)allinea(text_sect_offset + text_sect_size, 4);
    int cstring_size      = chordae_lon;
    int text_seg_filesize = (int)allinea(cstring_offset + cstring_size, PAGINA);
    int text_seg_vmsize   = text_seg_filesize;

    int data_seg_fileoff = text_seg_filesize;
    int got_sect_off     = 0;
    int data_sect_off    = (int)allinea(got_lon, 8);
    int data_file_size   = (int)allinea(data_sect_off, PAGINA);
    if (data_file_size < PAGINA)
        data_file_size = PAGINA;
    int data_seg_vmsize = (int)allinea(data_sect_off + bss_lon, PAGINA);
    if (data_seg_vmsize < PAGINA)
        data_seg_vmsize = PAGINA;

    int linkedit_fileoff = data_seg_fileoff + data_file_size;

    uint64_t text_vmaddr      = VM_BASIS;
    uint64_t text_sect_vmaddr = text_vmaddr + text_sect_offset;
    uint64_t cstring_vmaddr   = text_vmaddr + cstring_offset;
    uint64_t data_vmaddr      = text_vmaddr + text_seg_vmsize;
    uint64_t got_vmaddr       = data_vmaddr + got_sect_off;
    uint64_t bss_vmaddr       = data_vmaddr + data_sect_off;
    uint64_t linkedit_vmaddr  = data_vmaddr + data_seg_vmsize;

    /* ================================================================
     * applica fixups
     * ================================================================ */

    for (int i = 0; i < num_fixups; i++) {
        fixup_t *f = &fixups[i];
        uint32_t inst;
        memcpy(&inst, &codex[f->offset], 4);

        switch (f->genus) {
        case FIX_BRANCH: case FIX_BL: {
                int target_off = labels[f->target];
                if (target_off < 0)
                    erratum("label non resolutum");
                int delta = (target_off - f->offset) / 4;
                if (f->genus == FIX_BL)
                    inst = 0x94000000 | (delta & 0x3FFFFFF);
                else
                    inst = 0x14000000 | (delta & 0x3FFFFFF);
                break;
            }
        case FIX_BCOND: {
                int target_off = labels[f->target];
                int delta = (target_off - f->offset) / 4;
                inst = 0x54000000 | ((delta & 0x7FFFF) << 5) | f->magnitudo_accessus;
                break;
            }
        case FIX_CBZ: {
                int target_off = labels[f->target];
                int delta = (target_off - f->offset) / 4;
                int rt = inst & 0x1F;
                inst = 0xB4000000 | ((delta & 0x7FFFF) << 5) | rt;
                break;
            }
        case FIX_ADR_LABEL: {
                int target_off = labels[f->target];
                int delta = target_off - f->offset;
                int rd = inst & 0x1F;
                int immlo = delta & 3;
                int immhi = (delta >> 2) & 0x7FFFF;
                inst = 0x10000000 | (immlo << 29) | (immhi << 5) | rd;
                break;
            }
        case FIX_CBNZ: {
                int target_off = labels[f->target];
                int delta = (target_off - f->offset) / 4;
                int rt = inst & 0x1F;
                inst = 0xB5000000 | ((delta & 0x7FFFF) << 5) | rt;
                break;
            }
        case FIX_ADRP: {
            /* target = chorda id */
                uint64_t target_addr = cstring_vmaddr + chordae[f->target].offset;
                uint64_t pc = text_sect_vmaddr + f->offset;
                int64_t page_delta = (int64_t)((target_addr & ~0xFFFULL) - (pc & ~0xFFFULL));
                int64_t imm = page_delta >> 12;
                int rd = inst & 0x1F;
                int immlo = (int)(imm & 3);
                int immhi = (int)((imm >> 2) & 0x7FFFF);
                inst = 0x90000000 | (immlo << 29) | (immhi << 5) | rd;
                break;
            }
        case FIX_ADD_LO12: {
                uint64_t target_addr = cstring_vmaddr + chordae[f->target].offset;
                int lo12 = (int)(target_addr & 0xFFF);
                int rd = inst & 0x1F;
                int rn = (inst >> 5) & 0x1F;
                inst = 0x91000000 | (lo12 << 10) | (rn << 5) | rd;
                break;
            }
        case FIX_ADRP_GOT: {
                uint64_t target_addr = got_vmaddr + f->target * 8;
                uint64_t pc = text_sect_vmaddr + f->offset;
                int64_t page_delta = (int64_t)((target_addr & ~0xFFFULL) - (pc & ~0xFFFULL));
                int64_t imm = page_delta >> 12;
                int rd = inst & 0x1F;
                int immlo = (int)(imm & 3);
                int immhi = (int)((imm >> 2) & 0x7FFFF);
                inst = 0x90000000 | (immlo << 29) | (immhi << 5) | rd;
                break;
            }
        case FIX_LDR_GOT_LO12: {
                uint64_t target_addr = got_vmaddr + f->target * 8;
                int lo12 = (int)(target_addr & 0xFFF);
                int rd = inst & 0x1F;
                int rn = (inst >> 5) & 0x1F;
            /* LDR Xt, [Xn, #off] — offset / 8 */
                inst = 0xF9400000 | ((lo12 / 8) << 10) | (rn << 5) | rd;
                break;
            }
        case FIX_ADRP_DATA: {
                uint64_t target_addr = bss_vmaddr + globales[f->target].bss_offset;
                uint64_t pc = text_sect_vmaddr + f->offset;
                int64_t page_delta = (int64_t)((target_addr & ~0xFFFULL) - (pc & ~0xFFFULL));
                int64_t imm = page_delta >> 12;
                int rd = inst & 0x1F;
                int immlo = (int)(imm & 3);
                int immhi = (int)((imm >> 2) & 0x7FFFF);
                inst = 0x90000000 | (immlo << 29) | (immhi << 5) | rd;
                break;
            }
        case FIX_ADD_LO12_DATA: {
                uint64_t target_addr = bss_vmaddr + globales[f->target].bss_offset;
                int lo12 = (int)(target_addr & 0xFFF);
                int rd = inst & 0x1F;
                int rn = (inst >> 5) & 0x1F;
                inst = 0x91000000 | (lo12 << 10) | (rn << 5) | rd;
                break;
            }
        }
        memcpy(&codex[f->offset], &inst, 4);
    }

    /* ================================================================
     * construere bind info
     * ================================================================ */

    uint8_t bind_info[65536];
    int bind_lon = 0;

    if (num_got > 0) {
        /* data segment index = 2 (after PAGEZERO=0, TEXT=1) */
        bind_info[bind_lon++] = BIND_OPCODE_SET_DYLIB_ORDINAL_IMM | 1;
        bind_info[bind_lon++] = BIND_OPCODE_SET_TYPE_IMM | BIND_TYPE_POINTER;

        for (int i = 0; i < num_got; i++) {
            /* set segment and offset */
            bind_info[bind_lon++] = BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB | 2;
            bind_lon += encode_uleb128(
                &bind_info[bind_lon],
                got_sect_off + i * 8
            );
            /* set symbol */
            bind_info[bind_lon++] = BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM | 0;
            int slen = strlen(got[i].nomen);
            memcpy(&bind_info[bind_lon], got[i].nomen, slen + 1);
            bind_lon += slen + 1;
            /* bind */
            bind_info[bind_lon++] = BIND_OPCODE_DO_BIND;
        }
        bind_info[bind_lon++] = BIND_OPCODE_DONE;
    }
    bind_lon = (int)allinea(bind_lon, 8);

    /* ================================================================
     * construere export trie (minima — solum _main)
     * ================================================================ */

    uint8_t export_info[256];
    int export_lon = 0;

    /* radix nodus */
    export_info[export_lon++] = 0; /* terminal size = 0 */
    export_info[export_lon++] = 1; /* 1 child */
    /* edge: "_main" */
    const char *main_sym = "_main";
    memcpy(&export_info[export_lon], main_sym, strlen(main_sym) + 1);
    export_lon += strlen(main_sym) + 1;
    /* child node offset */
    int child_offset = export_lon + 1;
    export_info[export_lon++] = (uint8_t)child_offset;

    /* child nodus (terminalis) */
    /* terminal info: flags (ULEB) + address (ULEB) */
    uint8_t term_buf[16];
    int term_len = 0;
    term_len += encode_uleb128(&term_buf[term_len], 0); /* flags = regular */
    term_len += encode_uleb128(
        &term_buf[term_len],
        text_sect_offset + main_offset
    ); /* address */
    export_info[export_lon++] = (uint8_t)term_len;
    memcpy(&export_info[export_lon], term_buf, term_len);
    export_lon += term_len;
    export_info[export_lon++] = 0; /* 0 children */

    export_lon = (int)allinea(export_lon, 8);

    /* ================================================================
     * construere symbol table
     * ================================================================ */

    /* strtab */
    uint8_t strtab[65536];
    int strtab_lon       = 0;
    strtab[strtab_lon++] = ' ';  /* index 0: empty/space */
    strtab[strtab_lon++] = '\0';

    /* nlist entries */
    typedef struct {
        uint32_t n_strx;
        uint8_t n_type;
        uint8_t n_sect;
        int16_t n_desc;
        uint64_t n_value;
    } nlist64_t;

    nlist64_t symtab_entries[MAX_GOT + 16];
    int nsyms = 0;

    /* DYSYMTAB ordo: locals (nullae), extdefs (_main), undefs (GOT) */

    /* extdefs: _main */
    int iextdefsym = nsyms;
    {
        nlist64_t *nl = &symtab_entries[nsyms++];
        nl->n_strx    = strtab_lon;
        memcpy(&strtab[strtab_lon], "_main", 6);
        strtab_lon += 6;
        nl->n_type  = N_SECT | N_EXT;
        nl->n_sect  = 1; /* __text est sectio 1 */
        nl->n_desc  = 0;
        nl->n_value = text_sect_vmaddr + main_offset;
    }
    int nextdefsym = nsyms - iextdefsym;

    /* undefs: GOT symbola */
    int iundefsym = nsyms;
    for (int i = 0; i < num_got; i++) {
        nlist64_t *nl = &symtab_entries[nsyms++];
        nl->n_strx    = strtab_lon;
        memcpy(&strtab[strtab_lon], got[i].nomen, strlen(got[i].nomen) + 1);
        strtab_lon += strlen(got[i].nomen) + 1;
        nl->n_type  = N_EXT; /* external, undefined */
        nl->n_sect  = 0;
        nl->n_desc  = 0x0100; /* dylib ordinal 1 */
        nl->n_value = 0;
    }
    int nundefsym = nsyms - iundefsym;

    /* indirect symbol table: GOT entry i -> symbol index iundefsym+i */
    uint32_t indirect_syms[MAX_GOT];
    for (int i = 0; i < num_got; i++)
        indirect_syms[i] = (uint32_t)(iundefsym + i);

    strtab_lon = (int)allinea(strtab_lon, 8);

    /* linkedit layout */
    int rebase_off  = linkedit_fileoff;
    int rebase_size = 8; /* minimal: just DONE opcode */

    int bind_off  = rebase_off + rebase_size;
    int bind_size = bind_lon;

    int lazy_bind_off  = bind_off + bind_size;
    int lazy_bind_size = 8;

    int export_off  = lazy_bind_off + lazy_bind_size;
    int export_size = export_lon;

    int symtab_off  = (int)allinea(export_off + export_size, 8);
    int symtab_size = nsyms * 16; /* sizeof nlist_64 = 16 */

    int strtab_off  = symtab_off + symtab_size;
    int strtab_size = strtab_lon;

    int indsym_off  = (int)allinea(strtab_off + strtab_size, 4);
    int indsym_size = num_got * 4;

    int linkedit_size        = (int)allinea(indsym_off + indsym_size - linkedit_fileoff, PAGINA);
    uint64_t linkedit_vmsize = allinea(linkedit_size, PAGINA);

    /* ================================================================
     * scribo plicam
     * ================================================================ */

    FILE *fp = fopen(plica_exitus, "wb");
    if (!fp)
        erratum("non possum scribere '%s'", plica_exitus);

    /* --- Mach-O header --- */
    write32(fp, MH_MAGIC_64);
    write32(fp, CPU_TYPE_ARM64);
    write32(fp, CPU_SUBTYPE_ALL);
    write32(fp, MH_EXECUTE);
    write32(fp, ncmds);
    write32(fp, sizeofcmds);
    write32(fp, MH_PIE | MH_DYLDLINK | MH_TWOLEVEL);
    write32(fp, 0); /* reserved */

    /* --- LC_SEGMENT_64 __PAGEZERO --- */
    write32(fp, LC_SEGMENT_64);
    write32(fp, lc_pagezero);
    write_seg_name(fp, "__PAGEZERO");
    write64(fp, 0);                 /* vmaddr */
    write64(fp, VM_BASIS);          /* vmsize = 4GB */
    write64(fp, 0);                 /* fileoff */
    write64(fp, 0);                 /* filesize */
    write32(fp, VM_PROT_NONE);      /* maxprot */
    write32(fp, VM_PROT_NONE);      /* initprot */
    write32(fp, 0);                 /* nsects */
    write32(fp, 0);                 /* flags */

    /* --- LC_SEGMENT_64 __TEXT --- */
    write32(fp, LC_SEGMENT_64);
    write32(fp, lc_text);
    write_seg_name(fp, "__TEXT");
    write64(fp, text_vmaddr);
    write64(fp, text_seg_vmsize);
    write64(fp, 0);
    write64(fp, text_seg_filesize);
    write32(fp, VM_PROT_READ | VM_PROT_EXECUTE);
    write32(fp, VM_PROT_READ | VM_PROT_EXECUTE);
    write32(fp, 2);                 /* nsects */
    write32(fp, 0);

    /* section __text */
    write_seg_name(fp, "__text");
    write_seg_name(fp, "__TEXT");
    write64(fp, text_sect_vmaddr);
    write64(fp, text_sect_size);
    write32(fp, text_sect_offset);
    write32(fp, 2);                 /* align = 2^2 = 4 */
    write32(fp, 0);                 /* reloff */
    write32(fp, 0);                 /* nreloc */
    write32(fp, S_REGULAR | S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS);
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, 0);
    /* reserved1,2,3 */

    /* section __cstring */
    write_seg_name(fp, "__cstring");
    write_seg_name(fp, "__TEXT");
    write64(fp, cstring_vmaddr);
    write64(fp, cstring_size);
    write32(fp, cstring_offset);
    write32(fp, 0);                 /* align = 2^0 = 1 */
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, S_CSTRING_LITERALS);
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, 0);
    /* reserved1,2,3 */

    /* --- LC_SEGMENT_64 __DATA --- */
    write32(fp, LC_SEGMENT_64);
    write32(fp, lc_data);
    write_seg_name(fp, "__DATA");
    write64(fp, data_vmaddr);
    write64(fp, data_seg_vmsize);
    write64(fp, data_seg_fileoff);
    write64(fp, data_file_size);
    write32(fp, VM_PROT_READ | VM_PROT_WRITE);
    write32(fp, VM_PROT_READ | VM_PROT_WRITE);
    write32(fp, 3);                 /* nsects */
    write32(fp, 0);

    /* section __got */
    write_seg_name(fp, "__got");
    write_seg_name(fp, "__DATA");
    write64(fp, got_vmaddr);
    write64(fp, got_lon);
    write32(fp, data_seg_fileoff + got_sect_off);
    write32(fp, 3);                 /* align = 2^3 = 8 */
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, S_NON_LAZY_SYMBOL_POINTERS);
    write32(fp, 0); /* reserved1 = 0 (index in tabulam symbolorum indirectam) */
    write32(fp, 0);
    write32(fp, 0);
    /* reserved2,3 */

    /* section __bss */
    write_seg_name(fp, "__bss");
    write_seg_name(fp, "__DATA");
    write64(fp, bss_vmaddr);
    write64(fp, bss_lon);
    write32(fp, 0);                 /* no file data */
    write32(fp, 3);
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, S_ZEROFILL);
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, 0);
    /* reserved1,2,3 */

    /* section __data (placeholder) */
    write_seg_name(fp, "__data");
    write_seg_name(fp, "__DATA");
    write64(fp, bss_vmaddr + bss_lon);
    write64(fp, 0);
    write32(fp, 0);
    write32(fp, 3);
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, S_REGULAR);
    write32(fp, 0);
    write32(fp, 0);
    write32(fp, 0);
    /* reserved1,2,3 */

    /* --- LC_SEGMENT_64 __LINKEDIT --- */
    write32(fp, LC_SEGMENT_64);
    write32(fp, lc_linkedit);
    write_seg_name(fp, "__LINKEDIT");
    write64(fp, linkedit_vmaddr);
    write64(fp, linkedit_vmsize);
    write64(fp, linkedit_fileoff);
    write64(fp, linkedit_size);
    write32(fp, VM_PROT_READ);
    write32(fp, VM_PROT_READ);
    write32(fp, 0);
    write32(fp, 0);

    /* --- LC_DYLD_INFO_ONLY --- */
    write32(fp, LC_DYLD_INFO_ONLY);
    write32(fp, lc_dyld_info);
    write32(fp, rebase_off);        /* rebase_off */
    write32(fp, rebase_size);       /* rebase_size */
    write32(fp, bind_off);          /* bind_off */
    write32(fp, bind_size);         /* bind_size */
    write32(fp, 0);                 /* weak_bind_off */
    write32(fp, 0);                 /* weak_bind_size */
    write32(fp, lazy_bind_off);     /* lazy_bind_off */
    write32(fp, lazy_bind_size);    /* lazy_bind_size */
    write32(fp, export_off);        /* export_off */
    write32(fp, export_size);       /* export_size */

    /* --- LC_SYMTAB --- */
    write32(fp, LC_SYMTAB);
    write32(fp, lc_symtab);
    write32(fp, symtab_off);
    write32(fp, nsyms);
    write32(fp, strtab_off);
    write32(fp, strtab_size);

    /* --- LC_DYSYMTAB --- */
    write32(fp, LC_DYSYMTAB);
    write32(fp, lc_dysymtab);
    write32(fp, 0);  /* ilocalsym */
    write32(fp, 0);  /* nlocalsym */
    write32(fp, iextdefsym); /* iextdefsym */
    write32(fp, nextdefsym); /* nextdefsym */
    write32(fp, iundefsym);  /* iundefsym */
    write32(fp, nundefsym);  /* nundefsym */
    write32(fp, 0);
    write32(fp, 0);
    /* tocoff, ntoc */
    write32(fp, 0);
    write32(fp, 0);
    /* modtaboff, nmodtab */
    write32(fp, 0);
    write32(fp, 0);
    /* extrefsymoff, nextrefsyms */
    write32(fp, indsym_off);
    write32(fp, num_got);
    /* indirectsymoff, nindirectsyms */
    write32(fp, 0);
    write32(fp, 0);
    /* extreloff, nextrel */
    write32(fp, 0);
    write32(fp, 0);
    /* locreloff, nlocrel */

    /* --- LC_LOAD_DYLINKER --- */
    write32(fp, LC_LOAD_DYLINKER);
    write32(fp, lc_dylinker);
    write32(fp, 12);               /* name offset */
    write_str(fp, "/usr/lib/dyld");
    write_pad(fp, lc_dylinker - 12 - 14);

    /* --- LC_MAIN --- */
    write32(fp, LC_MAIN);
    write32(fp, lc_main);
    write64(fp, text_sect_offset + main_offset); /* entryoff */
    write64(fp, 0);                              /* stacksize */

    /* --- LC_BUILD_VERSION --- */
    write32(fp, LC_BUILD_VERSION);
    write32(fp, lc_build);
    write32(fp, PLATFORM_MACOS);
    write32(fp, 0x000F0000);       /* minos = 15.0 */
    write32(fp, 0x000F0000);       /* sdk = 15.0 */
    write32(fp, 1);                /* ntools */
    write32(fp, TOOL_LD);
    write32(fp, 0x03000000);       /* version */

    /* --- LC_LOAD_DYLIB --- */
    write32(fp, LC_LOAD_DYLIB);
    write32(fp, lc_dylib);
    write32(fp, 24);               /* name offset */
    write32(fp, 0);                /* timestamp */
    write32(fp, 0x010000);         /* current_version */
    write32(fp, 0x010000);         /* compat_version */
    write_str(fp, "/usr/lib/libSystem.B.dylib");
    write_pad(fp, lc_dylib - 24 - 27);

    /* --- padding ad text section --- */
    int cur_pos = header_size;
    write_pad(fp, text_sect_offset - cur_pos);

    /* --- __text section --- */
    fwrite(codex, 1, codex_lon, fp);
    cur_pos = text_sect_offset + codex_lon;

    /* --- padding ad __cstring --- */
    write_pad(fp, cstring_offset - cur_pos);
    fwrite(chordae_data, 1, chordae_lon, fp);
    cur_pos = cstring_offset + chordae_lon;

    /* --- padding ad data segment --- */
    write_pad(fp, data_seg_fileoff - cur_pos);

    /* --- __got section (zeros — dyld fills in) --- */
    write_pad(fp, got_lon);
    cur_pos = data_seg_fileoff + got_lon;

    /* --- padding ad linkedit --- */
    write_pad(fp, linkedit_fileoff - cur_pos);

    /* --- rebase info --- */
    {
        uint8_t rb[8] = {REBASE_OPCODE_DONE, 0, 0, 0, 0, 0, 0, 0};
        fwrite(rb, 1, rebase_size, fp);
    }

    /* --- bind info --- */
    fwrite(bind_info, 1, bind_size, fp);

    /* --- lazy bind info --- */
    {
        uint8_t lb[8] = {BIND_OPCODE_DONE, 0, 0, 0, 0, 0, 0, 0};
        fwrite(lb, 1, lazy_bind_size, fp);
    }

    /* --- export trie --- */
    fwrite(export_info, 1, export_lon, fp);
    write_pad(fp, export_off + export_size - (export_off + export_lon));

    /* --- padding ad symtab --- */
    cur_pos = export_off + export_size;
    write_pad(fp, symtab_off - cur_pos);

    /* --- symbol table --- */
    for (int i = 0; i < nsyms; i++) {
        write32(fp, symtab_entries[i].n_strx);
        write8(fp, symtab_entries[i].n_type);
        write8(fp, symtab_entries[i].n_sect);
        write16(fp, (uint16_t)symtab_entries[i].n_desc);
        write64(fp, symtab_entries[i].n_value);
    }

    /* --- string table --- */
    fwrite(strtab, 1, strtab_lon, fp);

    /* --- tabula symbolorum indirecta --- */
    cur_pos = strtab_off + strtab_lon;
    write_pad(fp, indsym_off - cur_pos);
    for (int i = 0; i < num_got; i++)
        write32(fp, indirect_syms[i]);

    /* --- padding ad finem --- */
    cur_pos     = indsym_off + indsym_size;
    int end_pos = linkedit_fileoff + linkedit_size;
    if (end_pos > cur_pos)
        write_pad(fp, end_pos - cur_pos);

    fclose(fp);

    /* signa ad-hoc */
    {
        char cmd[1024];
        snprintf(
            cmd, sizeof(cmd),
            "chmod +x '%s' && codesign -s - '%s' 2>/dev/null",
            plica_exitus, plica_exitus
        );
        system(cmd);
    }

    printf(
        "==> %s scriptum (%d octeti codis, %d GOT, %d globales)\n",
        plica_exitus, codex_lon, num_got, num_globalium
    );
}
