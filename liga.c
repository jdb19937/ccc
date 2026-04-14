/*
 * liga.c — ligator objectorum Mach-O
 *
 * Legit plicas MH_OBJECT a ccc generatas, componit codicem,
 * resolvit symbola, et scribit executabile per scribo_macho.
 */

#include "ccc.h"
#include "genera.h"
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
} liga_sym_t;

/* relocatio dilata — servatur pro passu secundo */
typedef struct {
    int  obiectum;
    int  r_addr;
    int  r_sym;         /* index symboli in obiecto */
    int  r_type;
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
    liga_syms[id].definita       = 0;
    liga_syms[id].valor_globalis = -1;
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
    if (!codex_bases)
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
            uint64_t n_value = lege64(data + noff + 8);

            const char *nomen = (const char *)(data + strtab_off + n_strx);
            int id = liga_sym_adde(nomen);

            if ((n_type & 0x0E) == N_SECT) {
                liga_syms[id].definita       = 1;
                liga_syms[id].sectio         = 1;
                liga_syms[id].valor          = (int)n_value;
                liga_syms[id].obiectum       = oi;
                liga_syms[id].valor_globalis = codex_bases[oi] + (int)n_value;
            }
        }

        /* collige relocationes (processa in passu secundo) */
        for (int ri = 0; ri < nrelocs; ri++) {
            int roff        = reloc_off + ri * 8;
            uint32_t r_addr = lege32(data + roff);
            uint32_t r_info = lege32(data + roff + 4);

            int r_extern = (r_info >> 27) & 1;
            if (!r_extern)
                continue;

            int r_sym  = r_info & 0xFFFFFF;
            int r_type = (r_info >> 28) & 0xF;

            /* resolve nomen symboli */
            int sym_noff      = symtab_off + r_sym * 16;
            uint32_t sym_strx = lege32(data + sym_noff);
            const char *sym_nomen = (const char *)(data + strtab_off + sym_strx);

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
            strncpy(relocs[num_relocs].sym_nomen, sym_nomen, 259);
            num_relocs++;
        }

        free(data);
    }

    /* === passus secundus: processa relocationes === */

    for (int ri = 0; ri < num_relocs; ri++) {
        liga_reloc_t *r = &relocs[ri];
        int inst_off    = codex_bases[r->obiectum] + r->r_addr;
        int sid         = liga_sym_quaere(r->sym_nomen);

        if (sid >= 0 && liga_syms[sid].definita) {
            int target = liga_syms[sid].valor_globalis;

            if (r->r_type == 2) {
                /* ARM64_RELOC_BRANCH26 — BL */
                int delta     = (target - inst_off) / 4;
                uint32_t inst = 0x94000000 | (delta & 0x3FFFFFF);
                memcpy(codex + inst_off, &inst, 4);
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

    /* registra chordas */
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

    /* quaere _main */
    int main_offset = -1;
    int mid         = liga_sym_quaere("_main");
    if (mid >= 0 && liga_syms[mid].definita)
        main_offset = liga_syms[mid].valor_globalis;
    if (main_offset < 0)
        erratum("symbolum _main non inventum in obiectis");

    free(codex_bases);

    /* scribo executabile */
    scribo_macho(plica_exitus, main_offset);
}
