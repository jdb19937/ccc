/*
 * liga.c — ligator objectorum Mach-O
 *
 * Legit plicas MH_OBJECT a ccc generatas, componit codicem,
 * resolvit symbola, et scribit executabile per scribo_macho.
 */

#include "utilia.h"
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
    int  sym_sect;      /* sectio symboli (pro externis chordis) */
    int  sym_valor;     /* n_value symboli (pro externis chordis) */
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
                    uint32_t s_reloff = lege32(data + sect_off + 56);
                    uint32_t s_nreloc = lege32(data + sect_off + 60);
                    sect_counter++; /* Mach-O sections 1-indexed */

                    /* registra in tabula sectionum */
                    if (sect_counter < MAX_SECT) {
                        int si_idx = oi * MAX_SECT + sect_counter;
                        sect_idata_vmas[si_idx] = (int)s_addr;
                        sect_file_offs[sect_counter] = s_off;
                        sect_mags[sect_counter] = (int)s_size;
                        sect_reloc_offs[sect_counter] = s_reloff;
                        sect_nrelocs[sect_counter] = s_nreloc;
                    }

                    if (strcmp(sectname, "__text") == 0) {
                        text_off  = s_off;
                        text_mag  = (int)s_size;
                        reloc_off = s_reloff;
                        nrelocs   = s_nreloc;
                        if (sect_counter < MAX_SECT)
                            sect_genera[oi * MAX_SECT + sect_counter] = 1;
                    } else if (strcmp(sectname, "__cstring") == 0) {
                        cstr_off     = s_off;
                        cstr_mag     = (int)s_size;
                        cstr_sectnum = sect_counter;
                        cstr_vma     = (int)s_addr;
                        if (sect_counter < MAX_SECT)
                            sect_genera[oi * MAX_SECT + sect_counter] = 2;
                    } else if (strcmp(sectname, "__data") == 0) {
                        idata_off    = s_off;
                        idata_mag    = (int)s_size;
                        data_sectnum = sect_counter;
                        data_vma     = (int)s_addr;
                        if (sect_counter < MAX_SECT)
                            sect_genera[oi * MAX_SECT + sect_counter] = 3;
                    } else if (strcmp(sectname, "__const") == 0) {
                        if (sect_counter < MAX_SECT)
                            sect_genera[oi * MAX_SECT + sect_counter] = 3;
                    } else if (
                        strcmp(sectname, "__bss") == 0
                        || strcmp(sectname, "__common") == 0
                    ) {
                        if (sect_counter < MAX_SECT) {
                            sect_genera[oi * MAX_SECT + sect_counter] = 3;
                            sect_is_bss[sect_counter] = 1;
                        }
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

        codex_bases[oi]   = codex_lon;
        text_mags[oi]     = text_mag;
        cstr_bases[oi]    = chordae_lon;
        data_bases[oi]    = init_data_lon;
        cstr_sectnums[oi] = cstr_sectnum;
        data_sectnums[oi] = data_sectnum;
        cstr_vmaddrs[oi]  = cstr_vma;
        data_vmaddrs[oi]  = data_vma;

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

        if (idata_mag > 0) {
            if (init_data_lon + idata_mag > MAX_DATA)
                erratum("data nimis magna in ligatione");
            memcpy(init_data + init_data_lon, data + idata_off, idata_mag);
            init_data_lon += idata_mag;
        }
        /* registra __data basis in tabula sectionum */
        if (data_sectnum > 0 && data_sectnum < MAX_SECT)
            sect_idata_bases[oi * MAX_SECT + data_sectnum] = data_bases[oi];

        /* __const et __bss sectiones → funde in init_data */
        for (int sn = 1; sn <= sect_counter && sn < MAX_SECT; sn++) {
            int si_idx = oi * MAX_SECT + sn;
            if (sect_genera[si_idx] != 3)
                continue;
            /* __data iam processata supra */
            if (data_sectnum > 0 && sn == data_sectnum)
                continue;
            sect_idata_bases[si_idx] = init_data_lon;
            if (sect_mags[sn] > 0) {
                if (init_data_lon + sect_mags[sn] > MAX_DATA)
                    erratum("data nimis magna in ligatione");
                if (sect_is_bss[sn])
                    memset(init_data + init_data_lon, 0, sect_mags[sn]);
                else
                    memcpy(
                        init_data + init_data_lon, data + sect_file_offs[sn],
                        sect_mags[sn]
                    );
                init_data_lon += sect_mags[sn];
            }
        }

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
                uint8_t  sym_nsect = data[sym_noff + 5];
                uint64_t sym_nval  = lege64(data + sym_noff + 8);
                const char *sym_nomen = (const char *)(data + strtab_off + sym_strx);
                strncpy(relocs[num_relocs].sym_nomen, sym_nomen, 259);
                relocs[num_relocs].sym_sect  = sym_nsect;
                relocs[num_relocs].sym_valor = (int)sym_nval;
            } else {
                relocs[num_relocs].sym_nomen[0] = '\0';
                relocs[num_relocs].sym_sect     = 0;
                relocs[num_relocs].sym_valor    = 0;
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
                    } else {
                        erratum(
                            "ARM64_RELOC_UNSIGNED nōn-externa: "
                            "sectiō %d nōn sustenta", sect_num
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

    /* tabula truncorum pro BL ad externos */
    int stub_offsets[MAX_GOT];
    for (int i = 0; i < MAX_GOT; i++)
        stub_offsets[i] = -1;

    for (int ri = 0; ri < num_relocs; ri++) {
        liga_reloc_t *r = &relocs[ri];
        int inst_off    = codex_bases[r->obiectum] + r->r_addr;
        int oi          = r->obiectum;

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
                int idata_off = sect_idata_bases[si_idx] + off;

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
                    /* ARM64_RELOC_BRANCH26 — BL */
                    int delta     = (target - inst_off) / 4;
                    uint32_t inst = 0x94000000 | (delta & 0x3FFFFFF);
                    memcpy(codex + inst_off, &inst, 4);
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
                    int delta     = (target - inst_off) / 4;
                    uint32_t inst = 0x94000000 | (delta & 0x3FFFFFF);
                    memcpy(codex + inst_off, &inst, 4);
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
                    + (r->sym_valor - sect_idata_vmas[si_idx]);

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
                /* patch BL ut ad truncum saliat */
                int delta     = (stub_offsets[gid] - inst_off) / 4;
                uint32_t inst = 0x94000000 | (delta & 0x3FFFFFF);
                memcpy(codex + inst_off, &inst, 4);
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
