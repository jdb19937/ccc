/*
 * liga.c — ligator objectorum Mach-O
 *
 * Legit plicas MH_OBJECT a ccc generatas, componit codicem,
 * resolvit symbola, et scribit executabile per scribo_macho.
 */

#include "ccc.h"
#include "emitte.h"
#include "scribo.h"

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
    char sym_nomen[260];
} liga_reloc_t;

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
    liga_syms[id].definita         = 0;
    liga_syms[id].valor_globalis   = -1;
    liga_syms[id].est_communis     = 0;
    liga_syms[id].magnitudo_communis = 0;
    liga_syms[id].globalis_index   = -1;
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
 * ligatio
 * ================================================================ */

void liga_objecta(int num_obj, const char **viae, const char *plica_exitus)
{
    emitte_initia();
    num_liga_sym = 0;

    /* relocationes dilatae */
    liga_reloc_t *relocs = NULL;
    int num_relocs       = 0;
    int cap_relocs       = 0;

    int *codex_bases = malloc(num_obj * sizeof(int));
    int *cstr_bases  = malloc(num_obj * sizeof(int));
    int *text_mags   = malloc(num_obj * sizeof(int));
    if (!codex_bases || !cstr_bases || !text_mags)
        erratum("memoria exhausta");

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

        int text_off   = 0, text_mag = 0;
        int cstr_off   = 0, cstr_mag = 0;
        int reloc_off  = 0, nrelocs = 0;
        int symtab_off = 0, nsyms_obj = 0;
        int strtab_off = 0;

        for (uint32_t ci = 0; ci < ncmds; ci++) {
            uint32_t cmd  = lege32(data + cmd_off);
            uint32_t csiz = lege32(data + cmd_off + 4);

            if (cmd == LC_SEGMENT_64) {
                uint32_t nsects = lege32(data + cmd_off + 64);
                int sect_off    = cmd_off + 72;
                for (uint32_t si = 0; si < nsects; si++) {
                    char sectname[17] = {0};
                    memcpy(sectname, data + sect_off, 16);
                    uint64_t s_size   = lege64(data + sect_off + 40);
                    uint32_t s_off    = lege32(data + sect_off + 48);
                    uint32_t s_reloff = lege32(data + sect_off + 56);
                    uint32_t s_nreloc = lege32(data + sect_off + 60);

                    if (strcmp(sectname, "__text") == 0) {
                        text_off  = s_off;
                        text_mag  = (int)s_size;
                        reloc_off = s_reloff;
                        nrelocs   = s_nreloc;
                    } else if (strcmp(sectname, "__cstring") == 0) {
                        cstr_off = s_off;
                        cstr_mag = (int)s_size;
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

        codex_bases[oi] = codex_lon;
        text_mags[oi]   = text_mag;
        cstr_bases[oi]  = chordae_lon;

        if (text_mag > 0) {
            if (codex_lon + text_mag > MAX_CODEX)
                erratum("codex nimis magnus in ligatione");
            memcpy(codex + codex_lon, data + text_off, text_mag);
            codex_lon += text_mag;
        }

        if (cstr_mag > 0) {
            memcpy(chordae_data + chordae_lon, data + cstr_off, cstr_mag);
            chordae_lon += cstr_mag;
        }

        /* lege symbola */
        for (int si = 0; si < nsyms_obj; si++) {
            int noff         = symtab_off + si * 16;
            uint32_t n_strx  = lege32(data + noff);
            uint8_t  n_type  = data[noff + 4];
            uint8_t  n_sect  = data[noff + 5];
            uint64_t n_value = lege64(data + noff + 8);

            const char *nomen = (const char *)(data + strtab_off + n_strx);
            int id = liga_sym_adde(nomen);

            if ((n_type & 0x0E) == N_SECT) {
                liga_syms[id].definita       = 1;
                liga_syms[id].sectio         = n_sect;
                liga_syms[id].valor          = (int)n_value;
                liga_syms[id].obiectum       = oi;
                liga_syms[id].valor_globalis = codex_bases[oi] + (int)n_value;
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

        /* collige relocationes (processa in passu secundo) */
        for (int ri = 0; ri < nrelocs; ri++) {
            int roff        = reloc_off + ri * 8;
            uint32_t r_addr = lege32(data + roff);
            uint32_t r_info = lege32(data + roff + 4);

            int r_ext  = (r_info >> 27) & 1;
            int r_sym  = r_info & 0xFFFFFF;
            int r_type = (r_info >> 28) & 0xF;

            /* serva relocationem */
            if (num_relocs >= cap_relocs) {
                cap_relocs = cap_relocs ? cap_relocs * 2 : 64;
                relocs     = realloc(relocs, cap_relocs * sizeof(liga_reloc_t));
                if (!relocs)
                    erratum("memoria exhausta");
            }
            relocs[num_relocs].obiectum = oi;
            relocs[num_relocs].r_addr   = (int)r_addr;
            relocs[num_relocs].r_sym    = r_sym;
            relocs[num_relocs].r_type   = r_type;
            relocs[num_relocs].r_extern = r_ext;
            if (r_ext) {
                int sym_noff      = symtab_off + r_sym * 16;
                uint32_t sym_strx = lege32(data + sym_noff);
                const char *sym_nomen = (const char *)(data + strtab_off + sym_strx);
                strncpy(relocs[num_relocs].sym_nomen, sym_nomen, 259);
            } else {
                relocs[num_relocs].sym_nomen[0] = '\0';
            }
            num_relocs++;
        }

        free(data);
    }

    /* registra chordas ante processum relocationum */
    if (chordae_lon > 0) {
        int pos = 0;
        while (pos < chordae_lon && num_chordarum < MAX_CHORDAE_LIT) {
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
        globales[gid].typus       = NULL;
        globales[gid].magnitudo   = mag;
        globales[gid].colineatio  = mag >= 8 ? 8 : (mag >= 4 ? 4 : 1);
        globales[gid].est_bss     = 1;
        globales[gid].est_staticus     = 0;
        globales[gid].valor_initialis  = 0;
        globales[gid].habet_valorem    = 0;
        liga_syms[i].globalis_index = gid;
    }

    /* === passus secundus: processa relocationes === */

    for (int ri = 0; ri < num_relocs; ri++) {
        liga_reloc_t *r = &relocs[ri];
        int inst_off    = codex_bases[r->obiectum] + r->r_addr;
        int oi          = r->obiectum;

        if (!r->r_extern) {
            /* relocatio non-externa — sectio-relativa */
            if (r->r_sym == 2 && (r->r_type == 3 || r->r_type == 4)) {
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
                int str_off_in_cstr = old_target - text_mags[oi];
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
            int target = liga_syms[sid].valor_globalis;

            if (r->r_type == 2) {
                /* ARM64_RELOC_BRANCH26 — BL */
                int delta     = (target - inst_off) / 4;
                uint32_t inst = 0x94000000 | (delta & 0x3FFFFFF);
                memcpy(codex + inst_off, &inst, 4);
            } else if (r->r_type == 3) {
                /* PAGE21 ad symbolum definitum in codice (raro) */
                /* NOP — adresse resolvitur per BL */
                uint32_t nop = 0xD503201F;
                memcpy(codex + inst_off, &nop, 4);
            } else if (r->r_type == 4) {
                /* PAGEOFF12 ad symbolum definitum in codice (raro) */
                uint32_t nop = 0xD503201F;
                memcpy(codex + inst_off, &nop, 4);
            } else if (r->r_type == 5 || r->r_type == 6) {
                /* GOT_LOAD_PAGE21/PAGEOFF12 ad symbolum definitum:
                 * ADRP → NOP, LDR → NOP, BLR → BL */
                uint32_t nop = 0xD503201F;
                memcpy(codex + inst_off, &nop, 4);
                if (r->r_type == 6) {
                    int blr_off = inst_off + 4;
                    uint32_t blr;
                    memcpy(&blr, codex + blr_off, 4);
                    if ((blr & 0xFFFFFC1F) == 0xD63F0000) {
                        int delta   = (target - blr_off) / 4;
                        uint32_t bl = 0x94000000 | (delta & 0x3FFFFFF);
                        memcpy(codex + blr_off, &bl, 4);
                    }
                }
            }
        } else {
            /* symbolum externum — adde ad GOT */
            int gid = got_adde(r->sym_nomen);

            if (r->r_type == 5)
                fixup_adde(FIX_ADRP_GOT, inst_off, gid, 0);
            else if (r->r_type == 6)
                fixup_adde(FIX_LDR_GOT_LO12, inst_off, gid, 8);
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
    free(text_mags);

    /* scribo executabile */
    scribo_macho(plica_exitus, main_offset);
}
