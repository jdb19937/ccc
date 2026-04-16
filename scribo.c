/*
 * scribo.c — scriptor Mach-O executabilis
 *
 * Scribit plicam executabilem Mach-O ARM64 directe.
 * Post scripturam, codesign -s - vocat ad signandum.
 */

#include "utilia.h"
#include "emitte.h"
#include "scribo.h"
#include "func.h"
#include "biblio.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void scribe8(FILE *fp, uint8_t v) { fwrite(&v, 1, 1, fp); }
static void scribe16(FILE *fp, uint16_t v) { fwrite(&v, 2, 1, fp); }
static void scribe32(FILE *fp, uint32_t v) { fwrite(&v, 4, 1, fp); }
static void scribe64(FILE *fp, uint64_t v) { fwrite(&v, 8, 1, fp); }
static void scribe_impletio(FILE *fp, int n) {
    for (int i = 0; i < n; i++)
        scribe8(fp, 0);
}
static void scribe_chorda(FILE *fp, const char *s) {
    fwrite(s, 1, strlen(s) + 1, fp);
}
static void scribe_nomen_seg(FILE *fp, const char *nomen)
{
    char alveus[16] = {0};
    strncpy(alveus, nomen, 16);
    fwrite(alveus, 1, 16, fp);
}

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

static uint64_t allinea(uint64_t val, uint64_t col)
{
    return (val + col - 1) & ~(col - 1);
}

/* auxiliāria prō fixūrīs ADRP et ADD LO12 */

static uint32_t applica_adrp(uint32_t inst, uint64_t target_addr, uint64_t pc)
{
    int64_t page_delta = (int64_t)((target_addr & ~0xFFFULL) - (pc & ~0xFFFULL));
    int64_t imm = page_delta >> 12;
    int rd = inst & 0x1F;
    int immlo = (int)(imm & 3);
    int immhi = (int)((imm >> 2) & 0x7FFFF);
    return 0x90000000 | (immlo << 29) | (immhi << 5) | rd;
}

static uint32_t applica_add_lo12(uint32_t inst, uint64_t target_addr)
{
    int lo12 = (int)(target_addr & 0xFFF);
    int rd = inst & 0x1F;
    int rn = (inst >> 5) & 0x1F;
    return 0x91000000 | (lo12 << 10) | (rn << 5) | rd;
}

