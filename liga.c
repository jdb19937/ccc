/*
 * liga.c — ligator objectorum Mach-O
 *
 * Legit plicas MH_OBJECT a ccc generatas, componit codicem,
 * resolvit symbola, et scribit executabile per scribo_macho.
 */

#include "utilia.h"
#include "emitte.h"
#include "scribo.h"
#include "biblio.h"

/* ================================================================
 * structurae internae
 * ================================================================ */

typedef struct {
    char nomen[260];
    int definita;           /* 1 si in hoc obiecto definita */
    int sectio;             /* sectio (1 = __text) */
    int valor;              /* offset in sectione */
    int obiectum;           /* index obiecti */
    int valor_globalis;     /* offset in codice finali */
    int est_communis;       /* symbolum commune (variabilis globalis) */
    int magnitudo_communis; /* magnitudo symboli communis */
    int globalis_index;     /* index in tabula globalium (-1 si non) */
} liga_sym_t;

/* relocatio dilata — servatur pro passu secundo */
typedef struct {
    int  obiectum;
    int  r_addr;
    int  r_sym;         /* index symboli in obiecto (extern) vel sectio (non-extern) */
    int  r_type;
    int  r_extern;
    int  addendum;      /* ex ARM64_RELOC_ADDEND praevio (sign-extendit 24-bit) */
    char sym_nomen[260];
    int  sym_sect;      /* sectio symboli (pro externis chordis) */
    int  sym_valor;     /* n_value symboli (pro externis chordis) */
} liga_reloc_t;

/* relocātiō dātōrum dīlāta — externum indefīnītum in hōc .o,
 * resolvendum per nōmen post prīmum passum */
typedef struct {
    int  patch_off;
    char sym_nomen[260];
} liga_data_reloc_t;

#define MAX_LIGA_SYM 4096

static liga_sym_t liga_syms[MAX_LIGA_SYM];
static int num_liga_sym = 0;

/* quaere symbolum per nomen */
static int liga_sym_quaere(const char *nomen)
{
    for (int i = 0; i < num_liga_sym; i++)
        if (strcmp(liga_syms[i].nomen, nomen) == 0)
            return i;
    return -1;
}

/* adde symbolum */
static int liga_sym_adde(const char *nomen)
{
    int id = liga_sym_quaere(nomen);
    if (id >= 0)
        return id;
    if (num_liga_sym >= MAX_LIGA_SYM)
        erratum("nimis multa symbola in ligatione");
    id = num_liga_sym++;
    strncpy(liga_syms[id].nomen, nomen, 259);
    liga_syms[id] .definita         = 0;
    liga_syms[id] .valor_globalis   = -1;
    liga_syms[id] .est_communis     = 0;
    liga_syms[id] .magnitudo_communis = 0;
    liga_syms[id] .globalis_index   = -1;
    return id;
}

/* ================================================================
 * auxiliares pro lectione Mach-O
 * ================================================================ */

static uint32_t lege32(const uint8_t *p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t lege64(const uint8_t *p) {
    return (uint64_t)lege32(p) | ((uint64_t)lege32(p+4) << 32);
}

/* ================================================================
 * patchā īnstrūctiōnem BRANCH26 (B aut BL) ut ad scopum saliat.
 *
 * Bitum 31 (vinculum) servātur ex īnstrūctiōne orīginālī: 0x14=B,
 * 0x94=BL. Clang cauda-vocātiōnēs cum B ēmittit, quī LR nōn corrumpit;
 * BL contemporat rēgistrum ligātiōnis. Ergō bitum 31 nōn mūtandum est.
 * ================================================================ */

static void patch_branch26(int inst_off, int target)
{
    int delta = (target - inst_off) / 4;
    uint32_t old_inst;
    memcpy(&old_inst, codex + inst_off, 4);
    uint32_t inst = (old_inst & 0x80000000)
        | 0x14000000 | (delta & 0x3FFFFFF);
    memcpy(codex + inst_off, &inst, 4);
}

/* ================================================================
 * stubs prō _objc_msgSend$<sel> — fabricātiō ABI ObjC moderna
 *
 * Clang, cum ObjC compīlat (ABI recens), pro quōque SEL uniquē ēmittit
 * symbolum indēfīnītum `_objc_msgSend$<sel>` quem ligātor substituet
 * stubō quī SEL ex `_sel_registerName` adipīscitur, deinde `_objc_msgSend`
 * vocat. Hīc stubus cache sēgregātum habet ut SEL semel tantum registrētur.
 * ================================================================ */

#define MAX_OBJC_STUBS 1024

typedef struct {
    char nomen[260];
    int  stub_off;
} objc_stub_t;

static objc_stub_t objc_stubs[MAX_OBJC_STUBS];
static int num_objc_stubs = 0;

static int objc_stub_quaere(const char *nomen)
{
    for (int i = 0; i < num_objc_stubs; i++)
        if (strcmp(objc_stubs[i].nomen, nomen) == 0)
            return objc_stubs[i].stub_off;
    return -1;
}

static int objc_msgsend_stub_adde(const char *sym_nomen)
{
    /* dēdūplicā */
    int iam = objc_stub_quaere(sym_nomen);
    if (iam >= 0)
        return iam;

    if (num_objc_stubs >= MAX_OBJC_STUBS)
        erratum("nimis multī stubī msgSend");

    /* extractā selectōre post '$' */
    const char *dollar = strchr(sym_nomen, '$');
    if (!dollar || !dollar[1])
        erratum("symbolum msgSend malformātum: '%s'", sym_nomen);
    const char  *sel = dollar + 1;
    int sel_len      = (int)strlen(sel);

    /* reservā 8 octetos in init_data prō cache SEL (initiātus ad 0) */
    if (init_data_lon + 8 > MAX_DATA)
        erratum("init_data nimis magna (cache SEL)");
    int cache_off = init_data_lon;
    memset(init_data + cache_off, 0, 8);
    init_data_lon += 8;

    /* chorda prō nōmine selectōris in __cstring */
    int cid = chorda_adde(sel, sel_len);

    /* intrantēs GOT prō runtime ObjC */
    int gid_reg = got_adde("_sel_registerName");
    int gid_msg = got_adde("_objc_msgSend");

    int stub_off = codex_lon;

    /* ADRP x9, cache@PAGE */
    fixup_adde(FIX_ADRP_IDATA, codex_lon, cache_off, 0);
    emit32(0x90000000 | 9);
    /* ADD x9, x9, cache@PAGEOFF */
    fixup_adde(FIX_ADD_LO12_IDATA, codex_lon, cache_off, 0);
    emit32(0x91000000 | (9 << 5) | 9);
    /* LDR x1, [x9] */
    emit_ldr64(1, 9, 0);

    /* CBNZ x1, Ldone — via label */
    int Ldone = label_novus();
    emit_cbnz_label(1, Ldone);

    /* — via lenta: registrā SEL cum sel_registerName — */
    /* STP x29, x30, [sp, #-160]! (reservā spatium prō x et d rēgistrīs) */
    emit_stp_pre(FP, LR, SP, -160);
    /* MOV x29, sp */
    emit_addi(FP, SP, 0);
    /* servā x0 (self) et argumenta x2..x8 in stacō */
    emit_str64(0, FP, 16);
    emit_str64(2, FP, 24);
    emit_str64(3, FP, 32);
    emit_str64(4, FP, 40);
    emit_str64(5, FP, 48);
    emit_str64(6, FP, 56);
    emit_str64(7, FP, 64);
    emit_str64(8, FP, 72);
    /* servā d0..d7 — sel_registerName pote clobber FP rēgistra
     * per AAPCS64; initWithContentRect: argūmenta NSRect passat
     * in d0-d3, et sine servātiōne fenestra crearētur cum
     * dimēnsiōnibus corruptīs (invīsibilis). */
    for (int d = 0; d < 8; d++) {
        int imm = 80 + d * 8;
        uint32_t inst = 0xFD000000 | (((uint32_t)imm / 8) << 10)
            | ((uint32_t)FP << 5) | (uint32_t)d;
        emit32(inst);
    }

    /* ADRP x0, sel_str@PAGE ; ADD x0, x0, sel_str@PAGEOFF */
    fixup_adde(FIX_ADRP, codex_lon, cid, 0);
    emit32(0x90000000 | 0);
    fixup_adde(FIX_ADD_LO12, codex_lon, cid, 0);
    emit32(0x91000000 | (0 << 5) | 0);

    /* ADRP x17, _sel_registerName@GOTPAGE */
    fixup_adde(FIX_ADRP_GOT, codex_lon, gid_reg, 0);
    emit32(0x90000000 | 17);
    /* LDR x17, [x17, _sel_registerName@GOTPAGEOFF] */
    fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid_reg, 8);
    emit32(0xF9400000 | (17 << 5) | 17);
    /* BLR x17 */
    emit32(0xD63F0000 | (17 << 5));

    /* x0 = SEL; scribe in cache */
    /* MOV x1, x0 (ORR Xd, XZR, Xm) */
    emit32(0xAA0003E0 | (0 << 16) | 1);

    /* recomputā cache adressa ut x17 nōn corrumpere opus sit */
    /* ADRP x9, cache@PAGE ; ADD x9, x9, cache@PAGEOFF */
    fixup_adde(FIX_ADRP_IDATA, codex_lon, cache_off, 0);
    emit32(0x90000000 | 9);
    fixup_adde(FIX_ADD_LO12_IDATA, codex_lon, cache_off, 0);
    emit32(0x91000000 | (9 << 5) | 9);
    /* STR x1, [x9] */
    emit_str64(1, 9, 0);

    /* restitue argumenta */
    emit_ldr64(0, FP, 16);
    emit_ldr64(2, FP, 24);
    emit_ldr64(3, FP, 32);
    emit_ldr64(4, FP, 40);
    emit_ldr64(5, FP, 48);
    emit_ldr64(6, FP, 56);
    emit_ldr64(7, FP, 64);
    emit_ldr64(8, FP, 72);
    /* restitue d0..d7 (LDR Dt, [FP, #imm]) */
    for (int d = 0; d < 8; d++) {
        int imm = 80 + d * 8;
        uint32_t inst = 0xFD400000 | (((uint32_t)imm / 8) << 10)
            | ((uint32_t)FP << 5) | (uint32_t)d;
        emit32(inst);
    }

    /* LDP x29, x30, [sp], #160 */
    emit_ldp_post(FP, LR, SP, 160);

    /* — Ldone: tail-call in _objc_msgSend — */
    label_pone(Ldone);
    /* ADRP x17, _objc_msgSend@GOTPAGE */
    fixup_adde(FIX_ADRP_GOT, codex_lon, gid_msg, 0);
    emit32(0x90000000 | 17);
    /* LDR x17, [x17, _objc_msgSend@GOTPAGEOFF] */
    fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid_msg, 8);
    emit32(0xF9400000 | (17 << 5) | 17);
    /* BR x17 */
    emit32(0xD61F0000 | (17 << 5));

    /* registrā in tabulā */
    strncpy(objc_stubs[num_objc_stubs].nomen, sym_nomen, 259);
    objc_stubs[num_objc_stubs] .nomen[259] = '\0';
    objc_stubs[num_objc_stubs] .stub_off   = stub_off;
    num_objc_stubs++;

    return stub_off;
}