void scribo_macho(const char *plica_exitus, int main_offset)
{
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

    /* bibliothecae additionles ex -l (dylib solum) */
    int num_dylib_add = biblio_num_dylib();
    int *lc_dylib_add = NULL;
    if (num_dylib_add > 0) {
        lc_dylib_add = malloc(num_dylib_add * sizeof(int));
        if (!lc_dylib_add)
            erratum("memoria exhausta");
        for (int i = 0; i < num_dylib_add; i++) {
            const char *via = biblio_dylib_via(i);
            lc_dylib_add[i] = (int)allinea(24 + strlen(via) + 1, 8);
        }
    }

    int ncmds = 11 + num_dylib_add;
    int sizeofcmds = lc_pagezero + lc_text + lc_data + lc_linkedit +
        lc_dyld_info + lc_symtab + lc_dysymtab +
        lc_dylinker + lc_main + lc_build + lc_dylib;
    for (int i = 0; i < num_dylib_add; i++)
        sizeofcmds += lc_dylib_add[i];
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
    int idata_sect_off   = (int)allinea(got_lon, 8);
    int idata_sect_size  = init_data_lon;
    int bss_sect_off     = (int)allinea(idata_sect_off + idata_sect_size, 8);
    int data_file_size   = (int)allinea(idata_sect_off + idata_sect_size, PAGINA);
    if (data_file_size < PAGINA)
        data_file_size = PAGINA;
    int data_seg_vmsize = (int)allinea(bss_sect_off + bss_lon, PAGINA);
    if (data_seg_vmsize < PAGINA)
        data_seg_vmsize = PAGINA;

    int linkedit_fileoff = data_seg_fileoff + data_file_size;

    uint64_t text_vmaddr      = VM_BASIS;
    uint64_t text_sect_vmaddr = text_vmaddr + text_sect_offset;
    uint64_t cstring_vmaddr   = text_vmaddr + cstring_offset;
    uint64_t data_vmaddr      = text_vmaddr + text_seg_vmsize;
    uint64_t got_vmaddr       = data_vmaddr + got_sect_off;
    uint64_t idata_vmaddr     = data_vmaddr + idata_sect_off;
    uint64_t bss_vmaddr       = data_vmaddr + bss_sect_off;
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
                uint64_t ta = cstring_vmaddr + chordae[f->target].offset;
                inst = applica_adrp(inst, ta, text_sect_vmaddr + f->offset);
                break;
            }
        case FIX_ADD_LO12: {
                uint64_t ta = cstring_vmaddr + chordae[f->target].offset;
                inst = applica_add_lo12(inst, ta);
                break;
            }
        case FIX_ADRP_GOT: {
                uint64_t ta = got_vmaddr + f->target * 8;
                inst = applica_adrp(inst, ta, text_sect_vmaddr + f->offset);
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
                uint64_t ta = bss_vmaddr + globales[f->target].bss_offset;
                inst = applica_adrp(inst, ta, text_sect_vmaddr + f->offset);
                break;
            }
        case FIX_ADD_LO12_DATA: {
                uint64_t ta = bss_vmaddr + globales[f->target].bss_offset;
                inst = applica_add_lo12(inst, ta);
                break;
            }
        case FIX_ADRP_TEXT: {
                uint64_t ta = text_sect_vmaddr + f->target;
                inst = applica_adrp(inst, ta, text_sect_vmaddr + f->offset);
                break;
            }
        case FIX_ADD_LO12_TEXT: {
                uint64_t ta = text_sect_vmaddr + f->target;
                inst = applica_add_lo12(inst, ta);
                break;
            }
        case FIX_ADRP_IDATA: {
                uint64_t ta = idata_vmaddr + f->target;
                inst = applica_adrp(inst, ta, text_sect_vmaddr + f->offset);
                break;
            }
        case FIX_ADD_LO12_IDATA:
        case FIX_STR_LO12_IDATA:
        case FIX_LDR_LO12_IDATA: {
                uint64_t target_addr = idata_vmaddr + f->target;
                int lo12 = (int)(target_addr & 0xFFF);
                /* determina scaling ex instructione */
                int scale = 1;
                /* LDR/STR unsigned offset: bits 29:24 = 111001, size = bits 31:30 */
                if (((inst >> 24) & 0x3F) == 0x39)
                    scale = 1 << (inst >> 30);
                int rd      = inst & 0x1F;
                int rn      = (inst >> 5) & 0x1F;
                uint32_t op = inst & 0xFFC00000;
                inst        = op | ((lo12 / scale) << 10) | (rn << 5) | rd;
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
        if (biblio_num_dylib() > 0)
            /* dynamic lookup: dyld quaerit per omnia libraria onerata */
            bind_info[bind_lon++] = BIND_OPCODE_SET_DYLIB_SPECIAL_IMM | 0x0E;
        else
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
            if (bind_lon + slen + 3 > (int)sizeof(bind_info))
                erratum("bind info nimis magna (> %d octeti)", (int)sizeof(bind_info));
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
        int glen = strlen(got[i].nomen);
        if (strtab_lon + glen + 1 > (int)sizeof(strtab))
            erratum("strtab nimis magna (> %d octeti)", (int)sizeof(strtab));
        memcpy(&strtab[strtab_lon], got[i].nomen, glen + 1);
        strtab_lon += glen + 1;
        nl->n_type  = N_EXT; /* external, undefined */
        nl->n_sect  = 0;
        nl->n_desc  = biblio_num_dylib() > 0 ? 0xFE00 : 0x0100;
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
    scribe32(fp, MH_MAGIC_64);
    scribe32(fp, CPU_TYPE_ARM64);
    scribe32(fp, CPU_SUBTYPE_ALL);
    scribe32(fp, MH_EXECUTE);
    scribe32(fp, ncmds);
    scribe32(fp, sizeofcmds);
    scribe32(fp, MH_PIE | MH_DYLDLINK | MH_TWOLEVEL);
    scribe32(fp, 0); /* reserved */

    /* --- LC_SEGMENT_64 __PAGEZERO --- */
    scribe32(fp, LC_SEGMENT_64);
    scribe32(fp, lc_pagezero);
    scribe_nomen_seg(fp, "__PAGEZERO");
    scribe64(fp, 0);                 /* vmaddr */
    scribe64(fp, VM_BASIS);          /* vmsize = 4GB */
    scribe64(fp, 0);                 /* fileoff */
    scribe64(fp, 0);                 /* filesize */
    scribe32(fp, VM_PROT_NONE);      /* maxprot */
    scribe32(fp, VM_PROT_NONE);      /* initprot */
    scribe32(fp, 0);                 /* nsects */
    scribe32(fp, 0);                 /* flags */

    /* --- LC_SEGMENT_64 __TEXT --- */
    scribe32(fp, LC_SEGMENT_64);
    scribe32(fp, lc_text);
    scribe_nomen_seg(fp, "__TEXT");
    scribe64(fp, text_vmaddr);
    scribe64(fp, text_seg_vmsize);
    scribe64(fp, 0);
    scribe64(fp, text_seg_filesize);
    scribe32(fp, VM_PROT_READ | VM_PROT_EXECUTE);
    scribe32(fp, VM_PROT_READ | VM_PROT_EXECUTE);
    scribe32(fp, 2);                 /* nsects */
    scribe32(fp, 0);

    /* section __text */
    scribe_nomen_seg(fp, "__text");
    scribe_nomen_seg(fp, "__TEXT");
    scribe64(fp, text_sect_vmaddr);
    scribe64(fp, text_sect_size);
    scribe32(fp, text_sect_offset);
    scribe32(fp, 2);                 /* align = 2^2 = 4 */
    scribe32(fp, 0);                 /* reloff */
    scribe32(fp, 0);                 /* nreloc */
    scribe32(fp, S_REGULAR | S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS);
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* reserved1,2,3 */

    /* section __cstring */
    scribe_nomen_seg(fp, "__cstring");
    scribe_nomen_seg(fp, "__TEXT");
    scribe64(fp, cstring_vmaddr);
    scribe64(fp, cstring_size);
    scribe32(fp, cstring_offset);
    scribe32(fp, 0);                 /* align = 2^0 = 1 */
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, S_CSTRING_LITERALS);
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* reserved1,2,3 */

    /* --- LC_SEGMENT_64 __DATA --- */
    scribe32(fp, LC_SEGMENT_64);
    scribe32(fp, lc_data);
    scribe_nomen_seg(fp, "__DATA");
    scribe64(fp, data_vmaddr);
    scribe64(fp, data_seg_vmsize);
    scribe64(fp, data_seg_fileoff);
    scribe64(fp, data_file_size);
    scribe32(fp, VM_PROT_READ | VM_PROT_WRITE);
    scribe32(fp, VM_PROT_READ | VM_PROT_WRITE);
    scribe32(fp, 3);                 /* nsects */
    scribe32(fp, 0);

    /* section __got */
    scribe_nomen_seg(fp, "__got");
    scribe_nomen_seg(fp, "__DATA");
    scribe64(fp, got_vmaddr);
    scribe64(fp, got_lon);
    scribe32(fp, data_seg_fileoff + got_sect_off);
    scribe32(fp, 3);                 /* align = 2^3 = 8 */
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, S_NON_LAZY_SYMBOL_POINTERS);
    scribe32(fp, 0); /* reserved1 = 0 (index in tabulam symbolorum indirectam) */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* reserved2,3 */

    /* section __data */
    scribe_nomen_seg(fp, "__data");
    scribe_nomen_seg(fp, "__DATA");
    scribe64(fp, idata_vmaddr);
    scribe64(fp, idata_sect_size);
    scribe32(fp, idata_sect_size > 0 ? data_seg_fileoff + idata_sect_off : 0);
    scribe32(fp, 3);
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, S_REGULAR);
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* reserved1,2,3 */

    /* section __bss */
    scribe_nomen_seg(fp, "__bss");
    scribe_nomen_seg(fp, "__DATA");
    scribe64(fp, bss_vmaddr);
    scribe64(fp, bss_lon);
    scribe32(fp, 0);                 /* no file data */
    scribe32(fp, 3);
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, S_ZEROFILL);
    scribe32(fp, 0);
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* reserved1,2,3 */

    /* --- LC_SEGMENT_64 __LINKEDIT --- */
    scribe32(fp, LC_SEGMENT_64);
    scribe32(fp, lc_linkedit);
    scribe_nomen_seg(fp, "__LINKEDIT");
    scribe64(fp, linkedit_vmaddr);
    scribe64(fp, linkedit_vmsize);
    scribe64(fp, linkedit_fileoff);
    scribe64(fp, linkedit_size);
    scribe32(fp, VM_PROT_READ);
    scribe32(fp, VM_PROT_READ);
    scribe32(fp, 0);
    scribe32(fp, 0);

    /* --- LC_DYLD_INFO_ONLY --- */
    scribe32(fp, LC_DYLD_INFO_ONLY);
    scribe32(fp, lc_dyld_info);
    scribe32(fp, rebase_off);        /* rebase_off */
    scribe32(fp, rebase_size);       /* rebase_size */
    scribe32(fp, bind_off);          /* bind_off */
    scribe32(fp, bind_size);         /* bind_size */
    scribe32(fp, 0);                 /* weak_bind_off */
    scribe32(fp, 0);                 /* weak_bind_size */
    scribe32(fp, lazy_bind_off);     /* lazy_bind_off */
    scribe32(fp, lazy_bind_size);    /* lazy_bind_size */
    scribe32(fp, export_off);        /* export_off */
    scribe32(fp, export_size);       /* export_size */

    /* --- LC_SYMTAB --- */
    scribe32(fp, LC_SYMTAB);
    scribe32(fp, lc_symtab);
    scribe32(fp, symtab_off);
    scribe32(fp, nsyms);
    scribe32(fp, strtab_off);
    scribe32(fp, strtab_size);

    /* --- LC_DYSYMTAB --- */
    scribe32(fp, LC_DYSYMTAB);
    scribe32(fp, lc_dysymtab);
    scribe32(fp, 0);  /* ilocalsym */
    scribe32(fp, 0);  /* nlocalsym */
    scribe32(fp, iextdefsym); /* iextdefsym */
    scribe32(fp, nextdefsym); /* nextdefsym */
    scribe32(fp, iundefsym);  /* iundefsym */
    scribe32(fp, nundefsym);  /* nundefsym */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* tocoff, ntoc */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* modtaboff, nmodtab */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* extrefsymoff, nextrefsyms */
    scribe32(fp, indsym_off);
    scribe32(fp, num_got);
    /* indirectsymoff, nindirectsyms */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* extreloff, nextrel */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* locreloff, nlocrel */

    /* --- LC_LOAD_DYLINKER --- */
    scribe32(fp, LC_LOAD_DYLINKER);
    scribe32(fp, lc_dylinker);
    scribe32(fp, 12);               /* name offset */
    scribe_chorda(fp, "/usr/lib/dyld");
    scribe_impletio(fp, lc_dylinker - 12 - 14);

    /* --- LC_MAIN --- */
    scribe32(fp, LC_MAIN);
    scribe32(fp, lc_main);
    scribe64(fp, text_sect_offset + main_offset); /* entryoff */
    scribe64(fp, 0);                              /* stacksize */

    /* --- LC_BUILD_VERSION --- */
    scribe32(fp, LC_BUILD_VERSION);
    scribe32(fp, lc_build);
    scribe32(fp, PLATFORM_MACOS);
    scribe32(fp, 0x000F0000);       /* minos = 15.0 */
    scribe32(fp, 0x000F0000);       /* sdk = 15.0 */
    scribe32(fp, 1);                /* ntools */
    scribe32(fp, TOOL_LD);
    scribe32(fp, 0x03000000);       /* version */

    /* --- LC_LOAD_DYLIB --- */
    scribe32(fp, LC_LOAD_DYLIB);
    scribe32(fp, lc_dylib);
    scribe32(fp, 24);               /* name offset */
    scribe32(fp, 0);                /* timestamp */
    scribe32(fp, 0x010000);         /* current_version */
    scribe32(fp, 0x010000);         /* compat_version */
    scribe_chorda(fp, "/usr/lib/libSystem.B.dylib");
    scribe_impletio(fp, lc_dylib - 24 - 27);

    /* --- LC_LOAD_DYLIB additionles (-l) --- */
    for (int i = 0; i < num_dylib_add; i++) {
        const char *via = biblio_dylib_via(i);
        int via_lon     = (int)strlen(via) + 1;
        scribe32(fp, LC_LOAD_DYLIB);
        scribe32(fp, lc_dylib_add[i]);
        scribe32(fp, 24);               /* name offset */
        scribe32(fp, 0);                /* timestamp */
        scribe32(fp, 0x010000);         /* current_version */
        scribe32(fp, 0x010000);         /* compat_version */
        scribe_chorda(fp, via);
        scribe_impletio(fp, lc_dylib_add[i] - 24 - via_lon);
    }

    /* --- padding ad text section --- */
    int cur_pos = header_size;
    scribe_impletio(fp, text_sect_offset - cur_pos);

    /* --- __text section --- */
    fwrite(codex, 1, codex_lon, fp);
    cur_pos = text_sect_offset + codex_lon;

    /* --- padding ad __cstring --- */
    scribe_impletio(fp, cstring_offset - cur_pos);
    fwrite(chordae_data, 1, chordae_lon, fp);
    cur_pos = cstring_offset + chordae_lon;

    /* --- padding ad data segment --- */
    scribe_impletio(fp, data_seg_fileoff - cur_pos);

    /* --- __got section (zeros — dyld fills in) --- */
    scribe_impletio(fp, got_lon);
    cur_pos = data_seg_fileoff + got_lon;

    /* --- __data section (initialized data) --- */
    if (idata_sect_size > 0) {
        scribe_impletio(fp, data_seg_fileoff + idata_sect_off - cur_pos);
        fwrite(init_data, 1, idata_sect_size, fp);
        cur_pos = data_seg_fileoff + idata_sect_off + idata_sect_size;
    }

    /* --- padding ad linkedit --- */
    scribe_impletio(fp, linkedit_fileoff - cur_pos);

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
    scribe_impletio(fp, export_off + export_size - (export_off + export_lon));

    /* --- padding ad symtab --- */
    cur_pos = export_off + export_size;
    scribe_impletio(fp, symtab_off - cur_pos);

    /* --- symbol table --- */
    for (int i = 0; i < nsyms; i++) {
        scribe32(fp, symtab_entries[i].n_strx);
        scribe8(fp, symtab_entries[i].n_type);
        scribe8(fp, symtab_entries[i].n_sect);
        scribe16(fp, (uint16_t)symtab_entries[i].n_desc);
        scribe64(fp, symtab_entries[i].n_value);
    }

    /* --- string table --- */
    fwrite(strtab, 1, strtab_lon, fp);

    /* --- tabula symbolorum indirecta --- */
    cur_pos = strtab_off + strtab_lon;
    scribe_impletio(fp, indsym_off - cur_pos);
    for (int i = 0; i < num_got; i++)
        scribe32(fp, indirect_syms[i]);

    /* --- padding ad finem --- */
    cur_pos     = indsym_off + indsym_size;
    int end_pos = linkedit_fileoff + linkedit_size;
    if (end_pos > cur_pos)
        scribe_impletio(fp, end_pos - cur_pos);

    fclose(fp);

    /* fac executābilem et signa ad-hoc */
    if (chmod(plica_exitus, 0755) != 0)
        erratum("chmod '%s' dēfēcit: %s", plica_exitus, strerror(errno));
    {
        pid_t pid = fork();
        if (pid < 0)
            erratum("fork dēfēcit: %s", strerror(errno));
        if (pid == 0) {
            /* prōcessus fīlius: codesign */
            execlp("codesign", "codesign", "-s", "-", plica_exitus, NULL);
            _exit(1); /* sī execlp dēfēcit */
        }
        int status;
        if (waitpid(pid, &status, 0) < 0)
            erratum("waitpid dēfēcit: %s", strerror(errno));
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            erratum("codesign '%s' dēfēcit", plica_exitus);
    }

    printf(
        "==> %s scriptum (%d octeti codis, %d GOT, %d globales)\n",
        plica_exitus, codex_lon, num_got, num_globalium
    );
}

void scribo_obiectum(const char *plica_exitus)
{
    /* ================================================================
     * applica fixups internos (rami intra codicem)
     * relocationes externae servantur pro ligatore
     * ================================================================ */

    /* collige symbola — functiones definitae et externae */
    /* symbola localia: functiones definitae in hoc plico */
    /* symbola externa: functiones in GOT (non definitae hic) */

    typedef struct {
        char nomen[260];    /* _nomen */
        int sectio;         /* 1 = __text, 2 = __cstring, 0 = undef */
        int valor;          /* offset in sectione */
    } sym_obj_t;

    static sym_obj_t syms[MAX_CHORDAE_LIT + MAX_GOT + MAX_GLOBALES];
    int nsyms    = 0;
    int n_extdef = 0;

    /* symbola localia pro chordis in __cstring */
    int n_loc = 0;
    int chorda_ad_sym[MAX_CHORDAE_LIT];
    if (num_chordarum > 0) {
        for (int i = 0; i < num_chordarum; i++) {
            chorda_ad_sym[i] = nsyms;
            snprintf(syms[nsyms].nomen, 260, "l_.str.%d", i);
            syms[nsyms].sectio = 2;
            syms[nsyms].valor  = codex_lon + chordae[i].offset;
            nsyms++;
            n_loc++;
        }
    }

    /* symbola localia pro functionibus staticis */
    for (int i = 0; i < num_func_loc; i++) {
        if (!func_loci[i].est_staticus)
            continue;
        char nn[260];
        snprintf(nn, 260, "_%s", func_loci[i].nomen);
        int iam = 0;
        for (int j = 0; j < nsyms; j++)
            if (strcmp(syms[j].nomen, nn) == 0) {
            iam = 1;
            break;
        }
        if (!iam) {
            strncpy(syms[nsyms].nomen, nn, 259);
            syms[nsyms].sectio = 1;
            syms[nsyms].valor  = labels[func_loci[i].label];
            nsyms++;
            n_loc++;
        }
    }

    /* functiones non-staticae definitae → extdef symbola */
    for (int i = 0; i < num_got; i++) {
        const char *gn = got[i].nomen;
        int fl         = func_loc_quaere(gn[0] == '_' ? gn + 1 : gn);
        if (fl >= 0) {
            int iam = 0;
            for (int j = 0; j < nsyms; j++)
                if (strcmp(syms[j].nomen, gn) == 0) {
                iam = 1;
                break;
            }
            if (!iam) {
                strncpy(syms[nsyms].nomen, gn, 259);
                syms[nsyms].sectio = 1;
                syms[nsyms].valor  = labels[fl];
                nsyms++;
                n_extdef++;
            }
        }
    }
    /* adde omnes functiones non-staticas quae non iam in GOT */
    for (int i = 0; i < num_func_loc; i++) {
        if (func_loci[i].est_staticus)
            continue;
        char nn[260];
        snprintf(nn, 260, "_%s", func_loci[i].nomen);
        int iam = 0;
        for (int j = 0; j < nsyms; j++)
            if (strcmp(syms[j].nomen, nn) == 0) {
            iam = 1;
            break;
        }
        if (!iam) {
            strncpy(syms[nsyms].nomen, nn, 259);
            syms[nsyms].sectio = 1;
            syms[nsyms].valor  = labels[func_loci[i].label];
            nsyms++;
            n_extdef++;
        }
    }

    /* globales → symbola communia (N_UNDF | N_EXT, valor = magnitudo) */
    int glob_ad_sym[MAX_GLOBALES];
    for (int i = 0; i < num_globalium; i++) {
        glob_ad_sym[i] = -1;
        char nn[260];
        if (globales[i].est_staticus)
            snprintf(nn, 260, "_L_%s.%d", globales[i].nomen, i);
        else
            snprintf(nn, 260, "_%s", globales[i].nomen);
        /* proba si iam addita */
        int iam = 0;
        for (int j = 0; j < nsyms; j++)
            if (strcmp(syms[j].nomen, nn) == 0) {
            iam = 1;
            glob_ad_sym[i] = j;
            break;
        }
        if (!iam) {
            glob_ad_sym[i] = nsyms;
            strncpy(syms[nsyms].nomen, nn, 259);
            syms[nsyms].sectio = 0; /* N_UNDF = communis */
            syms[nsyms].valor  = globales[i].magnitudo > 0 ? globales[i].magnitudo : 8;
            nsyms++;
        }
    }

    int i_undef = nsyms; /* index ubi externae incipiunt */

    /* symbola GOT non definita → undef */
    int got_ad_sym[MAX_GOT];
    for (int i = 0; i < num_got; i++) {
        const char *gn = got[i].nomen;
        int fl         = func_loc_quaere(gn[0] == '_' ? gn + 1 : gn);
        if (fl >= 0) {
            /* iam definitum — quaere indicem */
            for (int j = 0; j < i_undef; j++)
                if (strcmp(syms[j].nomen, gn) == 0) {
                got_ad_sym[i] = j;
                break;
            }
        } else {
            got_ad_sym[i] = nsyms;
            strncpy(syms[nsyms].nomen, gn, 259);
            syms[nsyms].sectio = 0;
            syms[nsyms].valor  = 0;
            nsyms++;
        }
    }

    /* relocationes ARM64 */
    typedef struct {
        int32_t  r_address;
        uint32_t r_info;   /* symbolum(24) | pcrel(1) | length(2) | extern(1) | type(4) */
    } reloc_t;

    #define ARM64_RELOC_UNSIGNED    0
    #define ARM64_RELOC_BRANCH26   2
    #define ARM64_RELOC_PAGE21     3
    #define ARM64_RELOC_PAGEOFF12  4
    #define ARM64_RELOC_GOT_LOAD_PAGE21     5
    #define ARM64_RELOC_GOT_LOAD_PAGEOFF12  6

    reloc_t relocs[MAX_FIXUPS];
    int nrelocs = 0;

    /* applica fixups — interni resolventur, externi → relocationes */
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
        case FIX_ADR_LABEL: {
                int target_off = labels[f->target];
                int delta = target_off - f->offset;
                int rd = inst & 0x1F;
                int immlo = delta & 3;
                int immhi = (delta >> 2) & 0x7FFFF;
                inst = 0x10000000 | (immlo << 29) | (immhi << 5) | rd;
                break;
            }
        case FIX_CBZ: {
                int target_off = labels[f->target];
                int delta = (target_off - f->offset) / 4;
                int rt = inst & 0x1F;
                inst = 0xB4000000 | ((delta & 0x7FFFF) << 5) | rt;
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
            /* ADRP ad chorda in __cstring — relocatio externa */
                int sym_idx = chorda_ad_sym[f->target];
                relocs[nrelocs].r_address = f->offset;
                relocs[nrelocs].r_info = (sym_idx & 0xFFFFFF) |
                    (1 << 24) | (2 << 25) | (1 << 27) |   /* pcrel=1, len=2, extern=1 */
                    ((uint32_t)ARM64_RELOC_PAGE21 << 28);
                nrelocs++;
                inst = 0x90000000 | (inst & 0x1F); /* ADRP Xd, #0 */
                break;
            }
        case FIX_ADD_LO12: {
            /* ADD lo12 ad chorda in __cstring — relocatio externa */
                int sym_idx = chorda_ad_sym[f->target];
                int rd = inst & 0x1F;
                int rn = (inst >> 5) & 0x1F;
                relocs[nrelocs].r_address = f->offset;
                relocs[nrelocs].r_info = (sym_idx & 0xFFFFFF) |
                    (0 << 24) | (2 << 25) | (1 << 27) |   /* pcrel=0, len=2, extern=1 */
                    ((uint32_t)ARM64_RELOC_PAGEOFF12 << 28);
                nrelocs++;
                inst = 0x91000000 | (rn << 5) | rd; /* ADD Xd, Xn, #0 */
                break;
            }
        case FIX_ADRP_GOT: {
            /* functio externa — relocatio GOT */
                int sym_idx = got_ad_sym[f->target];
                relocs[nrelocs].r_address = f->offset;
            /* r_info: sym(24) | pcrel(1) | len(2) | ext(1) | type(4) */
                relocs[nrelocs].r_info = (sym_idx & 0xFFFFFF) |
                    (1 << 24) | (2 << 25) | (1 << 27) |
                    ((uint32_t)ARM64_RELOC_GOT_LOAD_PAGE21 << 28);
                nrelocs++;
                inst = 0x90000000 | (inst & 0x1F);
                break;
            }
        case FIX_LDR_GOT_LO12: {
                int sym_idx = got_ad_sym[f->target];
                relocs[nrelocs].r_address = f->offset;
                relocs[nrelocs].r_info = (sym_idx & 0xFFFFFF) |
                    (0 << 24) | (2 << 25) | (1 << 27) |
                    ((uint32_t)ARM64_RELOC_GOT_LOAD_PAGEOFF12 << 28);
                nrelocs++;
                break;
            }
        case FIX_ADRP_DATA: {
                int gid     = f->target;
                int sym_idx = (gid >= 0 && gid < num_globalium) ? glob_ad_sym[gid] : -1;
                if (sym_idx >= 0) {
                    relocs[nrelocs].r_address = f->offset;
                    relocs[nrelocs].r_info = (sym_idx & 0xFFFFFF) |
                        (1 << 24) | (2 << 25) | (1 << 27) |
                        ((uint32_t)ARM64_RELOC_PAGE21 << 28);
                    nrelocs++;
                }
                inst = 0x90000000 | (inst & 0x1F); /* ADRP Xd, #0 */
                break;
            }
        case FIX_ADD_LO12_DATA:
        case FIX_LDR_LO12_DATA:
        case FIX_STR_LO12_DATA: {
                int gid     = f->target;
                int sym_idx = (gid >= 0 && gid < num_globalium) ? glob_ad_sym[gid] : -1;
                if (sym_idx >= 0) {
                    relocs[nrelocs].r_address = f->offset;
                    relocs[nrelocs].r_info = (sym_idx & 0xFFFFFF) |
                        (0 << 24) | (2 << 25) | (1 << 27) |
                        ((uint32_t)ARM64_RELOC_PAGEOFF12 << 28);
                    nrelocs++;
                }
                break;
            }
        default:
            break;
        }
        memcpy(&codex[f->offset], &inst, 4);
    }

    /* ================================================================
     * layout .o
     * ================================================================ */

    int cstring_size = chordae_lon;
    int text_size    = codex_lon;

    /* sectiones: __text, __cstring */
    int nsects = cstring_size > 0 ? 2 : 1;

    /* Mach-O caput */
    int header_size  = 32;  /* mach_header_64 */
    int seg_cmd_size = 72 + nsects * 80;
    int symtab_size  = 24;  /* LC_SYMTAB */
    int dysym_size   = 80;  /* LC_DYSYMTAB */
    int sizeofcmds   = seg_cmd_size + symtab_size + dysym_size;

    int text_offset    = header_size + sizeofcmds;
    int cstring_offset = text_offset + text_size;
    int reloc_offset   = (int)allinea(cstring_offset + cstring_size, 4);
    int reloc_size     = nrelocs * 8;

    /* tabula symbolorum */
    /* chordae nominum */
    static uint8_t strtab[262144];
    int strtab_lon = 1; /* primus octet = '\0' */
    strtab[0]      = '\0';

    static int sym_stridx[MAX_CHORDAE_LIT + MAX_GOT + MAX_GLOBALES];
    for (int i = 0; i < nsyms; i++) {
        sym_stridx[i] = strtab_lon;
        int len       = (int)strlen(syms[i].nomen);
        memcpy(&strtab[strtab_lon], syms[i].nomen, len + 1);
        strtab_lon += len + 1;
    }
    int strtab_size = (int)allinea(strtab_lon, 4);

    int nlist_size = nsyms * 16; /* nlist_64 = 16 octeti */
    int symtab_off = reloc_offset + reloc_size;
    int strtab_off = symtab_off + nlist_size;

    int n_undef = nsyms - n_extdef - n_loc;

    /* ================================================================
     * scribo plicam
     * ================================================================ */

    FILE *fp = fopen(plica_exitus, "wb");
    if (!fp)
        erratum("non possum aperire '%s'", plica_exitus);

    /* Mach-O caput */
    scribe32(fp, MH_MAGIC_64);
    scribe32(fp, CPU_TYPE_ARM64);
    scribe32(fp, CPU_SUBTYPE_ALL);
    scribe32(fp, 1); /* MH_OBJECT */
    scribe32(fp, 3); /* ncmds: segment + symtab + dysymtab */
    scribe32(fp, sizeofcmds);
    scribe32(fp, MH_NOUNDEFS);
    scribe32(fp, 0); /* reservatum */

    /* LC_SEGMENT_64 — segmentum unicum */
    scribe32(fp, LC_SEGMENT_64);
    scribe32(fp, seg_cmd_size);
    scribe_nomen_seg(fp, "");  /* nomen vacuum pro .o */
    scribe64(fp, 0);           /* vmaddr */
    scribe64(fp, text_size + cstring_size); /* vmsize */
    scribe64(fp, text_offset); /* fileoff */
    scribe64(fp, text_size + cstring_size); /* filesize */
    scribe32(fp, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE); /* maxprot */
    scribe32(fp, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE); /* initprot */
    scribe32(fp, nsects);
    scribe32(fp, 0); /* vexilla */

    /* sectio __text */
    {
        char sectname[16] = {0}, segname[16] = {0};
        strncpy(sectname, "__text", 16);
        strncpy(segname, "__TEXT", 16);
        fwrite(sectname, 1, 16, fp);
        fwrite(segname, 1, 16, fp);
        scribe64(fp, 0);           /* addr */
        scribe64(fp, text_size);   /* size */
        scribe32(fp, text_offset); /* offset */
        scribe32(fp, 2);           /* align = 2^2 = 4 */
        scribe32(fp, nrelocs > 0 ? reloc_offset : 0); /* reloff */
        scribe32(fp, nrelocs);     /* nreloc */
        scribe32(fp, S_REGULAR | S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS);
        scribe32(fp, 0);
        scribe32(fp, 0);
        scribe32(fp, 0);
        /* reservata 1-3 */
    }

    /* sectio __cstring (si necessaria) */
    if (nsects > 1) {
        char sectname[16] = {0}, segname[16] = {0};
        strncpy(sectname, "__cstring", 16);
        strncpy(segname, "__TEXT", 16);
        fwrite(sectname, 1, 16, fp);
        fwrite(segname, 1, 16, fp);
        scribe64(fp, text_size);       /* addr */
        scribe64(fp, cstring_size);    /* size */
        scribe32(fp, cstring_offset);  /* offset */
        scribe32(fp, 0);               /* align */
        scribe32(fp, 0);               /* reloff */
        scribe32(fp, 0);               /* nreloc */
        scribe32(fp, S_CSTRING_LITERALS);
        scribe32(fp, 0);
        scribe32(fp, 0);
        scribe32(fp, 0);
        /* reservata 1-3 */
    }

    /* LC_SYMTAB */
    scribe32(fp, LC_SYMTAB);
    scribe32(fp, symtab_size);
    scribe32(fp, symtab_off);
    scribe32(fp, nsyms);
    scribe32(fp, strtab_off);
    scribe32(fp, strtab_size);

    /* LC_DYSYMTAB */
    scribe32(fp, LC_DYSYMTAB);
    scribe32(fp, dysym_size);
    scribe32(fp, 0);           /* ilocalsym */
    scribe32(fp, n_loc);       /* nlocalsym */
    scribe32(fp, n_loc);       /* iextdefsym */
    scribe32(fp, n_extdef);    /* nextdefsym */
    scribe32(fp, n_loc + n_extdef); /* iundefsym */
    scribe32(fp, n_undef);     /* nundefsym */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* tocoff, ntoc */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* modtaboff, nmodtab */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* extrefsymoff, nextrefsyms */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* indirectsymoff, nindirectsyms */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* extreloff, nextrel */
    scribe32(fp, 0);
    scribe32(fp, 0);
    /* locreloff, nlocrel */

    /* __text data */
    fwrite(codex, 1, text_size, fp);

    /* __cstring data */
    if (cstring_size > 0)
        fwrite(chordae_data, 1, cstring_size, fp);

    /* impletio ante relocationes */
    int cur = cstring_offset + cstring_size;
    while (cur < reloc_offset) {
        scribe8(fp, 0);
        cur++;
    }

    /* relocationes */
    for (int i = 0; i < nrelocs; i++) {
        scribe32(fp, (uint32_t)relocs[i].r_address);
        scribe32(fp, relocs[i].r_info);
    }

    /* tabula symbolorum (nlist_64) */
    for (int i = 0; i < nsyms; i++) {
        scribe32(fp, sym_stridx[i]); /* n_strx */
        if (syms[i].sectio > 0) {
            scribe8(fp, i < n_loc ? N_SECT : (N_SECT | N_EXT)); /* n_type */
            scribe8(fp, syms[i].sectio); /* n_sect */
        } else {
            scribe8(fp, N_EXT);    /* n_type: externum */
            scribe8(fp, 0);        /* n_sect: NO_SECT */
        }
        scribe16(fp, 0);           /* n_desc */
        scribe64(fp, syms[i].valor); /* n_value */
    }

    /* tabula chordarum */
    fwrite(strtab, 1, strtab_lon, fp);
    cur = strtab_lon;
    while (cur < strtab_size) {
        scribe8(fp, 0);
        cur++;
    }

    fclose(fp);

    printf(
        "==> %s obiectum (%d octeti codis, %d symbola)\n",
        plica_exitus, codex_lon, nsyms
    );
}