/* ================================================================
 * ligatio
 * ================================================================ */

void liga_objecta(int num_obj, const char **viae, const char *plica_exitus)
{
    num_objc_stubs = 0;
    emitte_initia();
    num_liga_sym = 0;

    /* relocationes dilatae */
    liga_reloc_t   *relocs = NULL;
    int num_relocs         = 0;
    int cap_relocs         = 0;

    /* relocātiōnēs dātōrum dīlātae (ad symbola externa indefīnīta) */
    liga_data_reloc_t *data_relocs_dilatae = NULL;
    int num_data_relocs_dilatae            = 0;
    int cap_data_relocs_dilatae            = 0;

    int *codex_bases   = malloc(num_obj * sizeof(int));
    int *cstr_bases    = malloc(num_obj * sizeof(int));
    int *data_bases    = malloc(num_obj * sizeof(int));
    int *text_mags     = malloc(num_obj * sizeof(int));
    int *cstr_sectnums = malloc(num_obj * sizeof(int));
    int *data_sectnums = malloc(num_obj * sizeof(int));
    int *cstr_vmaddrs  = malloc(num_obj * sizeof(int));
    int *data_vmaddrs  = malloc(num_obj * sizeof(int));

    /* tabula sectionum generalis pro resolutione symbolorum localium */
    #define MAX_SECT 32
    /* genus: 0=ignota, 1=textus, 2=chordae, 3=idata (__data/__const/__bss) */
    int *sect_genera      = calloc(num_obj * MAX_SECT, sizeof(int));
    int *sect_idata_bases = calloc(num_obj * MAX_SECT, sizeof(int));
    int *sect_idata_vmas  = calloc(num_obj * MAX_SECT, sizeof(int));

    if (
        !codex_bases || !cstr_bases || !data_bases || !text_mags ||
        !cstr_sectnums || !data_sectnums || !cstr_vmaddrs || !data_vmaddrs ||
        !sect_genera || !sect_idata_bases || !sect_idata_vmas
    )
        erratum("memoria exhausta");

    /* === passus praevius: computā magnitūdinēs emmerārum init_data.
     * Sectiōnēs __DATA (et __const, __literal*, __objc_*, etc.) per
     * nōmen in emmeram dispōnuntur ut in executābilī cum nōmine
     * oriǵinālī servārī possint. Hoc necessārium est ut runtime ObjC
     * sectiōnēs __objc_classlist/classrefs/etc. per nōmen inveniat. */
    #define ALIGN_UP(v, a) (((v) + ((a)-1)) & ~((a)-1))
    int emm_seen[N_IDATA_EMM] = {0};
    for (int oi = 0; oi < num_obj; oi++) {
        int lon;
        uint8_t        *data = (uint8_t *)lege_plicam(viae[oi], &lon);
        uint32_t ncmds = lege32(data + 16);
        int cmd_off    = 32;
        for (uint32_t ci = 0; ci < ncmds; ci++) {
            uint32_t cmd  = lege32(data + cmd_off);
            uint32_t csiz = lege32(data + cmd_off + 4);
            if (cmd == LC_SEGMENT_64) {
                uint32_t nsects = lege32(data + cmd_off + 64);
                int sect_off    = cmd_off + 72;
                for (uint32_t si = 0; si < nsects; si++) {
                    char sectname[17] = {0};
                    memcpy(sectname, data + sect_off, 16);
                    uint64_t s_size  = lege64(data + sect_off + 40);
                    uint32_t s_align = lege32(data + sect_off + 52);
                    int s_aln        = s_align > 12 ? 4096 : (1 << s_align);
                    /* sōlum sectiōnēs dātōrum; salta __text et __cstring */
                    if (
                        strcmp(sectname, "__text") != 0
                        && strcmp(sectname, "__cstring") != 0
                    ) {
                        int b = idata_emm_pro_nomine(sectname);
                        if (!(idata_emm[b].dedup && emm_seen[b])) {
                            idata_emm[b].size = ALIGN_UP(idata_emm[b].size, s_aln)
                                + (int)s_size;
                        }
                        if (s_align > (uint32_t)idata_emm[b].align_log2)
                            idata_emm[b].align_log2 = s_align;
                        emm_seen[b] = 1;
                    }
                    sect_off += 80;
                }
            }
            cmd_off += csiz;
        }
        free(data);
    }
    /* assignā offsetōs initiālēs per emmeram; init_data_lon = summa */
    {
        int cursor = 0;
        for (int i = 0; i < N_IDATA_EMM; i++) {
            int aln      = 1 << idata_emm[i].align_log2;
            cursor       = ALIGN_UP(cursor, aln);
            idata_emm[i] .start = cursor;
            cursor += idata_emm[i].size;
        }
        init_data_lon = cursor;
        if (init_data_lon > MAX_DATA)
            erratum("data nimis magna in ligatione");
        memset(init_data, 0, init_data_lon);
    }
    int emm_cursor[N_IDATA_EMM] = {0};
    #undef ALIGN_UP

    /* === passus primus: lege objecta, collige symbola et codicem === */

    for (int oi = 0; oi < num_obj; oi++) {
        int lon;
        uint8_t *data = (uint8_t *)lege_plicam(viae[oi], &lon);
        if (lon < 32)
            erratum("plica '%s' nimis brevis", viae[oi]);

        uint32_t magicum = lege32(data);
        if (magicum != MH_MAGIC_64)
            erratum("plica '%s' non est Mach-O 64", viae[oi]);

        uint32_t plica_typus = lege32(data + 12);
        if (plica_typus != 1) /* MH_OBJECT */
            erratum("plica '%s' non est obiectum", viae[oi]);

        uint32_t ncmds = lege32(data + 16);
        int cmd_off    = 32;

        int text_off     = 0, text_mag = 0;
        int cstr_off     = 0, cstr_mag = 0;
        int idata_off    = 0, idata_mag = 0;
        int reloc_off    = 0, nrelocs = 0;
        int symtab_off   = 0, nsyms_obj = 0;
        int strtab_off   = 0;
        int cstr_sectnum = -1, data_sectnum = -1;
        int cstr_vma     = 0, data_vma = 0;
        int sect_counter = 0;
        /* temporanea pro sectiones extra (__const, __bss) */
        int sect_file_offs[MAX_SECT]  = {0};
        int sect_mags[MAX_SECT]       = {0};
        int sect_is_bss[MAX_SECT]     = {0};
        int sect_reloc_offs[MAX_SECT] = {0};
        int sect_nrelocs[MAX_SECT]    = {0};
        int sect_aligns[MAX_SECT]     = {0};
        int sect_emms[MAX_SECT]       = {0};
        int text_align = 4, cstr_align = 1, data_align = 1;

        for (uint32_t ci = 0; ci < ncmds; ci++) {
            uint32_t cmd  = lege32(data + cmd_off);
            uint32_t csiz = lege32(data + cmd_off + 4);

            if (cmd == LC_SEGMENT_64) {
                uint32_t nsects = lege32(data + cmd_off + 64);
                int sect_off    = cmd_off + 72;
                for (uint32_t si = 0; si < nsects; si++) {
                    char sectname[17] = {0};
                    memcpy(sectname, data + sect_off, 16);
                    uint64_t s_addr   = lege64(data + sect_off + 32);
                    uint64_t s_size   = lege64(data + sect_off + 40);
                    uint32_t s_off    = lege32(data + sect_off + 48);
                    uint32_t s_align  = lege32(data + sect_off + 52);
                    uint32_t s_reloff = lege32(data + sect_off + 56);
                    uint32_t s_nreloc = lege32(data + sect_off + 60);
                    /* §4.7.1: align est log2 allineātiōnis; cap ad 4096 */
                    int s_aln = s_align > 12 ? 4096 : (1 << s_align);
                    sect_counter++; /* Mach-O sections 1-indexed */

                    /* registra in tabula sectionum */
                    if (sect_counter < MAX_SECT) {
                        int si_idx = oi * MAX_SECT + sect_counter;
                        sect_idata_vmas[si_idx] = (int)s_addr;
                        sect_file_offs[sect_counter] = s_off;
                        sect_mags[sect_counter] = (int)s_size;
                        sect_reloc_offs[sect_counter] = s_reloff;
                        sect_nrelocs[sect_counter] = s_nreloc;
                        sect_aligns[sect_counter] = s_aln;
                    }

                    if (strcmp(sectname, "__text") == 0) {
                        text_off   = s_off;
                        text_mag   = (int)s_size;
                        reloc_off  = s_reloff;
                        nrelocs    = s_nreloc;
                        text_align = s_aln < 4 ? 4 : s_aln;
                        if (sect_counter < MAX_SECT)
                            sect_genera[oi * MAX_SECT + sect_counter] = 1;
                    } else if (strcmp(sectname, "__cstring") == 0) {
                        cstr_off     = s_off;
                        cstr_mag     = (int)s_size;
                        cstr_sectnum = sect_counter;
                        cstr_vma     = (int)s_addr;
                        cstr_align   = s_aln;
                        if (sect_counter < MAX_SECT)
                            sect_genera[oi * MAX_SECT + sect_counter] = 2;
                    } else if (strcmp(sectname, "__data") == 0) {
                        idata_off    = s_off;
                        idata_mag    = (int)s_size;
                        data_sectnum = sect_counter;
                        data_vma     = (int)s_addr;
                        data_align   = s_aln;
                        if (sect_counter < MAX_SECT) {
                            sect_genera[oi * MAX_SECT + sect_counter] = 3;
                            sect_emms[sect_counter]
                                = idata_emm_pro_nomine(sectname);
                        }
                    } else if (
                        strcmp(sectname, "__bss") == 0
                        || strcmp(sectname, "__common") == 0
                    ) {
                        if (sect_counter < MAX_SECT) {
                            sect_genera[oi * MAX_SECT + sect_counter] = 3;
                            sect_is_bss[sect_counter] = 1;
                            sect_emms[sect_counter]
                                = idata_emm_pro_nomine(sectname);
                        }
                    } else if (sect_counter < MAX_SECT) {
                        /* sectiō nōn-text/cstring: __const, __literal*,
                         * __objc_*, __cfstring, __compact_unwind, etc.
                         * Omnēs in emmeram dispōnuntur per nōmen.
                         * S_ZEROFILL (typus 1) implētur cum zerīs. */
                        uint32_t s_flags = lege32(data + sect_off + 64);
                        sect_genera[oi   * MAX_SECT + sect_counter] = 3;
                        sect_emms[sect_counter]
                            = idata_emm_pro_nomine(sectname);
                        if ((s_flags & 0xFF) == 1)
                            sect_is_bss[sect_counter] = 1;
                    }
                    sect_off += 80;
                }
            } else if (cmd == LC_SYMTAB) {
                symtab_off = (int)lege32(data + cmd_off + 8);
                nsyms_obj  = (int)lege32(data + cmd_off + 12);
                strtab_off = (int)lege32(data + cmd_off + 16);
            }
            cmd_off += csiz;
        }

        /* §6.2.9: allineā offsetōs ante posītionem singulī .o sectiōnis.
         * Sine hōc, bytes a .o priōris spillant in sectiōnem sequentis
         * cum allineātiōne alta (e.g. _bayer_8x8 __const align 2^2 = 4)
         * et instrūctiōnēs LDR cum PAGEOFF12 et scale > 1 offsetum male
         * codificant (scribo.c applica_lo12_idata: lo12 / scale truncat). */
        #define ALIGN_UP(v, a) (((v) + ((a)-1)) & ~((a)-1))
        if (text_mag > 0)
            codex_lon = ALIGN_UP(codex_lon, text_align);
        if (cstr_mag > 0)
            chordae_lon = ALIGN_UP(chordae_lon, cstr_align);

        codex_bases[oi]   = codex_lon;
        text_mags[oi]     = text_mag;
        cstr_bases[oi]    = chordae_lon;
        data_bases[oi]    = 0; /* nōn ūsum — layout per emmeram fit */
        cstr_sectnums[oi] = cstr_sectnum;
        data_sectnums[oi] = data_sectnum;
        cstr_vmaddrs[oi]  = cstr_vma;
        data_vmaddrs[oi]  = data_vma;
        (void)idata_off;
        (void)idata_mag;
        (void)data_align;

        if (text_mag > 0) {
            if (codex_lon + text_mag > MAX_CODEX)
                erratum("codex nimis magnus in ligatione");
            memcpy(codex + codex_lon, data + text_off, text_mag);
            codex_lon += text_mag;
        }

        if (cstr_mag > 0) {
            if (chordae_lon + cstr_mag > MAX_DATA)
                erratum("chordae nimis magnae in ligatione");
            memcpy(chordae_data + chordae_lon, data + cstr_off, cstr_mag);
            chordae_lon += cstr_mag;
        }

        /* Sectiōnēs dātōrum (genus 3) per emmerās dispōne. Omnēs
         * sectiōnēs — __data, __const, __cfstring, __objc_* — in
         * emmerā apud nōmen dēterminantur. In emmerā cuique, cursor
         * dēterminat offset intrā emmeram. Emmerae cum dedup=1
         * (e.g. __objc_imageinfo) sōlum prīmam instantiam retinent;
         * sequentēs ad eundem offsetum iaciuntur sine augmentō. */
        for (int sn = 1; sn <= sect_counter && sn < MAX_SECT; sn++) {
            int si_idx = oi * MAX_SECT + sn;
            if (sect_genera[si_idx] != 3)
                continue;
            int b   = sect_emms[sn];
            int aln = sect_aligns[sn] > 0 ? sect_aligns[sn] : 1;
            if (idata_emm[b].dedup && emm_cursor[b] > 0) {
                /* iam positum — omnēs occurrentiae ad eundem offsetum */
                sect_idata_bases[si_idx] = idata_emm[b].start;
                continue;
            }
            emm_cursor[b] = ALIGN_UP(emm_cursor[b], aln);
            int off_in_idata = idata_emm[b].start + emm_cursor[b];
            sect_idata_bases[si_idx] = off_in_idata;
            if (sect_mags[sn] > 0) {
                if (
                    off_in_idata + sect_mags[sn]
                    > idata_emm[b].start + idata_emm[b].size
                )
                    erratum(
                        "sectiō '%s' extrā emmeram '%s'",
                        "?", idata_emm[b].sectname
                    );
                if (sect_is_bss[sn])
                    memset(init_data + off_in_idata, 0, sect_mags[sn]);
                else
                    memcpy(
                        init_data + off_in_idata, data + sect_file_offs[sn],
                        sect_mags[sn]
                    );
                emm_cursor[b] += sect_mags[sn];
            }
        }
        #undef ALIGN_UP

        /* lege symbola */
        for (int si = 0; si < nsyms_obj; si++) {
            int noff         = symtab_off + si * 16;
            uint32_t n_strx  = lege32(data + noff);
            uint8_t  n_type  = data[noff + 4];
            uint8_t  n_sect  = data[noff + 5];
            uint64_t n_value = lege64(data + noff + 8);

            const char *nomen = (const char *)(data + strtab_off + n_strx);

            /* praetermitte symbola localia (sine N_EXT) */
            if (!(n_type & N_EXT))
                continue;

            int id = liga_sym_adde(nomen);

            if ((n_type & 0x0E) == N_SECT) {
                liga_syms[id] .definita       = 1;
                liga_syms[id] .sectio         = n_sect;
                liga_syms[id] .valor          = (int)n_value;
                liga_syms[id] .obiectum       = oi;
                liga_syms[id] .valor_globalis = codex_bases[oi] + (int)n_value;
            } else if ((n_type & 0x0E) == 0 && (n_type & N_EXT) && n_value > 0) {
                /* symbolum commune — variabilis globalis */
                if (
                    !liga_syms[id].est_communis ||
                    (int)n_value > liga_syms[id].magnitudo_communis
                )
                    liga_syms[id].magnitudo_communis = (int)n_value;
                liga_syms[id].est_communis = 1;
            }
        }

        /* collige relocationes (processa in passu secundo).
         * ARM64_RELOC_ADDEND (typus 10) est pseudo-relocātiō quae
         * praecedit PAGE21/PAGEOFF12 et addendum explicitum fornit.
         * Conservā addendum pro relocātiōne sequentī. */
        int pending_addendum = 0;
        for (int ri = 0; ri < nrelocs; ri++) {
            int roff        = reloc_off + ri * 8;
            uint32_t r_addr = lege32(data + roff);
            uint32_t r_info = lege32(data + roff + 4);

            int r_ext  = (r_info >> 27) & 1;
            int r_sym  = r_info & 0xFFFFFF;
            int r_type = (r_info >> 28) & 0xF;

            if (r_type == 10) {
                /* extende signum 24-bit r_sym ut addendum */
                int ad = r_sym;
                if (ad & 0x800000)
                    ad |= ~0xFFFFFF;
                pending_addendum = ad;
                continue;
            }

            /* serva relocationem */
            if (num_relocs >= cap_relocs) {
                cap_relocs = cap_relocs ? cap_relocs * 2 : 64;
                relocs     = realloc(relocs, cap_relocs * sizeof(liga_reloc_t));
                if (!relocs)
                    erratum("memoria exhausta");
            }
            relocs[num_relocs] .obiectum = oi;
            relocs[num_relocs] .r_addr   = (int)r_addr;
            relocs[num_relocs] .r_sym    = r_sym;
            relocs[num_relocs] .r_type   = r_type;
            relocs[num_relocs] .r_extern = r_ext;
            relocs[num_relocs] .addendum = pending_addendum;
            pending_addendum   = 0;
            if (r_ext) {
                int sym_noff       = symtab_off + r_sym * 16;
                uint32_t sym_strx  = lege32(data + sym_noff);
                uint8_t  sym_nsect = data[sym_noff + 5];
                uint64_t sym_nval  = lege64(data + sym_noff + 8);
                const char         *sym_nomen = (const char *)(data + strtab_off + sym_strx);
                strncpy(relocs[num_relocs].sym_nomen, sym_nomen, 259);
                relocs[num_relocs] .sym_sect  = sym_nsect;
                relocs[num_relocs] .sym_valor = (int)sym_nval;
            } else {
                relocs[num_relocs] .sym_nomen[0] = '\0';
                relocs[num_relocs] .sym_sect     = 0;
                relocs[num_relocs] .sym_valor    = 0;
            }
            num_relocs++;
        }

        /* collige relocātiōnēs datōrum (ARM64_RELOC_UNSIGNED in sectiōnibus datōrum) */
        for (int sn = 1; sn <= sect_counter && sn < MAX_SECT; sn++) {
            int si_idx = oi * MAX_SECT + sn;
            if (sect_genera[si_idx] != 3) /* sōlum sectiōnēs datōrum */
                continue;
            int s_reloff = sect_reloc_offs[sn];
            int s_nreloc = sect_nrelocs[sn];

            for (int ri = 0; ri < s_nreloc; ri++) {
                int roff        = s_reloff + ri * 8;
                uint32_t r_addr = lege32(data + roff);
                uint32_t r_info = lege32(data + roff + 4);

                int r_ext  = (r_info >> 27) & 1;
                int r_sym  = r_info & 0xFFFFFF;
                int r_type = (r_info >> 28) & 0xF;

                if (r_type != 0) /* sōlum ARM64_RELOC_UNSIGNED (typus 0) */
                    continue;

                /* computa offset in init_data */
                int patch_off = sect_idata_bases[si_idx] + (int)r_addr;

                if (r_ext) {
                    int sym_noff       = symtab_off + r_sym * 16;
                    uint8_t  sym_nsect = data[sym_noff + 5];
                    uint64_t sym_nval  = lege64(data + sym_noff + 8);

                    if (cstr_sectnum > 0 && sym_nsect == cstr_sectnum) {
                        int cstr_target = cstr_bases[oi]
                            + (int)sym_nval - cstr_vma;
                        data_reloc_adde(patch_off, DR_CSTRING, cstr_target);
                    } else if (sym_nsect == 1) {
                        int text_target = codex_bases[oi] + (int)sym_nval;
                        data_reloc_adde(patch_off, DR_TEXT, text_target);
                    } else if (sym_nsect > 0 && sym_nsect < MAX_SECT) {
                        int tsi = oi * MAX_SECT + sym_nsect;
                        int sg  = sect_genera[tsi];
                        if (sg == 2) {
                            int cstr_target = cstr_bases[oi]
                                + (int)sym_nval - cstr_vma;
                            data_reloc_adde(
                                patch_off, DR_CSTRING,
                                cstr_target
                            );
                        } else if (sg == 3) {
                            int idata_target = sect_idata_bases[tsi]
                                + (int)sym_nval
                                - sect_idata_vmas[tsi];
                            data_reloc_adde(
                                patch_off, DR_IDATA,
                                idata_target
                            );
                        } else {
                            erratum(
                                "ARM64_RELOC_UNSIGNED: sectiō %d "
                                "genus %d nōn sustenta",
                                sym_nsect, sg
                            );
                        }
                    } else if (sym_nsect == 0) {
                        /* externum indefīnītum in hōc .o —
                         * dīlāta resolūtiō per nōmen post prīmum passum */
                        uint32_t sym_strx = lege32(data + sym_noff);
                        const char *sym_nomen =
                            (const char *)(data + strtab_off + sym_strx);
                        if (num_data_relocs_dilatae >= cap_data_relocs_dilatae) {
                            cap_data_relocs_dilatae = cap_data_relocs_dilatae
                                ? cap_data_relocs_dilatae * 2 : 64;
                            data_relocs_dilatae = realloc(
                                data_relocs_dilatae,
                                cap_data_relocs_dilatae
                                * sizeof(liga_data_reloc_t)
                            );
                            if (!data_relocs_dilatae)
                                erratum("memoria exhausta");
                        }
                        data_relocs_dilatae[num_data_relocs_dilatae].patch_off
                            = patch_off;
                        strncpy(
                            data_relocs_dilatae[num_data_relocs_dilatae]
                            .sym_nomen,
                            sym_nomen, 259
                        );
                        data_relocs_dilatae[num_data_relocs_dilatae]
                            .sym_nomen[259] = '\0';
                        num_data_relocs_dilatae++;
                    } else {
                        erratum(
                            "ARM64_RELOC_UNSIGNED: sectiō %d invalida",
                            sym_nsect
                        );
                    }
                } else {
                    /* relocātiō nōn-externa — sectiō-relātīva */
                    int sect_num      = r_sym;
                    uint64_t addendum = 0;
                    memcpy(&addendum, init_data + patch_off, 8);

                    if (cstr_sectnum > 0 && sect_num == cstr_sectnum) {
                        int cstr_target = cstr_bases[oi]
                            + (int)addendum - cstr_vma;
                        data_reloc_adde(patch_off, DR_CSTRING, cstr_target);
                    } else if (sect_num == 1) {
                        int text_target = codex_bases[oi] + (int)addendum;
                        data_reloc_adde(patch_off, DR_TEXT, text_target);
                    } else if (sect_num > 0 && sect_num < MAX_SECT) {
                        /* §6.7.8¶4: relocātiō ad sectiōnem dātōrum
                         * (init_data/const) — resolvī per idata offset */
                        int tsi = oi * MAX_SECT + sect_num;
                        int sg  = sect_genera[tsi];
                        if (sg == 3) {
                            int idata_target = sect_idata_bases[tsi]
                                + (int)addendum
                                - sect_idata_vmas[tsi];
                            data_reloc_adde(
                                patch_off, DR_IDATA,
                                idata_target
                            );
                        } else {
                            erratum(
                                "ARM64_RELOC_UNSIGNED nōn-externa: "
                                "sectiō %d genus %d nōn sustenta",
                                sect_num, sg
                            );
                        }
                    } else {
                        erratum(
                            "ARM64_RELOC_UNSIGNED nōn-externa: "
                            "sectiō %d invalida", sect_num
                        );
                    }
                }
            }
        }

        free(data);
    }

    /* registra chordas ante processum relocationum */
    if (chordae_lon > 0) {
        int pos = 0;
        while (pos < chordae_lon) {
            if (num_chordarum >= MAX_CHORDAE_LIT)
                erratum("nimis multae chordae litterales in ligatione");
            chordae[num_chordarum].data      = (char *)&chordae_data[pos];
            int slon = (int)strlen((char *)&chordae_data[pos]);
            chordae[num_chordarum].longitudo = slon + 1;
            chordae[num_chordarum].offset    = pos;
            num_chordarum++;
            pos += slon + 1;
        }
    }

    /* registra symbola communia (globales) */
    for (int i = 0; i < num_liga_sym; i++) {
        if (!liga_syms[i].est_communis)
            continue;
        /* nomen cum _ praefixo — remove praefixum */
        const char *nom = liga_syms[i].nomen;
        if (nom[0] == '_')
            nom++;
        int mag = liga_syms[i].magnitudo_communis;
        if (num_globalium >= MAX_GLOBALES)
            erratum("nimis multae globales in ligatione");
        int gid = num_globalium++;
        strncpy(globales[gid].nomen, nom, 255);
        globales[gid] .typus       = NULL;
        globales[gid] .magnitudo   = mag;
        globales[gid] .colineatio  = mag >= 8 ? 8 : (mag >= 4 ? 4 : 1);
        globales[gid] .est_bss     = 1;
        globales[gid] .est_staticus     = 0;
        globales[gid] .valor_initialis  = 0;
        globales[gid] .habet_valorem    = 0;
        liga_syms[i]  .globalis_index = gid;
    }

    /* resolvere relocātiōnēs dātōrum dīlātās ad externa indefīnīta */
    for (int i = 0; i < num_data_relocs_dilatae; i++) {
        liga_data_reloc_t *d = &data_relocs_dilatae[i];
        int sid = liga_sym_quaere(d->sym_nomen);
        if (sid < 0 || !liga_syms[sid].definita) {
            if (
                sid >= 0 && liga_syms[sid].est_communis
                && liga_syms[sid].globalis_index >= 0
            ) {
                /* symbolum commune — in __bss/globales */
                int gid = liga_syms[sid].globalis_index;
                /* BSS offset scītur tantum in scribo_macho; ūtere
                 * DR_IDATA sī data_offset est assignātum, aliter erratum */
                if (!globales[gid].est_bss) {
                    data_reloc_adde(
                        d->patch_off, DR_IDATA, globales[gid].data_offset
                    );
                    continue;
                }
            }
            /* externum dylib in dātīs — dyld ligābit post onerātiōnem */
            if (biblio_num_dylib() > 0) {
                uint64_t zero = 0;
                memcpy(init_data + d->patch_off, &zero, 8);
                data_bind_adde(d->patch_off, d->sym_nomen);
                continue;
            }
            erratum(
                "symbolum '%s' in relocātiōne dātōrum nōn resolūtum",
                d->sym_nomen
            );
        }
        int sym_oi   = liga_syms[sid].obiectum;
        int sym_sect = liga_syms[sid].sectio;
        if (sym_sect == 1) {
            data_reloc_adde(
                d->patch_off, DR_TEXT, liga_syms[sid].valor_globalis
            );
        } else if (sym_sect > 0 && sym_sect < MAX_SECT) {
            int tsi = sym_oi * MAX_SECT + sym_sect;
            int sg  = sect_genera[tsi];
            if (sg == 1) {
                data_reloc_adde(
                    d->patch_off, DR_TEXT, liga_syms[sid].valor_globalis
                );
            } else if (sg == 2) {
                int cstr_target = cstr_bases[sym_oi]
                    + liga_syms[sid].valor - cstr_vmaddrs[sym_oi];
                data_reloc_adde(d->patch_off, DR_CSTRING, cstr_target);
            } else if (sg == 3) {
                int idata_target = sect_idata_bases[tsi]
                    + liga_syms[sid].valor - sect_idata_vmas[tsi];
                data_reloc_adde(d->patch_off, DR_IDATA, idata_target);
            } else {
                erratum(
                    "symbolum '%s' in sectione ignota (genus %d)"
                    " prō relocātiōne dātōrum",
                    d->sym_nomen, sg
                );
            }
        } else {
            erratum(
                "symbolum '%s' sine sectione prō relocātiōne dātōrum",
                d->sym_nomen
            );
        }
    }

    /* === passus secundus: processa relocationes === */

    /* tabula truncorum pro BL ad externos */
    int stub_offsets[MAX_GOT];
    for (int i = 0; i < MAX_GOT; i++)
        stub_offsets[i] = -1;

    for (int ri = 0; ri < num_relocs; ri++) {
        liga_reloc_t *r = &relocs[ri];
        int inst_off = codex_bases[r->obiectum] + r->r_addr;
        int oi       = r->obiectum;

        if (!r->r_extern) {
            /* relocatio non-externa — sectio-relativa */
            if (
                cstr_sectnums[oi] > 0 && r->r_sym == cstr_sectnums[oi]
                && (r->r_type == 3 || r->r_type == 4)
            ) {
                /* PAGE21/PAGEOFF12 ad __cstring — crea fixup pro scribo_macho */

                /* decode ADRP+ADD ut chordam inveniamus */
                int adrp_off = inst_off;
                int add_off  = inst_off + 4;
                if (r->r_type == 4) {
                    adrp_off = inst_off - 4;
                    add_off  = inst_off;
                }
                uint32_t adrp_inst;
                memcpy(&adrp_inst, codex + adrp_off, 4);
                int immhi = ((int)adrp_inst >> 5) & 0x7FFFF;
                int immlo = ((int)adrp_inst >> 29) & 3;
                int imm21 = (immhi << 2) | immlo;
                if (imm21 & 0x100000)
                    imm21 |= (int)0xFFE00000;
                int64_t old_page_delta = (int64_t)imm21 << 12;

                uint32_t add_inst;
                memcpy(&add_inst, codex + add_off, 4);
                int old_lo12 = ((int)add_inst >> 10) & 0xFFF;

                int old_pc = r->r_addr;
                if (r->r_type == 4)
                    old_pc -= 4;
                int old_target      = (int)((old_pc & ~0xFFF) + old_page_delta) + old_lo12;
                /* Sectiōnēs nōn necessāriō contiguae: phantasma.o cum
                 * sectiōnibus __objc_* interpositīs habet __cstring ad
                 * VMADDR > __text size. Ūtere vmaddr sectiōnis __cstring. */
                int str_off_in_cstr = old_target - cstr_vmaddrs[oi];
                int global_cstr_off = cstr_bases[oi] + str_off_in_cstr;

                /* quaere indicem chordae */
                int cid = -1;
                for (int ci = 0; ci < num_chordarum; ci++) {
                    if (chordae[ci].offset == global_cstr_off) {
                        cid = ci;
                        break;
                    }
                }
                if (cid < 0) {
                    /* chorda non exacte in initio — quaere proximam */
                    for (int ci = 0; ci < num_chordarum; ci++) {
                        if (
                            chordae[ci].offset <= global_cstr_off &&
                            global_cstr_off < chordae[ci].offset + chordae[ci].longitudo
                        ) {
                            cid = ci;
                            break;
                        }
                    }
                }

                if (cid >= 0) {
                    if (r->r_type == 3)
                        fixup_adde(FIX_ADRP, adrp_off, cid, 0);
                    else
                        fixup_adde(FIX_ADD_LO12, add_off, cid, 0);
                }
                continue;
            }
            /* PAGE21/PAGEOFF12 non-extern ad aliam sectiōnem dātōrum
             * (__data, __bss, __const, __objc_*). Clang ēmittit
             * ADRP+ADD prō accessū variābilis staticae: ūtere
             * addendum ex ipsā īnstrūctiōne et repatcha ad offsetum
             * in init_data. */
            if (
                r->r_sym > 0 && r->r_sym < MAX_SECT
                && (r->r_type == 3 || r->r_type == 4)
            ) {
                int tsi = oi * MAX_SECT + r->r_sym;
                int sg  = sect_genera[tsi];
                if (sg != 3)
                    continue;
                int adrp_off = inst_off;
                int add_off  = inst_off + 4;
                if (r->r_type == 4) {
                    adrp_off = inst_off - 4;
                    add_off  = inst_off;
                }
                uint32_t adrp_inst, add_inst;
                memcpy(&adrp_inst, codex + adrp_off, 4);
                memcpy(&add_inst,  codex + add_off, 4);
                int immhi = ((int)adrp_inst >> 5) & 0x7FFFF;
                int immlo = ((int)adrp_inst >> 29) & 3;
                int imm21 = (immhi << 2) | immlo;
                if (imm21 & 0x100000)
                    imm21 |= (int)0xFFE00000;
                int64_t old_page_delta = (int64_t)imm21 << 12;
                int old_lo12 = ((int)add_inst >> 10) & 0xFFF;
                int old_pc   = r->r_addr;
                if (r->r_type == 4)
                    old_pc -= 4;
                int old_target
                    = (int)((old_pc & ~0xFFF) + old_page_delta) + old_lo12;
                int off_in_sect = old_target - sect_idata_vmas[tsi];
                int idata_off   = sect_idata_bases[tsi] + off_in_sect;
                if (r->r_type == 3)
                    fixup_adde(FIX_ADRP_IDATA, adrp_off, idata_off, 0);
                else
                    fixup_adde(FIX_ADD_LO12_IDATA, add_off, idata_off, 0);
                continue;
            }
            continue;
        }

        /* relocatio externa ad __cstring symbolum */
        if (
            cstr_sectnums[oi] > 0 && r->sym_sect == cstr_sectnums[oi] &&
            (r->r_type == 3 || r->r_type == 4)
        ) {
            /* symbolum locale in __cstring — computa indicem chordae */
            int str_off_in_cstr = r->sym_valor - cstr_vmaddrs[oi];
            int global_cstr_off = cstr_bases[oi] + str_off_in_cstr;

            int cid = -1;
            for (int ci = 0; ci < num_chordarum; ci++) {
                if (chordae[ci].offset == global_cstr_off) {
                    cid = ci;
                    break;
                }
            }
            if (cid < 0) {
                for (int ci = 0; ci < num_chordarum; ci++) {
                    if (
                        chordae[ci].offset <= global_cstr_off &&
                        global_cstr_off < chordae[ci].offset + chordae[ci].longitudo
                    ) {
                        cid = ci;
                        break;
                    }
                }
            }
            if (cid >= 0) {
                if (r->r_type == 3)
                    fixup_adde(FIX_ADRP, inst_off, cid, 0);
                else
                    fixup_adde(FIX_ADD_LO12, inst_off, cid, 0);
            }
            continue;
        }

        /* relocatio externa */
        int sid = liga_sym_quaere(r->sym_nomen);

        if (sid >= 0 && liga_syms[sid].est_communis) {
            /* symbolum commune — variabilis globalis → fixup ad BSS */
            int gid = liga_syms[sid].globalis_index;
            if (gid >= 0) {
                if (r->r_type == 3)       /* PAGE21 */
                    fixup_adde(FIX_ADRP_DATA, inst_off, gid, 0);
                else if (r->r_type == 4)  /* PAGEOFF12 */
                    fixup_adde(FIX_ADD_LO12_DATA, inst_off, gid, 0);
                else if (r->r_type == 5)  /* GOT_LOAD_PAGE21 → ADRP ad BSS */
                    fixup_adde(FIX_ADRP_DATA, inst_off, gid, 0);
                else if (r->r_type == 6) {
                    /* GOT_LOAD_PAGEOFF12 → converte LDR in ADD */
                    uint32_t inst;
                    memcpy(&inst, codex + inst_off, 4);
                    int rd = inst & 0x1F;
                    int rn = (inst >> 5) & 0x1F;
                    /* rescribe ut ADD Xd, Xn, #0 (fixup implebit lo12) */
                    inst = 0x91000000 | (rn << 5) | rd;
                    memcpy(codex + inst_off, &inst, 4);
                    fixup_adde(FIX_ADD_LO12_DATA, inst_off, gid, 0);
                }
            }
        } else if (sid >= 0 && liga_syms[sid].definita) {
            int sym_oi   = liga_syms[sid].obiectum;
            int sym_sect = liga_syms[sid].sectio;
            int sg = (sym_sect > 0 && sym_sect < MAX_SECT)
                ? sect_genera[sym_oi * MAX_SECT + sym_sect] : 0;

            if (sg == 3) {
                /* symbolum in sectione dati (__data, __const, __bss) */
                int si_idx    = sym_oi * MAX_SECT + sym_sect;
                int off       = liga_syms[sid].valor - sect_idata_vmas[si_idx];
                int idata_off = sect_idata_bases[si_idx] + off + r->addendum;

                if (r->r_type == 3 || r->r_type == 5) {
                    fixup_adde(FIX_ADRP_IDATA, inst_off, idata_off, 0);
                } else if (r->r_type == 4 || r->r_type == 6) {
                    if (r->r_type == 6) {
                        /* GOT_LOAD_PAGEOFF12 → converte LDR in ADD */
                        uint32_t ldr_inst;
                        memcpy(&ldr_inst, codex + inst_off, 4);
                        int rd = ldr_inst & 0x1F;
                        int rn = (ldr_inst >> 5) & 0x1F;
                        uint32_t add_inst = 0x91000000 | (rn << 5) | rd;
                        memcpy(codex + inst_off, &add_inst, 4);
                    }
                    fixup_adde(FIX_ADD_LO12_IDATA, inst_off, idata_off, 0);
                }
            } else if (sg == 1) {
                /* symbolum in __text */
                int target = liga_syms[sid].valor_globalis;

                if (r->r_type == 2) {
                    patch_branch26(inst_off, target);
                } else if (r->r_type == 3) {
                    fixup_adde(FIX_ADRP_TEXT, inst_off, target, 0);
                } else if (r->r_type == 4) {
                    fixup_adde(FIX_ADD_LO12_TEXT, inst_off, target, 0);
                } else if (r->r_type == 5) {
                    fixup_adde(FIX_ADRP_TEXT, inst_off, target, 0);
                } else if (r->r_type == 6) {
                    uint32_t ldr_inst;
                    memcpy(&ldr_inst, codex + inst_off, 4);
                    int rd = ldr_inst & 0x1F;
                    int rn = (ldr_inst >> 5) & 0x1F;
                    uint32_t add_inst = 0x91000000 | (rn << 5) | rd;
                    memcpy(codex + inst_off, &add_inst, 4);
                    fixup_adde(FIX_ADD_LO12_TEXT, inst_off, target, 0);
                }
            } else {
                erratum(
                    "symbolum '%s' in sectione ignota (genus %d)",
                    r->sym_nomen, sg
                );
            }
        } else if (sid < 0 && r->sym_sect > 0) {
            /* symbolum locale (non N_EXT) in obiecto — resolve per sectionem */
            int sym_sect_num = r->sym_sect;

            if (sym_sect_num == 1) {
                /* in __text */
                int target = codex_bases[oi] + r->sym_valor;

                if (r->r_type == 2) {
                    patch_branch26(inst_off, target);
                } else if (r->r_type == 3 || r->r_type == 5) {
                    fixup_adde(FIX_ADRP_TEXT, inst_off, target, 0);
                } else if (r->r_type == 4 || r->r_type == 6) {
                    if (r->r_type == 6) {
                        uint32_t li;
                        memcpy(&li, codex + inst_off, 4);
                        int rd      = li & 0x1F;
                        int rn      = (li >> 5) & 0x1F;
                        uint32_t ai = 0x91000000 | (rn << 5) | rd;
                        memcpy(codex + inst_off, &ai, 4);
                    }
                    fixup_adde(FIX_ADD_LO12_TEXT, inst_off, target, 0);
                } else {
                    erratum(
                        "relocatio non sustenta (typus %d) ad '%s' in __text",
                        r->r_type, r->sym_nomen
                    );
                }
            } else {
                /* in __const, __data, vel __bss — quaere in tabula sectionum */
                if (sym_sect_num >= MAX_SECT)
                    erratum(
                        "symbolum '%s' in sectione %d ultra limitem",
                        r->sym_nomen, sym_sect_num
                    );
                int si_idx = oi * MAX_SECT + sym_sect_num;
                int sg     = sect_genera[si_idx];
                if (sg != 3)
                    erratum(
                        "symbolum '%s' in sectione ignota %d (genus %d)",
                        r->sym_nomen, sym_sect_num, sg
                    );
                int idata_off = sect_idata_bases[si_idx]
                    + (r->sym_valor - sect_idata_vmas[si_idx])
                    + r->addendum;

                if (r->r_type == 3 || r->r_type == 5) {
                    fixup_adde(FIX_ADRP_IDATA, inst_off, idata_off, 0);
                } else if (r->r_type == 4 || r->r_type == 6) {
                    if (r->r_type == 6) {
                        uint32_t li;
                        memcpy(&li, codex + inst_off, 4);
                        int rd      = li & 0x1F;
                        int rn      = (li >> 5) & 0x1F;
                        uint32_t ai = 0x91000000 | (rn << 5) | rd;
                        memcpy(codex + inst_off, &ai, 4);
                    }
                    fixup_adde(FIX_ADD_LO12_IDATA, inst_off, idata_off, 0);
                } else {
                    erratum(
                        "relocatio non sustenta (typus %d) ad '%s' in datis",
                        r->r_type, r->sym_nomen
                    );
                }
            }
        } else {
            /* symbolum externum — adde ad GOT */

            /* ObjC ABI recēns: _objc_msgSend$<sel> → fabricā stubum
             * specializātum quī SEL registrat et _objc_msgSend vocat */
            if (
                r->r_type == 2 &&
                strncmp(r->sym_nomen, "_objc_msgSend$", 14) == 0
            ) {
                int stub_off = objc_msgsend_stub_adde(r->sym_nomen);
                patch_branch26(inst_off, stub_off);
                continue;
            }

            int gid = got_adde(r->sym_nomen);

            if (r->r_type == 5)
                fixup_adde(FIX_ADRP_GOT, inst_off, gid, 0);
            else if (r->r_type == 6)
                fixup_adde(FIX_LDR_GOT_LO12, inst_off, gid, 8);
            else if (r->r_type == 2) {
                /* BRANCH26 ad externum — crea truncum (stub) */
                if (stub_offsets[gid] < 0) {
                    if (codex_lon + 12 > MAX_CODEX)
                        erratum("codex nimis longus (truncus)");
                    stub_offsets[gid] = codex_lon;
                    /* ADRP x16, GOT@PAGE */
                    uint32_t adrp = 0x90000000 | 16;
                    memcpy(codex + codex_lon, &adrp, 4);
                    fixup_adde(FIX_ADRP_GOT, codex_lon, gid, 0);
                    codex_lon += 4;
                    /* LDR x16, [x16, GOT@PAGEOFF] */
                    uint32_t ldr = 0xF9400210;
                    memcpy(codex + codex_lon, &ldr, 4);
                    fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
                    codex_lon += 4;
                    /* BR x16 */
                    uint32_t br = 0xD61F0200;
                    memcpy(codex + codex_lon, &br, 4);
                    codex_lon += 4;
                }
                /* patch B/BL ut ad truncum saliat */
                patch_branch26(inst_off, stub_offsets[gid]);
            }
        }
    }

    free(relocs);

    /* quaere _main */
    int main_offset = -1;
    int mid         = liga_sym_quaere("_main");
    if (mid >= 0 && liga_syms[mid].definita)
        main_offset = liga_syms[mid].valor_globalis;
    if (main_offset < 0)
        erratum("symbolum _main non inventum in obiectis");

    /* si ___ccc_init adest, genera involucrum quod eam vocat ante _main */
    int iid = liga_sym_quaere("___ccc_init");
    if (iid >= 0 && liga_syms[iid].definita) {
        int init_target = liga_syms[iid].valor_globalis;
        int involucrum  = codex_lon;
        /* STP x29, x30, [sp, #-32]! */
        emit_stp_pre(FP, LR, SP, -32);
        /* MOV x29, sp */
        emit_addi(FP, SP, 0);
        /* STP x19, x20, [x29, #16] — serva argc/argv */
        emit_stp_pre(19, 20, FP, 16);
        emit_mov(19, 0); /* x19 = argc */
        emit_mov(20, 1); /* x20 = argv */
        /* BL __ccc_init */
        int delta_init = (init_target - codex_lon) / 4;
        emit32(0x94000000 | (delta_init & 0x3FFFFFF));
        /* restitue argc/argv */
        emit_mov(0, 19);
        emit_mov(1, 20);
        /* BL _main */
        int delta_main = (main_offset - codex_lon) / 4;
        emit32(0x94000000 | (delta_main & 0x3FFFFFF));
        /* LDP x19, x20, [x29, #16] */
        emit_ldp_post(19, 20, FP, 16);
        /* LDP x29, x30, [sp], #32 */
        emit_ldp_post(FP, LR, SP, 32);
        emit_ret();
        main_offset = involucrum;
    }

    free(codex_bases);
    free(cstr_bases);
    free(data_bases);
    free(text_mags);
    free(cstr_sectnums);
    free(data_sectnums);
    free(cstr_vmaddrs);
    free(data_vmaddrs);
    free(sect_genera);
    free(sect_idata_bases);
    free(sect_idata_vmas);

    /* scribo executabile */
    scribo_macho(plica_exitus, main_offset);
}
