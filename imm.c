/*
 * imm.c — assemblator ARM64 pro Mach-O (forma "cc -S" compatibilis)
 *
 * Nomen: IMM = I ante MM = 1999 (annus ISO C99), lectio non-
 * classica Romanorum numerorum. Conformiter Knuthi MIX = 1009,
 * ubi Mixal assemblator est eius machinae.
 *
 * Usus:
 *   imm plica.s [-o plica.o]
 *
 * Legit plicam .s generatam per `cc -S` in macOS ARM64, emittit .o
 * utens emitte.c pro instructionibus et scribo.c pro scriptione
 * Mach-O. Si instructio vel directiva ignota occurrit, erratum().
 */

#include "utilia.h"
#include "emitte.h"
#include "scribo.h"
#include "typus.h"

/* ================================================================
 * sectiones
 * ================================================================ */

enum { SEC_TEXT, SEC_CSTRING, SEC_DATA, SEC_CONST, SEC_BSS };
static int sectio_currens = SEC_TEXT;

/* ================================================================
 * tabula symbolorum (nomen .s → referentia interna)
 * ================================================================ */

enum {
    SYM_IGNOTUS,    /* nondum definitum nec refertum */
    SYM_TXT_LAB,    /* label localis in __text (LBB..., Ltmp...) */
    SYM_FUNC,       /* functio in __text (_nomen:) — habet func_loc */
    SYM_CHORDA,     /* intrans in __cstring (id = index chordae) */
    SYM_GLOB,       /* variabilis in __data/__const/__bss (id = index globalis) */
    SYM_EXT         /* symbolum externum (id = index GOT) */
};

typedef struct {
    char nomen[256];
    int  genus;         /* SYM_* */
    int  id;            /* depends on genus */
    int  est_globalis;  /* .globl visum */
    int  definitum;     /* vera si label vel data definita */
} sym_t;

static sym_t *symbola     = NULL;
static int num_symbolorum = 0;
static int cap_symbolorum = 0;

static int sym_quaere_idx(const char *nomen)
{
    for (int i = 0; i < num_symbolorum; i++)
        if (strcmp(symbola[i].nomen, nomen) == 0)
            return i;
    return -1;
}

static int sym_crea(const char *nomen)
{
    CRESC_SERIEM(symbola, num_symbolorum, cap_symbolorum, sym_t);
    int i = num_symbolorum++;
    strncpy(symbola[i].nomen, nomen, 255);
    symbola[i] .nomen[255] = 0;
    symbola[i] .genus = SYM_IGNOTUS;
    symbola[i] .id = -1;
    symbola[i] .est_globalis = 0;
    symbola[i] .definitum = 0;
    return i;
}

static int sym_quaere_vel_crea(const char *nomen)
{
    int i = sym_quaere_idx(nomen);
    if (i < 0)
        i = sym_crea(nomen);
    return i;
}

/* ================================================================
 * datae (__data, __const, __bss) — accumulatur per globalis_adde
 * Ad scribo_obiectum conventionem simplius servamus:
 * ponimus omnia in init_data sub entriis globalis_t.
 * ================================================================ */

static int colineatio_currens = 1; /* ex .p2align */

/* ================================================================
 * parser primitives
 * ================================================================ */

static const char *fons;
static int fons_lon;
static int pos;
static int linea_num = 1;

static int adest(void) { return pos < fons_lon; }
static char spectus(void) { return pos < fons_lon ? fons[pos] : 0; }

static void saltus_spatii(void)
{
    while (adest()) {
        char c = fons[pos];
        if (c == ' ' || c == '\t')
            pos++;
        else
            break;
    }
}

static int est_initium_nom(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
        || c == '_' || c == '.' || c == '$'
        || ((unsigned char)c & 0x80);
}

static int est_pars_nom(char c)
{
    return est_initium_nom(c) || (c >= '0' && c <= '9');
}

static int lege_nom(char *buf, int max)
{
    saltus_spatii();
    int n = 0;
    if (!adest() || !est_initium_nom(fons[pos]))
        return 0;
    while (adest() && est_pars_nom(fons[pos]) && n + 1 < max)
        buf[n++] = fons[pos++];
    buf[n] = 0;
    if (!utf8_valida(buf, n))
        erratum_ad(linea_num, "nomen cum UTF-8 invalida: '%s'", buf);
    return n;
}

/* lege operandum qui potest esse 'nomen' vel 'nomen@PAGE' etc. */
static int lege_sym_cum_rel(char *buf, int max, char *rel, int rmax)
{
    rel[0] = 0;
    int n  = lege_nom(buf, max);
    if (n == 0)
        return 0;
    if (adest() && fons[pos] == '@') {
        pos++;
        int rn = 0;
        while (adest() && est_pars_nom(fons[pos]) && rn + 1 < rmax)
            rel[rn++] = fons[pos++];
        rel[rn] = 0;
    }
    return n;
}

static int matcha(char c)
{
    saltus_spatii();
    if (adest() && fons[pos] == c) {
        pos++;
        return 1;
    }
    return 0;
}

static void exige(char c)
{
    saltus_spatii();
    if (!adest() || fons[pos] != c)
        erratum_ad(linea_num, "exspectavi '%c'", c);
    pos++;
}

/* lege numerum integrum: supportat decimal, 0x... negativus */
static long lege_num(void)
{
    saltus_spatii();
    int neg = 0;
    if (adest() && fons[pos] == '-') {
        neg = 1;
        pos++;
    } else if (adest() && fons[pos] == '+') {
        pos++;
    }
    if (!adest() || fons[pos] < '0' || fons[pos] > '9')
        erratum_ad(linea_num, "exspectavi numerum");
    long val = 0;
    if (
        fons[pos] == '0' && pos + 1 < fons_lon
        && (fons[pos+1] == 'x' || fons[pos+1] == 'X')
    ) {
        pos += 2;
        while (adest()) {
            char c = fons[pos];
            int d;
            if (c >= '0' && c <= '9')
                d = c - '0';
            else if (c >= 'a' && c <= 'f')
                d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')
                d = c - 'A' + 10;
            else
                break;
            val = val * 16 + d;
            pos++;
        }
    } else {
        while (adest() && fons[pos] >= '0' && fons[pos] <= '9') {
            val = val * 10 + (fons[pos] - '0');
            pos++;
        }
    }
    return neg ? -val : val;
}

/* #imm */
static long lege_imm(void)
{
    saltus_spatii();
    if (!matcha('#'))
        erratum_ad(linea_num, "exspectavi valorem immediatum '#N'");
    return lege_num();
}

/* ================================================================
 * parser registrorum
 *
 * Reddit codicem 0-31; pone *est_w = 1 pro W-registris, 0 pro X.
 * SP/XZR → 31. FP → 29. LR → 30.
 * ================================================================ */

/* reg_kind_ult: 0=normalis, 1=sp, 2=zr */
static int reg_kind_ult = 0;

static int parse_reg(int *est_w)
{
    saltus_spatii();
    est_w[0]     = 0;
    reg_kind_ult = 0;
    if (!adest())
        erratum_ad(linea_num, "exspectavi registrum");
    char c = fons[pos];
    if (c == 'x' || c == 'X' || c == 'w' || c == 'W') {
        *est_w = (c == 'w' || c == 'W');
        pos++;
        if (adest() && fons[pos] == 'z' && pos+1 < fons_lon && fons[pos+1] == 'r') {
            pos += 2;
            reg_kind_ult = 2;
            return 31;
        }
        if (!adest() || fons[pos] < '0' || fons[pos] > '9')
            erratum_ad(linea_num, "registrum invalidum");
        int n = 0;
        while (adest() && fons[pos] >= '0' && fons[pos] <= '9') {
            n = n * 10 + (fons[pos] - '0');
            pos++;
        }
        if (n > 31)
            erratum_ad(linea_num, "registrum > 31: %d", n);
        return n;
    }
    if (c == 's' || c == 'S') {
        /* SP */
        if (pos+1 < fons_lon && (fons[pos+1] == 'p' || fons[pos+1] == 'P')) {
            pos += 2;
            reg_kind_ult = 1;
            return 31;
        }
    }
    if (c == 'f' || c == 'F') {
        if (pos+1 < fons_lon && (fons[pos+1] == 'p' || fons[pos+1] == 'P')) {
            pos += 2;
            return 29;
        }
    }
    if (c == 'l' || c == 'L') {
        if (pos+1 < fons_lon && (fons[pos+1] == 'r' || fons[pos+1] == 'R')) {
            pos += 2;
            return 30;
        }
    }
    /* wzr/xzr iam factum supra, sed reliquum */
    erratum_ad(linea_num, "registrum ignotum");
    return -1;
}

/* estne sequentia characterum registrum (proxime)? */
static int est_prox_registrum(void)
{
    saltus_spatii();
    if (!adest())
        return 0;
    char c  = fons[pos];
    char c1 = (pos+1 < fons_lon) ? fons[pos+1] : 0;
    char c2 = (pos+2 < fons_lon) ? fons[pos+2] : 0;
    if (c == 'x' || c == 'X' || c == 'w' || c == 'W') {
        if (c1 >= '0' && c1 <= '9')
            return 1;
        if (c1 == 'z' && c2 == 'r')
            return 1;
        return 0;
    }
    if ((c == 's' || c == 'S') && (c1 == 'p' || c1 == 'P')) {
        if (est_pars_nom(c2))
            return 0;
        return 1;
    }
    if ((c == 'f' || c == 'F') && (c1 == 'p' || c1 == 'P')) {
        if (est_pars_nom(c2))
            return 0;
        return 1;
    }
    if ((c == 'l' || c == 'L') && (c1 == 'r' || c1 == 'R')) {
        if (est_pars_nom(c2))
            return 0;
        return 1;
    }
    return 0;
}

/* condicio: eq, ne, lt, ... → codicem */
static int parse_cond(const char *s)
{
    if (!strcmp(s, "eq"))
        return 0x0;
    if (!strcmp(s, "ne"))
        return 0x1;
    if (!strcmp(s, "cs") || !strcmp(s, "hs"))
        return 0x2;
    if (!strcmp(s, "cc") || !strcmp(s, "lo"))
        return 0x3;
    if (!strcmp(s, "mi"))
        return 0x4;
    if (!strcmp(s, "pl"))
        return 0x5;
    if (!strcmp(s, "vs"))
        return 0x6;
    if (!strcmp(s, "vc"))
        return 0x7;
    if (!strcmp(s, "hi"))
        return 0x8;
    if (!strcmp(s, "ls"))
        return 0x9;
    if (!strcmp(s, "ge"))
        return 0xA;
    if (!strcmp(s, "lt"))
        return 0xB;
    if (!strcmp(s, "gt"))
        return 0xC;
    if (!strcmp(s, "le"))
        return 0xD;
    if (!strcmp(s, "al"))
        return 0xE;
    return -1;
}

/* ================================================================
 * parse stringi litteralis "..."
 * ================================================================ */

static int parse_chorda_lit(char *out, int max)
{
    saltus_spatii();
    exige('"');
    int n = 0;
    while (adest() && fons[pos] != '"') {
        char c = fons[pos++];
        if (c == '\\' && adest()) {
            char e = fons[pos++];
            switch (e) {
            case 'n':
                c = '\n';
                break;
            case 't':
                c = '\t';
                break;
            case 'r':
                c = '\r';
                break;
            case '\\':
                c = '\\';
                break;
            case '"':
                c = '"';
                break;
            case '\'':
                c = '\'';
                break;
            case 'a':
                c = '\a';
                break;
            case 'b':
                c = '\b';
                break;
            case 'f':
                c = '\f';
                break;
            case 'v':
                c = '\v';
                break;
            case 'x': {
                    int v = 0, k = 0;
                    while (k < 2 && adest()) {
                        char d = fons[pos];
                        int x;
                        if (d >= '0' && d <= '9')
                            x = d - '0';
                        else if (d >= 'a' && d <= 'f')
                            x = d - 'a' + 10;
                        else if (d >= 'A' && d <= 'F')
                            x = d - 'A' + 10;
                        else
                            break;
                        v = v*16 + x;
                        pos++;
                        k++;
                    }
                    c = (char)v;
                    break;
                }
            default:
                if (e >= '0' && e <= '7') {
                    int v = e - '0', k = 1;
                    while (k < 3 && adest() && fons[pos] >= '0' && fons[pos] <= '7') {
                        v = v*8 + (fons[pos] - '0');
                        pos++;
                        k++;
                    }
                    c = (char)v;
                } else
                    erratum_ad(linea_num, "escape '\\%c' ignotum", e);
            }
        }
        if (n + 1 >= max)
            erratum_ad(linea_num, "chorda nimis longa");
        out[n++] = c;
    }
    exige('"');
    return n;
}

/* ================================================================
 * ignora ad finem lineae
 * ================================================================ */

static void saltus_ad_eol(void)
{
    while (adest() && fons[pos] != '\n')
        pos++;
}

static int est_eol(void)
{
    saltus_spatii();
    if (!adest())
        return 1;
    if (fons[pos] == '\n')
        return 1;
    if (fons[pos] == ';') { /* commentum ; ... */ return 1; }
    if (fons[pos] == '/' && pos+1 < fons_lon && fons[pos+1] == '/')
        return 1;
    return 0;
}

/* ================================================================
 * memoria operandi: [Xn], [Xn, #imm], [Xn, Xm, lsl #s], [Xn, #imm]!,
 *                   [Xn], #imm (post-index)
 *
 * Reddit structuram parsatam.
 * ================================================================ */

typedef struct {
    int rn;          /* registrum base */
    int habet_imm;
    long imm;
    int habet_rm;
    int rm;
    int rm_w;        /* 1 si W-reg */
    int ext_shift;   /* lsl/sxtw/uxtw amount, -1 si nullum */
    int ext_genus;   /* 0=lsl, 1=sxtw, 2=uxtw, 3=sxtx */
    int pre_index;   /* [Xn, #imm]! */
    int post_index;  /* [Xn], #imm — post-index, imm iam parsatum */
    int habet_rel_sym; /* [Xn, _foo@PAGEOFF] */
    char rel_sym[256];
    char rel[32];    /* "PAGEOFF" vel "GOTPAGEOFF" */
} mem_t;

static void parse_mem(mem_t *m)
{
    memset(m, 0, sizeof(*m));
    m->ext_shift = -1;
    exige('[');
    int ew;
    m->rn = parse_reg(&ew);
    if (ew)
        erratum_ad(linea_num, "memoriae base debet esse X-reg");
    if (matcha(',')) {
        saltus_spatii();
        /* intus potest esse #imm, Xm, vel _sym@PAGEOFF */
        if (spectus() == '#') {
            m ->habet_imm = 1;
            m ->imm       = lege_imm();
        } else if (
            est_initium_nom(spectus())
            && !(
                spectus() == 'x' || spectus() == 'X'
                || spectus() == 'w' || spectus() == 'W'
                || (
                    spectus() == 's' && pos+1 < fons_lon
                    && (fons[pos+1] == 'p' || fons[pos+1] == 'P')
                )
                || (
                    spectus() == 'f' && pos+1 < fons_lon
                    && (fons[pos+1] == 'p' || fons[pos+1] == 'P')
                )
                || (
                    spectus() == 'l' && pos+1 < fons_lon
                    && (fons[pos+1] == 'r' || fons[pos+1] == 'R')
                )
            )
        ) {
            m->habet_rel_sym = 1;
            lege_sym_cum_rel(m->rel_sym, 256, m->rel, 32);
        } else {
            /* heuristica: si litterae sunt 'x'/'w'+numerus vel 'sp', registrum */
            /* proba parse_reg */
            int save = pos;
            (void)save;
            m ->habet_rm = 1;
            m ->rm       = parse_reg(&m->rm_w);
            if (matcha(',')) {
                /* shift vel extend */
                char ext[16];
                lege_nom(ext, 16);
                if (!strcmp(ext, "lsl"))
                    m->ext_genus = 0;
                else if (!strcmp(ext, "sxtw"))
                    m->ext_genus = 1;
                else if (!strcmp(ext, "uxtw"))
                    m->ext_genus = 2;
                else if (!strcmp(ext, "sxtx"))
                    m->ext_genus = 3;
                else
                    erratum_ad(linea_num, "extendere ignotum '%s'", ext);
                saltus_spatii();
                if (spectus() == '#')
                    m->ext_shift = (int)lege_imm();
                else
                    m->ext_shift = 0;
            }
        }
    }
    exige(']');
    if (matcha('!'))
        m->pre_index = 1;
    else if (matcha(',')) {
        /* post-index: [Xn], #imm */
        m->post_index = 1;
        saltus_spatii();
        if (spectus() == '#') {
            m ->habet_imm = 1;
            m ->imm       = lege_imm();
        } else
            erratum_ad(linea_num, "post-index exspectat valorem immediatum");
    }
}

/* ================================================================
 * registra ignorata in clang: certae directivae sine effectu codicis
 * ================================================================ */

static int directiva_ignoranda(const char *d)
{
    if (!strncmp(d, ".cfi_", 5))
        return 1;
    if (!strcmp(d, ".build_version"))
        return 1;
    if (!strcmp(d, ".subsections_via_symbols"))
        return 1;
    if (!strcmp(d, ".file"))
        return 1;
    if (!strcmp(d, ".loc"))
        return 1;
    if (!strcmp(d, ".private_extern"))
        return 1;
    if (!strcmp(d, ".weak_def_can_be_hidden"))
        return 1;
    if (!strcmp(d, ".weak_definition"))
        return 1;
    if (!strcmp(d, ".weak_reference"))
        return 1;
    if (!strcmp(d, ".no_dead_strip"))
        return 1;
    if (!strcmp(d, ".addrsig"))
        return 1;
    if (!strcmp(d, ".addrsig_sym"))
        return 1;
    if (!strcmp(d, ".debug_info"))
        return 1;
    if (!strcmp(d, ".ident"))
        return 1;
    if (!strcmp(d, ".loh"))
        return 1;
    if (!strcmp(d, ".align"))
        return 0; /* non ignorata */
    return 0;
}

/* ================================================================
 * pass 1: scanna omnes labels ut tabulam symbolorum pleniorem habeamus
 * ================================================================ */

static int chordae_counter_p1 = 0;

static void pass1(void)
{
    int save_pos = pos;
    int save_linea = linea_num;
    int save_sect = sectio_currens;
    pos = 0;
    linea_num = 1;
    int sect = SEC_TEXT;
    int ultimus_globl_def = 0;
    chordae_counter_p1 = 0;

    while (adest()) {
        saltus_spatii();
        if (!adest() || fons[pos] == '\n') {
            if (adest()) {
                pos++;
                linea_num++;
            }
            continue;
        }
        if (fons[pos] == ';' || (fons[pos] == '/' && pos+1 < fons_lon && fons[pos+1] == '/')) {
            saltus_ad_eol();
            continue;
        }
        if (fons[pos] == '#') {
            /* praeprocessor lineae: ignora */
            saltus_ad_eol();
            continue;
        }
        /* label vel directiva vel instructio */
        char nom[256];
        int save_p = pos;
        int nl     = lege_nom(nom, 256);
        if (nl == 0) {
            saltus_ad_eol();
            continue;
        }
        saltus_spatii();
        if (adest() && fons[pos] == ':') {
            pos++;
            /* labelum definitum — registra sectionem eius */
            int si      = sym_quaere_vel_crea(nom);
            symbola[si] .definitum = 1;
            if (sect == SEC_TEXT) {
                if (nom[0] == '_') {
                    symbola[si] .genus        = SYM_FUNC;
                    symbola[si] .est_globalis = ultimus_globl_def;
                } else {
                    symbola[si].genus = SYM_TXT_LAB;
                }
            } else if (sect == SEC_CSTRING) {
                symbola[si].genus = SYM_CHORDA;
                if (symbola[si].id < 0)
                    symbola[si].id = chordae_counter_p1++;
            } else {
                symbola[si].genus = SYM_GLOB;
                if (nom[0] == '_')
                    symbola[si].est_globalis = ultimus_globl_def;
            }
            ultimus_globl_def = 0;
            continue;
        }
        /* directiva? */
        if (nom[0] == '.') {
            if (!strcmp(nom, ".text"))
                sect = SEC_TEXT;
            else if (!strcmp(nom, ".data"))
                sect = SEC_DATA;
            else if (!strcmp(nom, ".const"))
                sect = SEC_CONST;
            else if (!strcmp(nom, ".cstring"))
                sect = SEC_CSTRING;
            else if (!strcmp(nom, ".bss"))
                sect = SEC_BSS;
            else if (!strcmp(nom, ".section")) {
                /* .section __TEXT,__text ...  */
                char seg[64], sct[64];
                lege_nom(seg, 64);
                if (matcha(','))
                    lege_nom(sct, 64);
                else
                    sct[0] = 0;
                if (!strcmp(sct, "__text"))
                    sect = SEC_TEXT;
                else if (!strcmp(sct, "__cstring"))
                    sect = SEC_CSTRING;
                else if (!strcmp(sct, "__const"))
                    sect = SEC_CONST;
                else if (!strcmp(sct, "__data"))
                    sect = SEC_DATA;
                else if (!strcmp(sct, "__bss"))
                    sect = SEC_BSS;
                else if (!strcmp(sct, "__literal8"))
                    sect = SEC_CONST;
                else if (!strcmp(sct, "__literal4"))
                    sect = SEC_CONST;
                else if (!strcmp(sct, "__literal16"))
                    sect = SEC_CONST;
                else
                    erratum_ad(linea_num, "sectio ignota: %s", sct);
            } else if (!strcmp(nom, ".globl") || !strcmp(nom, ".global")) {
                char sn[256];
                lege_nom(sn, 256);
                int si = sym_quaere_vel_crea(sn);
                symbola[si].est_globalis = 1;
                ultimus_globl_def = 1;
            } else if (!strcmp(nom, ".comm") || !strcmp(nom, ".lcomm")) {
                char sn[256];
                lege_nom(sn, 256);
                exige(',');
                long sz  = lege_num();
                long col = 1;
                if (matcha(','))
                    col = lege_num();
                int si = sym_quaere_vel_crea(sn);
                if (num_globalium >= MAX_GLOBALES)
                    erratum("nimis multae globales");
                int gi     = num_globalium++;
                const char *nn2 = (sn[0] == '_') ? sn + 1 : sn;
                strncpy(globales[gi].nomen, nn2, 255);
                globales[gi] .typus = NULL;
                globales[gi] .magnitudo = (int)sz;
                globales[gi] .colineatio = 1 << col;
                globales[gi] .est_bss = 1;
                globales[gi] .est_staticus = !strcmp(nom, ".lcomm");
                globales[gi] .valor_initialis = 0;
                globales[gi] .habet_valorem = 0;
                globales[gi] .data_offset = 0;
                symbola[si]  .genus = SYM_GLOB;
                symbola[si]  .id = gi;
                symbola[si]  .definitum = 1;
                if (!strcmp(nom, ".comm"))
                    symbola[si].est_globalis = 1;
            } else if (!strcmp(nom, ".zerofill")) {
                char seg[32], sct[32], sn[256];
                lege_nom(seg, 32);
                exige(',');
                lege_nom(sct, 32);
                exige(',');
                lege_nom(sn, 256);
                exige(',');
                long sz  = lege_num();
                long col = 0;
                if (matcha(','))
                    col = lege_num();
                int si = sym_quaere_vel_crea(sn);
                if (num_globalium >= MAX_GLOBALES)
                    erratum("nimis multae globales");
                int gi     = num_globalium++;
                const char *nn2 = (sn[0] == '_') ? sn + 1 : sn;
                strncpy(globales[gi].nomen, nn2, 255);
                globales[gi] .typus = NULL;
                globales[gi] .magnitudo = (int)sz;
                globales[gi] .colineatio = 1 << col;
                globales[gi] .est_bss = 1;
                globales[gi] .est_staticus = !symbola[si].est_globalis;
                globales[gi] .data_offset = 0;
                symbola[si]  .genus = SYM_GLOB;
                symbola[si]  .id = gi;
                symbola[si]  .definitum = 1;
                (void)seg;
                (void)sct;
            }
            saltus_ad_eol();
            continue;
        }
        /* alioquin instructio — skippa; colligimus symbola referita in pass2 */
        saltus_ad_eol();
        (void)save_p;
    }
    pos = save_pos;
    linea_num = save_linea;
    sectio_currens = save_sect;
}

/* ================================================================
 * alvei auxiliarii pro data
 * ================================================================ */

static void alinea_data(int col)
{
    if (col <= 1)
        return;
    int rem = init_data_lon % col;
    if (rem) {
        int pad = col - rem;
        if (init_data_lon + pad > MAX_DATA)
            erratum("init_data nimis magna");
        memset(init_data + init_data_lon, 0, pad);
        init_data_lon += pad;
    }
}

static void scribe_data(const void *b, int n)
{
    if (init_data_lon + n > MAX_DATA)
        erratum("init_data nimis magna");
    memcpy(init_data + init_data_lon, b, n);
    init_data_lon += n;
}

/* praesens symbolum datum (ultimum definitum in sectione data) */
static int ultimum_data_sym = -1;

/* ================================================================
 * resolutio symboli ad operandum — crea quae necesse sunt
 * ================================================================ */

/* Pro symbolō GLOB (definitum in __data/__const/__bss) sed nōndum locātum:
 * crea globalis entrātum placeholder. Ūtilis prō prō-rēferentiīs in pass2.
 * pone_labelum posterior updātābit data_offset et colineationem. */
static int glob_sym_ensure(int si)
{
    if (symbola[si].id >= 0)
        return symbola[si].id;
    if (num_globalium >= MAX_GLOBALES)
        erratum("nimis multae globales");
    int gi         = num_globalium++;
    const char *nm = symbola[si].nomen;
    const char *nn = (nm[0] == '_') ? nm + 1 : nm;
    strncpy(globales[gi].nomen, nn, 255);
    globales[gi] .nomen[255] = 0;
    globales[gi] .typus = NULL;
    globales[gi] .magnitudo = 0;
    globales[gi] .colineatio = 1;
    globales[gi] .est_bss = 0;
    globales[gi] .est_staticus = !symbola[si].est_globalis;
    globales[gi] .valor_initialis = 0;
    globales[gi] .habet_valorem = 0;
    globales[gi] .data_offset = 0;
    symbola[si]  .genus = SYM_GLOB;
    symbola[si]  .id = gi;
    return gi;
}

static int sym_got_id(int si)
{
    sym_t *s = &symbola[si];
    if (s->genus == SYM_IGNOTUS) {
        /* non definitum hic → externum */
        s ->genus = SYM_EXT;
        s ->id    = got_adde(s->nomen);
        return s->id;
    }
    if (s->genus == SYM_EXT)
        return s->id;
    /* functio vel globalis localis referita per GOT → intrans GOT nomine suo */
    if (s->genus == SYM_FUNC || s->genus == SYM_GLOB) {
        int g = got_adde(s->nomen);
        return g;
    }
    erratum_ad(linea_num, "symbolum '%s' non potest esse GOT intrans", s->nomen);
    return -1;
}

/* ================================================================
 * encodings cruciales (non in emitte.c)
 * ================================================================ */

/* STUR Xt, [Xn, #imm9] — simm9 -256..255 */
static void enc_stur64(int rt, int rn, int imm)
{
    emit32(0xF8000000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_ldur64(int rt, int rn, int imm)
{
    emit32(0xF8400000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_stur32(int rt, int rn, int imm)
{
    emit32(0xB8000000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_ldur32(int rt, int rn, int imm)
{
    emit32(0xB8400000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_sturb(int rt, int rn, int imm)
{
    emit32(0x38000000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_ldurb(int rt, int rn, int imm)
{
    emit32(0x38400000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_sturh(int rt, int rn, int imm)
{
    emit32(0x78000000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_ldurh(int rt, int rn, int imm)
{
    emit32(0x78400000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_ldursw(int rt, int rn, int imm)
{
    emit32(0xB8800000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}

/* STP Xt1, Xt2, [Xn, #imm] (positivus offset, non pre/post-index) */
static void enc_stp64(int t1, int t2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA9000000 | (imm7 << 15) | (t2 << 10) | (rn << 5) | t1);
}
static void enc_ldp64(int t1, int t2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA9400000 | (imm7 << 15) | (t2 << 10) | (rn << 5) | t1);
}
static void enc_stp64_pre(int t1, int t2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA9800000 | (imm7 << 15) | (t2 << 10) | (rn << 5) | t1);
}
static void enc_ldp64_pre(int t1, int t2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA9C00000 | (imm7 << 15) | (t2 << 10) | (rn << 5) | t1);
}
static void enc_stp64_post(int t1, int t2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA8800000 | (imm7 << 15) | (t2 << 10) | (rn << 5) | t1);
}
static void enc_ldp64_post(int t1, int t2, int rn, int imm)
{
    int imm7 = (imm / 8) & 0x7F;
    emit32(0xA8C00000 | (imm7 << 15) | (t2 << 10) | (rn << 5) | t1);
}

/* LDR/STR (register offset, extended/shifted): [Xn, Xm{, ext #s}] */
static uint32_t enc_option(int ext_genus, int rm_w)
{
    /* lsl → opt = 011 (UXTX) pro X-reg, sed clang '[Xn, Wm, sxtw ...]' */
    if (ext_genus == 0)
        return 3; /* LSL/UXTX */
    if (ext_genus == 1)
        return 6; /* SXTW */
    if (ext_genus == 2)
        return 2; /* UXTW */
    if (ext_genus == 3)
        return 7; /* SXTX */
    (void)rm_w;
    return 3;
}

/* STR Xt, [Xn, Xm {, ext #S}] */
static void enc_str64_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0xF8200800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_ldr64_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0xF8600800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_str32_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0xB8200800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_ldr32_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0xB8600800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_strb_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0x38200800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_ldrb_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0x38600800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_strh_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0x78200800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_ldrh_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0x78600800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}
static void enc_ldrsw_r(int rt, int rn, int rm, int ext_genus, int S)
{
    uint32_t opt = enc_option(ext_genus, 0);
    uint32_t s   = S ? 1 : 0;
    emit32(0xB8A00800 | (rm << 16) | (opt << 13) | (s << 12) | (rn << 5) | rt);
}

/* W-registra arithmetica */
static void enc_sub32(int rd, int rn, int rm)
{
    emit32(0x4B000000 | (rm << 16) | (rn << 5) | rd);
}
static void enc_mul32(int rd, int rn, int rm)
{
    emit32(0x1B007C00 | (rm << 16) | (rn << 5) | rd);
}
static void enc_sdiv32(int rd, int rn, int rm)
{
    emit32(0x1AC00C00 | (rm << 16) | (rn << 5) | rd);
}
static void enc_udiv32(int rd, int rn, int rm)
{
    emit32(0x1AC00800 | (rm << 16) | (rn << 5) | rd);
}
static void enc_and32(int rd, int rn, int rm)
{
    emit32(0x0A000000 | (rm << 16) | (rn << 5) | rd);
}
static void enc_orr32(int rd, int rn, int rm)
{
    emit32(0x2A000000 | (rm << 16) | (rn << 5) | rd);
}
static void enc_eor32(int rd, int rn, int rm)
{
    emit32(0x4A000000 | (rm << 16) | (rn << 5) | rd);
}
static void enc_lslv32(int rd, int rn, int rm)
{
    emit32(0x1AC02000 | (rm << 16) | (rn << 5) | rd);
}
static void enc_lsrv32(int rd, int rn, int rm)
{
    emit32(0x1AC02400 | (rm << 16) | (rn << 5) | rd);
}
static void enc_asrv32(int rd, int rn, int rm)
{
    emit32(0x1AC02800 | (rm << 16) | (rn << 5) | rd);
}
static void enc_addi32(int rd, int rn, int imm)
{
    if (imm < 0) {
        emit32(0x51000000 | ((-imm & 0xFFF) << 10) | (rn << 5) | rd);
        return;
    }
    emit32(0x11000000 | ((imm & 0xFFF) << 10) | (rn << 5) | rd);
}
static void enc_subi32(int rd, int rn, int imm)
{
    if (imm < 0) {
        enc_addi32(rd, rn, -imm);
        return;
    }
    emit32(0x51000000 | ((imm & 0xFFF) << 10) | (rn << 5) | rd);
}
static void enc_subsi32(int rd, int rn, int imm)
{
    if (imm < 0)
        emit32(0x31000000 | ((-imm & 0xFFF) << 10) | (rn << 5) | rd);
    else
        emit32(0x71000000 | ((imm & 0xFFF) << 10) | (rn << 5) | rd);
}
static void enc_addsi32(int rd, int rn, int imm)
{
    if (imm < 0) {
        enc_subsi32(rd, rn, -imm);
        return;
    }
    emit32(0x31000000 | ((imm & 0xFFF) << 10) | (rn << 5) | rd);
}
static void enc_subs32(int rd, int rn, int rm)
{
    emit32(0x6B000000 | (rm << 16) | (rn << 5) | rd);
}

/* 64-bit subs */
static void enc_subs64(int rd, int rn, int rm)
{
    emit32(0xEB000000 | (rm << 16) | (rn << 5) | rd);
}
static void enc_subsi64(int rd, int rn, int imm)
{
    if (imm < 0)
        emit32(0xB1000000 | ((-imm & 0xFFF) << 10) | (rn << 5) | rd);
    else
        emit32(0xF1000000 | ((imm & 0xFFF) << 10) | (rn << 5) | rd);
}
static void enc_addsi64(int rd, int rn, int imm)
{
    if (imm < 0) {
        enc_subsi64(rd, rn, -imm);
        return;
    }
    emit32(0xB1000000 | ((imm & 0xFFF) << 10) | (rn << 5) | rd);
}

/* ADD/SUB (imm, unchanged flags) ad SP-compatibilem formam */
static void enc_addi64(int rd, int rn, int imm)
{
    /* imm potest esse 0..4095 vel cum lsl #12 (multipla 4096) */
    if (imm < 0) {
        emit32(0xD1000000 | ((-imm & 0xFFF) << 10) | (rn << 5) | rd);
        return;
    }
    if (imm <= 4095)
        emit32(0x91000000 | (imm << 10) | (rn << 5) | rd);
    else if ((imm & 0xFFF) == 0 && imm <= 0xFFFFFF)
        emit32(0x91400000 | ((imm >> 12) << 10) | (rn << 5) | rd);
    else if (imm <= 0xFFFFFF) {
        int hi = imm & 0xFFF000;
        int lo = imm & 0xFFF;
        emit32(0x91400000 | ((hi >> 12) << 10) | (rn << 5) | rd);
        emit32(0x91000000 | (lo << 10) | (rd << 5) | rd);
    } else
        erratum_ad(linea_num, "add immediatum extra rangum: %d", imm);
}
static void enc_subi64(int rd, int rn, int imm)
{
    if (imm < 0) {
        enc_addi64(rd, rn, -imm);
        return;
    }
    if (imm <= 4095)
        emit32(0xD1000000 | (imm << 10) | (rn << 5) | rd);
    else if ((imm & 0xFFF) == 0 && imm <= 0xFFFFFF)
        emit32(0xD1400000 | ((imm >> 12) << 10) | (rn << 5) | rd);
    else if (imm <= 0xFFFFFF) {
        int hi = imm & 0xFFF000;
        int lo = imm & 0xFFF;
        emit32(0xD1400000 | ((hi >> 12) << 10) | (rn << 5) | rd);
        emit32(0xD1000000 | (lo << 10) | (rd << 5) | rd);
    } else
        erratum_ad(linea_num, "sub immediatum extra rangum: %d", imm);
}

/* MOV (registr) 32-bit: ORR Wd, WZR, Wn */
static void enc_mov32_reg(int rd, int rn)
{
    emit32(0x2A0003E0 | (rn << 16) | rd);
}
/* MOVZ/MOVK/MOVN W */
static void enc_movz32(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0x52800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}
static void enc_movk32(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0x72800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}
static void enc_movn32(int rd, uint16_t imm, int shift)
{
    int hw = shift / 16;
    emit32(0x12800000 | (hw << 21) | ((uint32_t)imm << 5) | rd);
}

/* MOV Wn, #imm (alias MOVZ/MOVN/ORR imm) — per MOVZ/MOVN/seq */
static void enc_movi32(int rd, long imm)
{
    uint32_t v = (uint32_t)(imm & 0xFFFFFFFF);
    if (v == 0) {
        enc_movz32(rd, 0, 0);
        return;
    }
    if ((int32_t)v > 0 && v < 65536) {
        enc_movz32(rd, (uint16_t)v, 0);
        return;
    }
    /* MOVN: ~v in 16 bit */
    if (((int32_t)imm) < 0 && ((uint32_t)~(uint32_t)imm & 0xFFFF0000) == 0) {
        enc_movn32(rd, (uint16_t)(~v), 0);
        return;
    }
    /* MOVZ lo, MOVK hi */
    enc_movz32(rd, (uint16_t)v, 0);
    if ((v >> 16) != 0)
        enc_movk32(rd, (uint16_t)(v >> 16), 16);
}

/* CMP Wn, Wm / CMP Wn, #imm */
static void enc_cmp32(int rn, int rm)
{
    emit32(0x6B00001F | (rm << 16) | (rn << 5));
}
static void enc_cmpi32(int rn, int imm)
{
    if (imm < 0)
        emit32(0x3100001F | ((-imm & 0xFFF) << 10) | (rn << 5));
    else
        emit32(0x7100001F | ((imm & 0xFFF) << 10) | (rn << 5));
}
/* CSET Wd, cond */
static void enc_cset32(int rd, int cond)
{
    int ci = cond ^ 1;
    emit32(0x1A9F07E0 | (ci << 12) | rd);
}

/* SXTW / UXTW variantes — iam in emitte.c (emit_sxtw/emit_uxtw).
 * SXTB/SXTH/UXTB/UXTH Wd, Wn — pone W-versions:
 * Sunt idem ut emit_sxtb etc. sed cum sf=0 in emitte etiam; video
 * emit_sxtb utitur 0x93401C00 (sf=1, X-reg form) — sed destinatio
 * est X-reg. Pro W-dest nobis aliud encoding needatur. */
static void enc_sxtb_w(int rd, int rn)
{
    /* SBFM Wd, Wn, #0, #7 */
    emit32(0x13001C00 | (rn << 5) | rd);
}
static void enc_sxth_w(int rd, int rn)
{
    emit32(0x13003C00 | (rn << 5) | rd);
}

/* NEGS (SUBS with XZR) */

/* LDRSB/LDRSH/LDRSW variants signatae vs non-signatae — emit_ldrsb in emitte.c
 * habet dest X-reg. Pro Wd dest fac alium encoding. */
static void enc_ldrsb_w(int rt, int rn, int imm)
{
    /* bit 22: 0=64, 1=32 ... rather: LDRSB 32-bit = 0x39C00000 */
    if (imm < 0 || imm > 4095) {
        int ra = (rt == 17) ? 16 : 17;
        emit_movi(ra, imm);
        emit_add(ra, rn, ra);
        emit32(0x39C00000 | (ra << 5) | rt);
        return;
    }
    emit32(0x39C00000 | ((imm & 0xFFF) << 10) | (rn << 5) | rt);
}
static void enc_ldrsh_w(int rt, int rn, int imm)
{
    if (imm < 0 || imm > 4094 || (imm & 1)) {
        int ra = (rt == 17) ? 16 : 17;
        emit_movi(ra, imm);
        emit_add(ra, rn, ra);
        emit32(0x79C00000 | (ra << 5) | rt);
        return;
    }
    emit32(0x79C00000 | ((imm / 2) << 10) | (rn << 5) | rt);
}
static void enc_ldursb_w(int rt, int rn, int imm)
{
    emit32(0x38C00000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}
static void enc_ldursh_w(int rt, int rn, int imm)
{
    emit32(0x78C00000 | ((imm & 0x1FF) << 12) | (rn << 5) | rt);
}

/* ADR Xd, label (pc-relative) */
static void enc_adr_fixup(int rd, int label_id)
{
    fixup_adde(FIX_ADR_LABEL, codex_lon, label_id, 0);
    emit32(0x10000000 | rd);
}

/* ================================================================
 * encode bitmask immediatum (logicum imm) — brute force
 * reddit 1 si inventum, 0 alioquin. *out obtinet (N<<22)|(immr<<16)|(imms<<10).
 * ================================================================ */
static int enc_bitmask_imm(uint64_t val, int sf, uint32_t *out)
{
    if (!sf)
        val &= 0xFFFFFFFFULL;
    int Nmax = sf ? 1 : 0;
    for (int N = 0; N <= Nmax; N++) {
        for (int immr = 0; immr < 64; immr++) {
            for (int imms = 0; imms < 64; imms++) {
                int top = (N << 6) | ((~imms) & 0x3F);
                int len = -1;
                for (int b = 6; b >= 0; b--) {
                    if (top & (1 << b)) {
                        len = b;
                        break;
                    }
                }
                if (len < 1)
                    continue;
                if (!sf && N == 1)
                    continue;
                int esize  = 1 << len;
                int levels = esize - 1;
                int S      = imms & levels;
                int R      = immr & levels;
                if (S == levels)
                    continue;
                uint64_t welem  = ((uint64_t)1 << (S + 1)) - 1;
                uint64_t mask   = (esize == 64) ? ~(uint64_t)0 : (((uint64_t)1 << esize) - 1);
                uint64_t rored  = ((welem >> R) | (welem << (esize - R))) & mask;
                uint64_t result = 0;
                for (int i = 0; i < 64; i += esize)
                    result |= rored << i;
                if (!sf)
                    result &= 0xFFFFFFFFULL;
                if (result == val) {
                    *out = ((uint32_t)N << 22) | ((uint32_t)immr << 16) | ((uint32_t)imms << 10);
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* MSUB Xd, Xn, Xm, Xa (pro negationibus, etc.) */
static void enc_msub64(int rd, int rn, int rm, int ra)
{
    emit32(0x9B008000 | (rm << 16) | (ra << 10) | (rn << 5) | rd);
}
static void enc_madd64(int rd, int rn, int rm, int ra)
{
    emit32(0x9B000000 | (rm << 16) | (ra << 10) | (rn << 5) | rd);
}
static void enc_madd32(int rd, int rn, int rm, int ra)
{
    emit32(0x1B000000 | (rm << 16) | (ra << 10) | (rn << 5) | rd);
}
static void enc_msub32(int rd, int rn, int rm, int ra)
{
    emit32(0x1B008000 | (rm << 16) | (ra << 10) | (rn << 5) | rd);
}

/* ================================================================
 * pass 2: parse et emit
 * ================================================================ */

/* directivae datorum */
static void dir_long(void)
{
    long v = lege_num();
    /* si destinatio est __cstring, impossibile */
    if (sectio_currens == SEC_CSTRING)
        erratum_ad(linea_num, ".long in __cstring illicitus");
    alinea_data(4);
    uint32_t u = (uint32_t)v;
    scribe_data(&u, 4);
}
static void dir_quad(void)
{
    /* potest esse numerus vel symbolum referens */
    saltus_spatii();
    if (est_initium_nom(spectus())) {
        char sn[256];
        lege_nom(sn, 256);
        alinea_data(8);
        int si        = sym_quaere_vel_crea(sn);
        sym_t         *s = &symbola[si];
        uint64_t zero = 0;
        int data_off  = init_data_lon;
        scribe_data(&zero, 8);
        if (s->genus == SYM_CHORDA) {
            int coff = chordae[s->id].offset;
            data_reloc_adde(data_off, DR_CSTRING, coff);
        } else if (s->genus == SYM_IGNOTUS) {
            /* symbolum externum (functio vel globalis in alia .o):
             * emitte relocationem externam per GOT */
            int gid         = got_adde(s->nomen);
            s       ->genus = SYM_EXT;
            s       ->id    = gid;
            data_reloc_adde(data_off, DR_EXT_FUNC, gid);
        } else if (s->genus == SYM_EXT) {
            data_reloc_adde(data_off, DR_EXT_FUNC, s->id);
        } else if (s->genus == SYM_GLOB) {
            int gid = glob_sym_ensure(si);
            data_reloc_adde(data_off, DR_IDATA, globales[gid].data_offset);
        } else if (s->genus == SYM_FUNC) {
            /* genera.c conventio: target = text offset, non label id */
            if (s->id < 0) {
                const char *nn2 = (sn[0] == '_') ? sn + 1 : sn;
                (void)func_loc_adde(nn2, !s->est_globalis);
                s->id = num_func_loc - 1;
            }
            int lab = func_loci[s->id].label;
            data_reloc_adde(data_off, DR_TEXT, labels[lab]);
        } else {
            erratum_ad(linea_num, ".quad symbolum '%s' genus non supp", sn);
        }
        return;
    }
    long v = lege_num();
    alinea_data(8);
    uint64_t u = (uint64_t)v;
    scribe_data(&u, 8);
}
static void dir_short(void)
{
    long v = lege_num();
    alinea_data(2);
    uint16_t u = (uint16_t)v;
    scribe_data(&u, 2);
}
static void dir_byte(void)
{
    long v    = lege_num();
    uint8_t u = (uint8_t)v;
    scribe_data(&u, 1);
}
static void dir_asciz(int addo_nul)
{
    char buf[4096];
    int n = parse_chorda_lit(buf, 4096);
    if (sectio_currens == SEC_CSTRING) {
        /* adde chordam — ordo debet concordare cum pass1 */
        int cid = chorda_adde(buf, n);
        if (
            ultimum_data_sym >= 0
            && symbola[ultimum_data_sym].genus == SYM_CHORDA
            && symbola[ultimum_data_sym].id != cid
        ) {
            erratum_ad(
                linea_num,
                "ordo chordarum: exspectavi id=%d, accepit id=%d",
                symbola[ultimum_data_sym].id, cid
            );
        }
    } else {
        scribe_data(buf, n);
        if (addo_nul) {
            uint8_t z = 0;
            scribe_data(&z, 1);
        }
    }
}
static void dir_zero_or_space(void)
{
    long n = lege_num();
    if (init_data_lon + n > MAX_DATA)
        erratum("init_data nimis magna");
    memset(init_data + init_data_lon, 0, n);
    init_data_lon += n;
}
static void dir_p2align(void)
{
    long a = lege_num();
    int col = 1 << a;
    colineatio_currens = col;
    if (sectio_currens != SEC_TEXT && sectio_currens != SEC_CSTRING)
        alinea_data(col);
    /* potentialiter ", value" — ignora */
    if (matcha(','))
        lege_num();
}

/* definitio labeli cum effectu in alveis */
static void pone_labelum(const char *nom)
{
    int si      = sym_quaere_vel_crea(nom);
    symbola[si] .definitum = 1;
    if (sectio_currens == SEC_TEXT) {
        int lab_id;
        if (nom[0] == '_') {
            /* functio */
            if (symbola[si].id < 0) {
                int lab     = func_loc_adde(nom + 1, !symbola[si].est_globalis);
                symbola[si] .genus = SYM_FUNC;
                symbola[si] .id = num_func_loc - 1;
                lab_id      = lab;
            } else {
                lab_id = func_loci[symbola[si].id].label;
            }
        } else {
            if (symbola[si].id < 0)
                symbola[si].id = label_novus();
            symbola[si] .genus = SYM_TXT_LAB;
            lab_id      = symbola[si].id;
        }
        label_pone(lab_id);
    } else if (sectio_currens == SEC_CSTRING) {
        /* id assignabitur cum .asciz sequitur */
        symbola[si]      .genus = SYM_CHORDA;
        ultimum_data_sym        = si;
    } else {
        /* data/const/bss — globalis. Sī iam placeholder creātum per
         * glob_sym_ensure, updā data_offset et colineationem nunc. */
        int gi       = glob_sym_ensure(si);
        globales[gi] .colineatio = colineatio_currens;
        globales[gi] .est_bss = (sectio_currens == SEC_BSS);
        globales[gi] .est_staticus = !symbola[si].est_globalis;
        alinea_data(colineatio_currens);
        globales[gi]     .data_offset = init_data_lon;
        symbola[si]      .genus = SYM_GLOB;
        ultimum_data_sym = si;
    }
}

/* ================================================================
 * instructio handlers
 * ================================================================ */

static void ins_mov(void)
{
    int w1, w2;
    int rd    = parse_reg(&w1);
    int rd_sp = (reg_kind_ult == 1);
    exige(',');
    saltus_spatii();
    if (spectus() == '#') {
        long v = lege_imm();
        if (w1)
            enc_movi32(rd, v);
        else
            emit_movi(rd, v);
        return;
    }
    int rn    = parse_reg(&w2);
    int rn_sp = (reg_kind_ult == 1);
    if (w1 != w2)
        erratum_ad(linea_num, "mov: mixta magnitudo");
    if (rd_sp || rn_sp) {
        /* MOV to/from SP = ADD Rd, Rn, #0 */
        if (w1)
            enc_addi32(rd, rn, 0);
        else
            enc_addi64(rd, rn, 0);
        return;
    }
    if (w1)
        enc_mov32_reg(rd, rn);
    else
        emit_mov(rd, rn);
}

static void ins_mov_variant(const char *op)
{
    /* movz/movk/movn Rd, #imm {, lsl #N} */
    int w;
    int rd = parse_reg(&w);
    exige(',');
    long imm  = lege_imm();
    int shift = 0;
    if (matcha(',')) {
        char sh[16];
        lege_nom(sh, 16);
        if (strcmp(sh, "lsl") != 0)
            erratum_ad(linea_num, "%s: exspectavi lsl", op);
        saltus_spatii();
        shift = (int)lege_imm();
    }
    if (!strcmp(op, "movz")) {
        if (w)
            enc_movz32(rd, (uint16_t)imm, shift);
        else
            emit_movz(rd, (uint16_t)imm, shift);
    } else if (!strcmp(op, "movk")) {
        if (w)
            enc_movk32(rd, (uint16_t)imm, shift);
        else
            emit_movk(rd, (uint16_t)imm, shift);
    } else if (!strcmp(op, "movn")) {
        if (w)
            enc_movn32(rd, (uint16_t)imm, shift);
        else
            emit_movn(rd, (uint16_t)imm, shift);
    }
}

static void ins_add_sub_generic(int est_sub, int est_s)
{
    int w1, w2, w3;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    saltus_spatii();
    if (spectus() == '#') {
        long imm = lege_imm();
        int sh   = 0;
        if (matcha(',')) {
            char nm[16];
            lege_nom(nm, 16);
            if (strcmp(nm, "lsl") != 0)
                erratum_ad(linea_num, "add/sub: exspectavi lsl");
            saltus_spatii();
            sh = (int)lege_imm();
        }
        long eff = imm << sh;
        if (w1) {
            if (est_s && est_sub)
                enc_subsi32(rd, rn, eff);
            else if (est_s)
                enc_addsi32(rd, rn, eff);
            else if (est_sub)
                enc_subi32(rd, rn, eff);
            else
                enc_addi32(rd, rn, eff);
        } else {
            if (est_s && est_sub)
                enc_subsi64(rd, rn, eff);
            else if (est_s)
                enc_addsi64(rd, rn, eff);
            else if (est_sub)
                enc_subi64(rd, rn, eff);
            else
                enc_addi64(rd, rn, eff);
        }
        return;
    }
    if (!est_prox_registrum()) {
        /* forma: add Rd, Rn, sym@PAGEOFF */
        char sn[256], rel[32];
        lege_sym_cum_rel(sn, 256, rel, 32);
        if (est_sub || est_s)
            erratum_ad(linea_num, "sub/adds/subs cum @%s invalidum", rel);
        if (strcmp(rel, "PAGEOFF") != 0)
            erratum_ad(linea_num, "add @%s non supportatum", rel);
        int si    = sym_quaere_vel_crea(sn);
        sym_t  *s = &symbola[si];
        if (s->genus == SYM_CHORDA) {
            fixup_adde(FIX_ADD_LO12, codex_lon, s->id, 0);
            emit32(0x91000000 | (rn << 5) | rd);
        } else if (s->genus == SYM_GLOB) {
            int gid = glob_sym_ensure(si);
            fixup_adde(FIX_ADD_LO12_DATA, codex_lon, gid, 0);
            emit32(0x91000000 | (rn << 5) | rd);
        } else if (s->genus == SYM_FUNC) {
            if (s->id < 0) {
                const char *nn2 = (sn[0] == '_') ? sn + 1 : sn;
                (void)func_loc_adde(nn2, !s->est_globalis);
                s->id = num_func_loc - 1;
            }
            fixup_adde(FIX_ADD_LO12_TEXT, codex_lon, func_loci[s->id].label, 0);
            emit32(0x91000000 | (rn << 5) | rd);
        } else if (s->genus == SYM_IGNOTUS) {
            int gid = glob_sym_ensure(si);
            fixup_adde(FIX_ADD_LO12_DATA, codex_lon, gid, 0);
            emit32(0x91000000 | (rn << 5) | rd);
        } else {
            erratum_ad(linea_num, "add @PAGEOFF: symbolum '%s' non supportatus", sn);
        }
        (void)w1;
        (void)w2;
        return;
    }
    int rm = parse_reg(&w3);
    if (w1 != w2)
        erratum_ad(linea_num, "add/sub: mixta magnitudo");
    int sh_type = 0; /* 0=LSL, 1=LSR, 2=ASR */
    int sh_amt  = 0;
    int ext_opt = -1; /* sī non-negātīvus, ūtimur extended-register forma */
    if (matcha(',')) {
        char nm[16];
        lege_nom(nm, 16);
        if (!strcmp(nm, "lsl"))
            sh_type = 0;
        else if (!strcmp(nm, "lsr"))
            sh_type = 1;
        else if (!strcmp(nm, "asr"))
            sh_type = 2;
        else if (!strcmp(nm, "uxtb"))
            ext_opt = 0;
        else if (!strcmp(nm, "uxth"))
            ext_opt = 1;
        else if (!strcmp(nm, "uxtw"))
            ext_opt = 2;
        else if (!strcmp(nm, "uxtx"))
            ext_opt = 3;
        else if (!strcmp(nm, "sxtb"))
            ext_opt = 4;
        else if (!strcmp(nm, "sxth"))
            ext_opt = 5;
        else if (!strcmp(nm, "sxtw"))
            ext_opt = 6;
        else if (!strcmp(nm, "sxtx"))
            ext_opt = 7;
        else
            erratum_ad(linea_num, "add/sub: shift ignotus '%s'", nm);
        saltus_spatii();
        if (spectus() == '#')
            sh_amt = (int)lege_imm();
    }
    if (ext_opt >= 0) {
        /* ADD/SUB extended register */
        uint32_t base;
        if (w1)
            base = est_sub ? (est_s ? 0x6B200000 : 0x4B200000)
                : (est_s ? 0x2B200000 : 0x0B200000);
        else
            base = est_sub ? (est_s ? 0xEB200000 : 0xCB200000)
                : (est_s ? 0xAB200000 : 0x8B200000);
        emit32(
            base | (rm << 16) | ((uint32_t)ext_opt << 13)
            | ((sh_amt & 0x7) << 10) | (rn << 5) | rd
        );
        return;
    }
    uint32_t base;
    if (w1)
        base = est_sub ? (est_s ? 0x6B000000 : 0x4B000000)
            : (est_s ? 0x2B000000 : 0x0B000000);
    else
        base = est_sub ? (est_s ? 0xEB000000 : 0xCB000000)
            : (est_s ? 0xAB000000 : 0x8B000000);
    emit32(
        base | ((uint32_t)sh_type << 22) | (rm << 16)
        | ((sh_amt & 0x3F) << 10) | (rn << 5) | rd
    );
    (void)w3;
}

static void ins_mul(int est_sub /*msub-like variant unused*/)
{
    (void)est_sub;
    int w1, w2, w3;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    int rm = parse_reg(&w3);
    if (w1)
        enc_mul32(rd, rn, rm);
    else
        emit_mul(rd, rn, rm);
}

static void ins_div(int signed_div)
{
    int w1, w2, w3;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    int rm = parse_reg(&w3);
    if (w1) {
        if (signed_div)
            enc_sdiv32(rd, rn, rm);
        else
            enc_udiv32(rd, rn, rm);
    } else {
        if (signed_div)
            emit_sdiv(rd, rn, rm);
        else
            emit_udiv(rd, rn, rm);
    }
}

static void ins_logic(const char *op)
{
    int w1, w2, w3;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    saltus_spatii();
    if (spectus() == '#') {
        long imm = lege_imm();
        int is_shift =
            !strcmp(op, "lsl") || !strcmp(op, "lslv")
            || !strcmp(op, "lsr") || !strcmp(op, "lsrv")
            || !strcmp(op, "asr") || !strcmp(op, "asrv");
        if (is_shift) {
            /* LSL/LSR/ASR immediatum — aliae formae UBFM/SBFM */
            int width = w1 ? 32 : 64;
            int k = (int)(imm & (width - 1));
            uint32_t sf_bit = w1 ? 0 : (0xD3400000 ^ 0x53000000);
            /* base 32-bit: UBFM = 0x53000000, SBFM = 0x13000000 */
            if (!strcmp(op, "lsl") || !strcmp(op, "lslv")) {
                int immr      = (width - k) & (width - 1);
                int imms      = width - 1 - k;
                uint32_t base = 0x53000000;
                if (!w1)
                    base = 0xD3400000;
                emit32(base | ((immr & 0x3F) << 16) | ((imms & 0x3F) << 10) | (rn << 5) | rd);
            } else if (!strcmp(op, "lsr") || !strcmp(op, "lsrv")) {
                int immr      = k;
                int imms      = width - 1;
                uint32_t base = 0x53000000;
                if (!w1)
                    base = 0xD3400000;
                emit32(base | ((immr & 0x3F) << 16) | ((imms & 0x3F) << 10) | (rn << 5) | rd);
            } else {
                int immr      = k;
                int imms      = width - 1;
                uint32_t base = 0x13000000;
                if (!w1)
                    base = 0x9340FC00 & ~(uint32_t)0xFC00;
                /* base 64-bit SBFM: sf=1,N=1 → 0x93400000 */
                if (!w1)
                    base = 0x93400000;
                emit32(base | ((immr & 0x3F) << 16) | ((imms & 0x3F) << 10) | (rn << 5) | rd);
            }
            (void)sf_bit;
            return;
        }
        /* and/orr/eor cum immediato bitmask */
        uint32_t fields;
        if (!enc_bitmask_imm((uint64_t)imm, w1 ? 0 : 1, &fields))
            erratum_ad(linea_num, "%s: immediatum #%ld non est bitmask valida", op, imm);
        uint32_t base = 0;
        if (!strcmp(op, "and"))
            base = w1 ? 0x12000000 : 0x92000000;
        else if (!strcmp(op, "orr"))
            base = w1 ? 0x32000000 : 0xB2000000;
        else if (!strcmp(op, "eor"))
            base = w1 ? 0x52000000 : 0xD2000000;
        else
            erratum_ad(linea_num, "%s cum immediato non supp", op);
        emit32(base | fields | (rn << 5) | rd);
        return;
    }
    int rm      = parse_reg(&w3);
    int sh_type = 0, sh_amt = 0;
    if (matcha(',')) {
        char nm[16];
        lege_nom(nm, 16);
        if (!strcmp(nm, "lsl"))
            sh_type = 0;
        else if (!strcmp(nm, "lsr"))
            sh_type = 1;
        else if (!strcmp(nm, "asr"))
            sh_type = 2;
        else if (!strcmp(nm, "ror"))
            sh_type = 3;
        else
            erratum_ad(linea_num, "%s: shift ignotus '%s'", op, nm);
        saltus_spatii();
        if (spectus() == '#')
            sh_amt = (int)lege_imm();
    }
    if (sh_amt || sh_type) {
        /* encode logicum shifted reg */
        uint32_t base32 = 0, base64 = 0;
        if (!strcmp(op, "and")) {
            base32 = 0x0A000000;
            base64 = 0x8A000000;
        } else if (!strcmp(op, "orr")) {
            base32 = 0x2A000000;
            base64 = 0xAA000000;
        } else if (!strcmp(op, "eor")) {
            base32 = 0x4A000000;
            base64 = 0xCA000000;
        } else
            erratum_ad(linea_num, "%s: shift non supp cum hoc op", op);
        uint32_t base = w1 ? base32 : base64;
        emit32(
            base | ((uint32_t)sh_type << 22) | (rm << 16)
            | ((sh_amt & 0x3F) << 10) | (rn << 5) | rd
        );
        return;
    }
    if (w1) {
        if (!strcmp(op, "and"))
            enc_and32(rd, rn, rm);
        else if (!strcmp(op, "orr"))
            enc_orr32(rd, rn, rm);
        else if (!strcmp(op, "eor"))
            enc_eor32(rd, rn, rm);
        else if (!strcmp(op, "lsl") || !strcmp(op, "lslv"))
            enc_lslv32(rd, rn, rm);
        else if (!strcmp(op, "lsr") || !strcmp(op, "lsrv"))
            enc_lsrv32(rd, rn, rm);
        else if (!strcmp(op, "asr") || !strcmp(op, "asrv"))
            enc_asrv32(rd, rn, rm);
    } else {
        if (!strcmp(op, "and"))
            emit_and(rd, rn, rm);
        else if (!strcmp(op, "orr"))
            emit_orr(rd, rn, rm);
        else if (!strcmp(op, "eor"))
            emit_eor(rd, rn, rm);
        else if (!strcmp(op, "lsl") || !strcmp(op, "lslv"))
            emit_lsl(rd, rn, rm);
        else if (!strcmp(op, "lsr") || !strcmp(op, "lsrv"))
            emit_lsr(rd, rn, rm);
        else if (!strcmp(op, "asr") || !strcmp(op, "asrv"))
            emit_asr(rd, rn, rm);
    }
}

static void ins_neg(int est_s)
{
    int w1, w2;
    int rd = parse_reg(&w1);
    exige(',');
    int rm = parse_reg(&w2);
    /* optional shift — ignora pro simplicitate */
    if (matcha(',')) {
        char nm[16];
        lege_nom(nm, 16);
        saltus_spatii();
        if (spectus() == '#')
            lege_imm();
    }
    if (w1) {
        if (est_s)
            enc_subs32(rd, 31, rm);
        else
            enc_sub32(rd, 31, rm);
    } else {
        if (est_s)
            enc_subs64(rd, 31, rm);
        else
            emit_sub(rd, 31, rm);
    }
}

static void ins_mvn(void)
{
    int w1, w2;
    int rd = parse_reg(&w1);
    exige(',');
    int rm = parse_reg(&w2);
    if (w1)
        emit32(0x2A2003E0 | (rm << 16) | rd);
    else
        emit_mvn(rd, rm);
}

static void ins_cmp(int est_n /* cmn si 1 */)
{
    int w1, w2;
    int rn = parse_reg(&w1);
    exige(',');
    saltus_spatii();
    if (spectus() == '#') {
        long imm = lege_imm();
        if (w1) {
            if (est_n)
                emit32(0x31000000 | ((imm & 0xFFF) << 10) | (rn << 5) | 31);
            else
                enc_cmpi32(rn, (int)imm);
        } else {
            if (est_n)
                emit32(0xB1000000 | ((imm & 0xFFF) << 10) | (rn << 5) | 31);
            else
                emit_cmpi(rn, (int)imm);
        }
        return;
    }
    int rm = parse_reg(&w2);
    if (matcha(',')) {
        char nm[16];
        lege_nom(nm, 16);
        saltus_spatii();
        if (spectus() == '#')
            lege_imm();
    }
    if (w1) {
        if (est_n)
            emit32(0x2B00001F | (rm << 16) | (rn << 5));
        else
            enc_cmp32(rn, rm);
    } else {
        if (est_n)
            emit32(0xAB00001F | (rm << 16) | (rn << 5));
        else
            emit_cmp(rn, rm);
    }
}

static void ins_tst(void)
{
    /* TST Rn, Rm (= ANDS XZR) — imm form tamen complexa */
    int w1, w2;
    int rn = parse_reg(&w1);
    exige(',');
    saltus_spatii();
    if (spectus() == '#')
        erratum_ad(linea_num, "tst cum immediato nondum supportatum");
    int rm = parse_reg(&w2);
    if (w1)
        emit32(0x6A00001F | (rm << 16) | (rn << 5));
    else
        emit32(0xEA00001F | (rm << 16) | (rn << 5));
}

static void ins_cset(void)
{
    int w1;
    int rd = parse_reg(&w1);
    exige(',');
    char c[8];
    lege_nom(c, 8);
    int cond = parse_cond(c);
    if (cond < 0)
        erratum_ad(linea_num, "cset: condicio ignota '%s'", c);
    if (w1)
        enc_cset32(rd, cond);
    else
        emit_cset(rd, cond);
}

static void ins_csel(void)
{
    /* CSEL Rd, Rn, Rm, cond */
    int w1, w2, w3;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    int rm = parse_reg(&w3);
    exige(',');
    char c[8];
    lege_nom(c, 8);
    int cond = parse_cond(c);
    if (cond < 0)
        erratum_ad(linea_num, "csel: cond");
    if (w1)
        emit32(0x1A800000 | (rm << 16) | (cond << 12) | (rn << 5) | rd);
    else
        emit32(0x9A800000 | (rm << 16) | (cond << 12) | (rn << 5) | rd);
}

static void ins_csinc(void)
{
    int w1, w2, w3;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    int rm = parse_reg(&w3);
    exige(',');
    char c[8];
    lege_nom(c, 8);
    int cond = parse_cond(c);
    if (cond < 0)
        erratum_ad(linea_num, "csinc: cond");
    if (w1)
        emit32(0x1A800400 | (rm << 16) | (cond << 12) | (rn << 5) | rd);
    else
        emit32(0x9A800400 | (rm << 16) | (cond << 12) | (rn << 5) | rd);
}

static void ins_ext(const char *op)
{
    /* sxtw/uxtw/sxtb/sxth/uxtb/uxth */
    int w1, w2;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    if (!strcmp(op, "sxtw"))
        emit_sxtw(rd, rn);
    else if (!strcmp(op, "uxtw"))
        emit_uxtw(rd, rn);
    else if (!strcmp(op, "sxtb")) {
        if (w1)
            enc_sxtb_w(rd, rn);
        else
            emit_sxtb(rd, rn);
    } else if (!strcmp(op, "sxth")) {
        if (w1)
            enc_sxth_w(rd, rn);
        else
            emit_sxth(rd, rn);
    } else if (!strcmp(op, "uxtb"))
        emit_uxtb(rd, rn);
    else if (!strcmp(op, "uxth"))
        emit_uxth(rd, rn);
}

/* ================================================================
 * rami et controllum
 * ================================================================ */

static void ins_b(int est_bl)
{
    char nom[256];
    lege_nom(nom, 256);
    int si    = sym_quaere_vel_crea(nom);
    sym_t  *s = &symbola[si];
    if (s->genus == SYM_TXT_LAB) {
        if (s->id < 0)
            s->id = label_novus();
        if (est_bl)
            emit_bl_label(s->id);
        else
            emit_b_label(s->id);
    } else if (s->genus == SYM_FUNC) {
        /* forward ref: functio nondum definita — praealloca func_loc entry */
        if (s->id < 0) {
            const char *nn = (nom[0] == '_') ? nom + 1 : nom;
            (void)func_loc_adde(nn, !s->est_globalis);
            s->id = num_func_loc - 1;
        }
        int lab = func_loci[s->id].label;
        if (est_bl)
            emit_bl_label(lab);
        else
            emit_b_label(lab);
    } else if (s->genus == SYM_IGNOTUS) {
        if (est_bl) {
            /* ad externum — emit FIX_BL_EXT */
            s ->genus = SYM_EXT;
            s ->id    = got_adde(nom);
            fixup_adde(FIX_BL_EXT, codex_lon, s->id, 0);
            emit32(0x94000000);
        } else {
            /* b ad ignotum: adsumimus labelem futurum */
            s ->genus = SYM_TXT_LAB;
            s ->id    = label_novus();
            emit_b_label(s->id);
        }
    } else if (s->genus == SYM_EXT) {
        if (!est_bl)
            erratum_ad(linea_num, "b ad externum illicit");
        fixup_adde(FIX_BL_EXT, codex_lon, s->id, 0);
        emit32(0x94000000);
    } else {
        erratum_ad(linea_num, "b/bl: symbolum '%s' invalidum", nom);
    }
}

static void ins_bcond(int cond)
{
    char nom[256];
    lege_nom(nom, 256);
    int si    = sym_quaere_vel_crea(nom);
    sym_t  *s = &symbola[si];
    if (s->genus == SYM_TXT_LAB || s->genus == SYM_IGNOTUS) {
        if (s->id < 0)
            s->id = label_novus();
        s->genus = SYM_TXT_LAB;
        emit_bcond_label(cond, s->id);
    } else
        erratum_ad(linea_num, "b.cond: symbolum invalidum");
}

static void ins_cbz_cbnz(int est_cbnz)
{
    int w1;
    int rt = parse_reg(&w1);
    exige(',');
    char nom[256];
    lege_nom(nom, 256);
    int si    = sym_quaere_vel_crea(nom);
    sym_t  *s = &symbola[si];
    if (s->id < 0)
        s->id = label_novus();
    s->genus = SYM_TXT_LAB;
    /* clang utitur CBZ/CBNZ utrumque 32-bit et 64-bit — hic utimur
     * 64-bit encoding per emit_cbz/cbnz. Pro W-reg, sf=0 flag. */
    if (est_cbnz)
        fixup_adde(FIX_CBNZ, codex_lon, s->id, 0);
    else
        fixup_adde(FIX_CBZ, codex_lon, s->id, 0);
    uint32_t sf   = w1 ? 0 : 0x80000000;
    uint32_t base = est_cbnz ? 0x35000000 : 0x34000000;
    emit32(sf | base | rt);
}

static void ins_tbz_tbnz(int est_tbnz)
{
    int w1;
    int rt = parse_reg(&w1);
    exige(',');
    long bit = lege_imm();
    exige(',');
    char nom[256];
    lege_nom(nom, 256);
    int si    = sym_quaere_vel_crea(nom);
    sym_t  *s = &symbola[si];
    if (s->id < 0)
        s->id = label_novus();
    s->genus = SYM_TXT_LAB;
    fixup_adde(est_tbnz ? FIX_TBNZ : FIX_TBZ, codex_lon, s->id, (int)bit);
    uint32_t base = est_tbnz ? 0x37000000 : 0x36000000;
    emit32(base | rt);
    (void)w1;
}

static void ins_ret(void)
{
    /* optional Xn */
    saltus_spatii();
    if (!est_eol()) {
        int w;
        (void)parse_reg(&w);
    }
    emit_ret();
}

static void ins_br_blr(int est_blr)
{
    int w;
    int rn = parse_reg(&w);
    if (est_blr)
        emit_blr(rn);
    else
        emit32(0xD61F0000 | (rn << 5));
}

/* ADRP Xd, symbol@PAGE */
static void ins_adrp(void)
{
    int w;
    int rd = parse_reg(&w);
    exige(',');
    char sn[256], rel[32];
    lege_sym_cum_rel(sn, 256, rel, 32);
    int si    = sym_quaere_vel_crea(sn);
    sym_t  *s = &symbola[si];
    if (!strcmp(rel, "PAGE")) {
        if (s->genus == SYM_CHORDA) {
            emit_adrp_fixup(rd, FIX_ADRP, s->id);
        } else if (s->genus == SYM_GLOB) {
            int gid = glob_sym_ensure(si);
            emit_adrp_fixup(rd, FIX_ADRP_DATA, gid);
        } else if (s->genus == SYM_FUNC) {
            /* adresse functionis localis per @PAGE/@PAGEOFF.
             * Si forward-ref, praealloca func_loc. */
            if (s->id < 0) {
                const char *nn2 = (sn[0] == '_') ? sn + 1 : sn;
                (void)func_loc_adde(nn2, !s->est_globalis);
                s->id = num_func_loc - 1;
            }
            emit_adrp_fixup(rd, FIX_ADRP_TEXT, func_loci[s->id].label);
        } else if (s->genus == SYM_IGNOTUS) {
            /* symbolum non definitum adhuc — suspicamur globalem
             * (functiones vel definita erunt in pass2 vel extern) */
            int gid = glob_sym_ensure(si);
            emit_adrp_fixup(rd, FIX_ADRP_DATA, gid);
        } else
            erratum_ad(linea_num, "adrp @PAGE: symbolum ext — adhibe @GOTPAGE");
    } else if (!strcmp(rel, "GOTPAGE")) {
        int gid = sym_got_id(si);
        emit_adrp_fixup(rd, FIX_ADRP_GOT, gid);
    } else
        erratum_ad(linea_num, "adrp relocatio '%s' ignota", rel);
}

static void ins_adr(void)
{
    int w;
    int rd = parse_reg(&w);
    exige(',');
    char sn[256], rel[32];
    lege_sym_cum_rel(sn, 256, rel, 32);
    if (rel[0])
        erratum_ad(linea_num, "adr cum @%s non supportatur", rel);
    int si    = sym_quaere_vel_crea(sn);
    sym_t  *s = &symbola[si];
    if (s->genus != SYM_TXT_LAB && s->genus != SYM_IGNOTUS)
        erratum_ad(linea_num, "adr: symbolum '%s' non label", sn);
    if (s->id < 0)
        s->id = label_novus();
    s->genus = SYM_TXT_LAB;
    enc_adr_fixup(rd, s->id);
}

/* ================================================================
 * LDR/STR dispatch — handle both imm and reg forms
 *
 * Formae:
 *   LDR Xt, [Xn, #imm]                         — unsigned offset
 *   LDR Xt, [Xn, Xm {, lsl #N}]               — register offset
 *   LDR Xt, [Xn, #imm]!   / STR [Xn,#imm]!   — pre-index
 *   LDR Xt, [Xn], #imm                         — post-index
 *   LDUR/STUR Xt, [Xn, #imm]                  — signed 9-bit
 *   LDR Xt, [Xn, _sym@...OFF]                 — via fixup
 * ================================================================ */

static void enc_ldr_str_mem(
    int est_store, int rt, int is_w,
    mem_t *m, int magn, int signed_ld
) {
    /*
     * magn: 8, 4, 2, 1 octeti.
     * signed_ld: 1 si LDRSW/LDRSB/LDRSH (sign-extend).
     * is_w: destinatio est W-reg (ergo 32-bit result, signed_ld => SW/SB/SH
     *        to W; pro mag==8 impossibile cum W).
     */
    if (m->habet_rel_sym) {
        /* fixup: e.g. ldr x8, [x8, _sym@GOTPAGEOFF] */
        if (!strcmp(m->rel, "GOTPAGEOFF") && magn == 8 && !est_store) {
            int si  = sym_quaere_vel_crea(m->rel_sym);
            int gid = sym_got_id(si);
            fixup_adde(FIX_LDR_GOT_LO12, codex_lon, gid, 8);
            emit32(0xF9400000 | (m->rn << 5) | rt);
            return;
        }
        if (!strcmp(m->rel, "PAGEOFF")) {
            int si    = sym_quaere_vel_crea(m->rel_sym);
            sym_t  *s = &symbola[si];
            if (s->genus == SYM_GLOB || s->genus == SYM_IGNOTUS) {
                int gid = glob_sym_ensure(si);
                if (est_store) {
                    fixup_adde(FIX_STR_LO12_DATA, codex_lon, gid, magn);
                } else {
                    fixup_adde(FIX_LDR_LO12_DATA, codex_lon, gid, magn);
                }
                /* emit placeholder ldr/str Xt, [Xn, #0] — scribo assumes generic */
                uint32_t op;
                if (magn == 8)
                    op = est_store ? 0xF9000000 : 0xF9400000;
                else if (magn == 4)
                    op = est_store ? 0xB9000000 : (signed_ld ? 0xB9800000 : 0xB9400000);
                else if (magn == 2)
                    op = est_store ? 0x79000000 : (signed_ld ? 0x79800000 : 0x79400000);
                else
                    op = est_store ? 0x39000000 : (signed_ld ? 0x39800000 : 0x39400000);
                emit32(op | (m->rn << 5) | rt);
                return;
            }
            erratum_ad(
                linea_num,
                "ldr/str @PAGEOFF ad non-data symbolum '%s'", m->rel_sym
            );
        }
        erratum_ad(linea_num, "ldr/str cum @%s", m->rel);
    }
    if (m->post_index || m->pre_index) {
        /* pre/post-index unus 64/32-bit forma */
        if (magn != 8 && magn != 4)
            erratum_ad(linea_num, "pre/post-index solum pro 64/32-bit");
        int imm = (int)m->imm;
        /* encoding: LDR/STR (immediate) pre-index: imm9 << 12 | 01 << 10
                                                   post-index: 01<<10 sine pre */
        uint32_t base;
        if (est_store)
            base = magn == 8 ? 0xF8000000 : 0xB8000000;
        else
            base = magn == 8 ? 0xF8400000 : (signed_ld ? 0xB8800000 : 0xB8400000);
        uint32_t mode = m->pre_index ? 0xC00 : 0x400; /* pre: 11, post: 01 at bits[11:10] */
        emit32(base | ((imm & 0x1FF) << 12) | mode | (m->rn << 5) | rt);
        (void)is_w;
        return;
    }
    if (m->habet_rm) {
        /* register offset */
        int S = m->ext_shift;
        if (S < 0)
            S = 0;
        if (magn == 8) {
            if (est_store)
                enc_str64_r(rt, m->rn, m->rm, m->ext_genus, S);
            else if (signed_ld)
                erratum_ad(linea_num, "ldrsw reg-form unsupp");
            else
                enc_ldr64_r(rt, m->rn, m->rm, m->ext_genus, S);
        } else if (magn == 4) {
            if (est_store)
                enc_str32_r(rt, m->rn, m->rm, m->ext_genus, S);
            else if (signed_ld)
                enc_ldrsw_r(rt, m->rn, m->rm, m->ext_genus, S);
            else
                enc_ldr32_r(rt, m->rn, m->rm, m->ext_genus, S);
        } else if (magn == 2) {
            if (est_store)
                enc_strh_r(rt, m->rn, m->rm, m->ext_genus, S);
            else
                enc_ldrh_r(rt, m->rn, m->rm, m->ext_genus, S);
        } else {
            if (est_store)
                enc_strb_r(rt, m->rn, m->rm, m->ext_genus, S);
            else
                enc_ldrb_r(rt, m->rn, m->rm, m->ext_genus, S);
        }
        return;
    }
    /* immediate form */
    int imm = (int)m->imm;
    if (magn == 8) {
        if (est_store)
            emit_str64(rt, m->rn, imm);
        else
            emit_ldr64(rt, m->rn, imm);
    } else if (magn == 4) {
        if (est_store)
            emit_str32(rt, m->rn, imm);
        else if (signed_ld)
            emit_ldrsw(rt, m->rn, imm);
        else
            emit_ldr32(rt, m->rn, imm);
    } else if (magn == 2) {
        if (est_store)
            emit_strh(rt, m->rn, imm);
        else if (signed_ld) {
            if (is_w)
                enc_ldrsh_w(rt, m->rn, imm);
            else
                emit_ldrsh(rt, m->rn, imm);
        } else
            emit_ldrh(rt, m->rn, imm);
    } else {
        if (est_store)
            emit_strb(rt, m->rn, imm);
        else if (signed_ld) {
            if (is_w)
                enc_ldrsb_w(rt, m->rn, imm);
            else
                emit_ldrsb(rt, m->rn, imm);
        } else
            emit_ldrb(rt, m->rn, imm);
    }
}

static void ins_ldr_str(int est_store, int magn, int signed_ld)
{
    int w;
    int rt = parse_reg(&w);
    exige(',');
    mem_t m;
    parse_mem(&m);
    if (magn == 0)
        magn = w ? 4 : 8;
    enc_ldr_str_mem(est_store, rt, w, &m, magn, signed_ld);
}

/* STUR/LDUR — signed 9-bit imm offset */
static void ins_ur(int est_store, int magn, int signed_ld)
{
    int w;
    int rt = parse_reg(&w);
    exige(',');
    mem_t m;
    parse_mem(&m);
    if (magn == 0)
        magn = w ? 4 : 8;
    if (m.habet_rm || m.pre_index || m.post_index || m.habet_rel_sym)
        erratum_ad(linea_num, "stur/ldur: forma invalida");
    int imm = (int)m.imm;
    if (imm < -256 || imm > 255)
        erratum_ad(linea_num, "stur/ldur: immediatum extra rangum (simm9)");
    if (magn == 8) {
        if (est_store)
            enc_stur64(rt, m.rn, imm);
        else
            enc_ldur64(rt, m.rn, imm);
    } else if (magn == 4) {
        if (est_store)
            enc_stur32(rt, m.rn, imm);
        else if (signed_ld)
            enc_ldursw(rt, m.rn, imm);
        else
            enc_ldur32(rt, m.rn, imm);
    } else if (magn == 2) {
        if (est_store)
            enc_sturh(rt, m.rn, imm);
        else if (signed_ld)
            enc_ldursh_w(rt, m.rn, imm);
        else
            enc_ldurh(rt, m.rn, imm);
    } else {
        if (est_store)
            enc_sturb(rt, m.rn, imm);
        else if (signed_ld)
            enc_ldursb_w(rt, m.rn, imm);
        else
            enc_ldurb(rt, m.rn, imm);
    }
    (void)w;
}

static void ins_ldp_stp(int est_load)
{
    int w1, w2;
    int t1 = parse_reg(&w1);
    exige(',');
    int t2 = parse_reg(&w2);
    exige(',');
    mem_t m;
    parse_mem(&m);
    if (w1 || w2)
        erratum_ad(linea_num, "ldp/stp W nondum supp");
    int imm = (int)m.imm;
    if (m.post_index) {
        if (est_load)
            enc_ldp64_post(t1, t2, m.rn, imm);
        else
            enc_stp64_post(t1, t2, m.rn, imm);
    } else if (m.pre_index) {
        if (est_load)
            enc_ldp64_pre(t1, t2, m.rn, imm);
        else
            enc_stp64_pre(t1, t2, m.rn, imm);
    } else {
        if (est_load)
            enc_ldp64(t1, t2, m.rn, imm);
        else
            enc_stp64(t1, t2, m.rn, imm);
    }
}

static void ins_madd_msub(int est_sub)
{
    int w1, w2, w3, w4;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    int rm = parse_reg(&w3);
    exige(',');
    int ra = parse_reg(&w4);
    if (w1) {
        if (est_sub)
            enc_msub32(rd, rn, rm, ra);
        else
            enc_madd32(rd, rn, rm, ra);
    } else {
        if (est_sub)
            enc_msub64(rd, rn, rm, ra);
        else
            enc_madd64(rd, rn, rm, ra);
    }
}

/* ================================================================
 * FP/SIMD registra — Qn (128), Dn (64), Sn (32), Hn (16), Bn (8)
 * ================================================================ */

/* probat prōximum registrum esse FP reg (q/d/s/h/b + digit) */
static int est_prox_fpreg(void)
{
    saltus_spatii();
    if (!adest())
        return 0;
    char c  = fons[pos];
    char c1 = (pos+1 < fons_lon) ? fons[pos+1] : 0;
    if (
        c != 'q' && c != 'Q' && c != 'd' && c != 'D'
        && c != 's' && c != 'S' && c != 'h' && c != 'H'
        && c != 'b' && c != 'B'
    )
        return 0;
    return (c1 >= '0' && c1 <= '9');
}

/* reddit indicem 0-31; *magn = 1,2,4,8,16 pro B,H,S,D,Q */
static int parse_fpreg(int *magn)
{
    saltus_spatii();
    char c = fons[pos];
    if (c == 'q' || c == 'Q')
        *magn = 16;
    else if (c == 'd' || c == 'D')
        *magn = 8;
    else if (c == 's' || c == 'S')
        *magn = 4;
    else if (c == 'h' || c == 'H')
        *magn = 2;
    else if (c == 'b' || c == 'B')
        *magn = 1;
    else
        erratum_ad(linea_num, "fp registrum invalidum");
    pos++;
    int n = 0;
    if (!adest() || fons[pos] < '0' || fons[pos] > '9')
        erratum_ad(linea_num, "fp registrum sine numero");
    while (adest() && fons[pos] >= '0' && fons[pos] <= '9') {
        n = n * 10 + (fons[pos] - '0');
        pos++;
    }
    if (n > 31)
        erratum_ad(linea_num, "fp registrum > 31");
    return n;
}

/* LDR/STR Qt — 128-bit unsigned imm offset (imm multiplum 16) */
static void enc_ldr_q(int rt, int rn, int imm)
{
    uint32_t uimm = ((uint32_t)imm / 16) & 0xFFF;
    emit32(0x3DC00000 | (uimm << 10) | (rn << 5) | (rt & 0x1F));
}
static void enc_str_q(int rt, int rn, int imm)
{
    uint32_t uimm = ((uint32_t)imm / 16) & 0xFFF;
    emit32(0x3D800000 | (uimm << 10) | (rn << 5) | (rt & 0x1F));
}
/* LDUR/STUR St/Dt — signed 9-bit */
static void enc_ldur_s(int rt, int rn, int imm)
{
    emit32(0xBC400000 | ((imm & 0x1FF) << 12) | (rn << 5) | (rt & 0x1F));
}
static void enc_ldur_d(int rt, int rn, int imm)
{
    emit32(0xFC400000 | ((imm & 0x1FF) << 12) | (rn << 5) | (rt & 0x1F));
}
static void enc_stur_s(int rt, int rn, int imm)
{
    emit32(0xBC000000 | ((imm & 0x1FF) << 12) | (rn << 5) | (rt & 0x1F));
}
static void enc_stur_d(int rt, int rn, int imm)
{
    emit32(0xFC000000 | ((imm & 0x1FF) << 12) | (rn << 5) | (rt & 0x1F));
}
/* LDUR/STUR Qt — 128-bit simm9 */
static void enc_ldur_q(int rt, int rn, int imm)
{
    emit32(0x3CC00000 | ((imm & 0x1FF) << 12) | (rn << 5) | (rt & 0x1F));
}
static void enc_stur_q(int rt, int rn, int imm)
{
    emit32(0x3C800000 | ((imm & 0x1FF) << 12) | (rn << 5) | (rt & 0x1F));
}

/* ldr/str FP forma: distribuit inter fluat emit_fldr/str et enc_ldr/str_q */
static void ins_fp_ldr_str(int est_store, int est_ur)
{
    int magn;
    int rt = parse_fpreg(&magn);
    exige(',');
    mem_t m;
    parse_mem(&m);
    if (m.habet_rm || m.pre_index || m.post_index || m.habet_rel_sym)
        erratum_ad(linea_num, "fp ldr/str: forma non supp");
    int imm = (int)m.imm;
    if (est_ur) {
        if (imm < -256 || imm > 255)
            erratum_ad(linea_num, "fp ldur/stur: simm9 out of range");
        if (magn == 4)
            est_store ? enc_stur_s(rt, m.rn, imm) : enc_ldur_s(rt, m.rn, imm);
        else if (magn == 8)
            est_store ? enc_stur_d(rt, m.rn, imm) : enc_ldur_d(rt, m.rn, imm);
        else if (magn == 16)
            est_store ? enc_stur_q(rt, m.rn, imm) : enc_ldur_q(rt, m.rn, imm);
        else
            erratum_ad(linea_num, "fp ldur/stur: magnitudo %d non supp", magn);
        return;
    }
    if (magn == 4)
        est_store ? emit_fstr32(rt, m.rn, imm) : emit_fldr32(rt, m.rn, imm);
    else if (magn == 8)
        est_store ? emit_fstr64(rt, m.rn, imm) : emit_fldr64(rt, m.rn, imm);
    else if (magn == 16)
        est_store ? enc_str_q(rt, m.rn, imm) : enc_ldr_q(rt, m.rn, imm);
    else
        erratum_ad(linea_num, "fp ldr/str: magnitudo %d non supp", magn);
}

/* movi dN, #0 — ūtitur FMOV Dd, XZR per fluat */
static void ins_movi(void)
{
    int magn;
    int rd = parse_fpreg(&magn);
    exige(',');
    long imm = lege_imm();
    if (imm != 0 || magn != 8)
        erratum_ad(linea_num, "movi: solum 'movi dN, #0' supportatur");
    /* FMOV Dd, XZR = 0x9E6703E0 | Rd */
    emit32(0x9E6703E0 | (rd & 0x1F));
}

/* ================================================================
 * campus bitōrum — BFI, UBFX, SBFX
 * forma: op Wd, Wn, #lsb, #width
 * encoding (W-reg, sf=0):
 *   BFI  Wd,Wn,#lsb,#w = BFM   Wd,Wn,#immr=(32-lsb)&31,#imms=w-1 (0x33000000)
 *   UBFX Wd,Wn,#lsb,#w = UBFM  Wd,Wn,#immr=lsb,#imms=lsb+w-1     (0x53000000)
 *   SBFX Wd,Wn,#lsb,#w = SBFM  Wd,Wn,#immr=lsb,#imms=lsb+w-1     (0x13000000)
 * ================================================================ */
static void ins_bfm_variant(uint32_t base, int est_bfi)
{
    int w1, w2;
    int rd = parse_reg(&w1);
    exige(',');
    int rn = parse_reg(&w2);
    exige(',');
    long lsb = lege_imm();
    exige(',');
    long width = lege_imm();
    if (w1 != w2)
        erratum_ad(linea_num, "bfm: mixta magnitudo");
    if (width < 1 || lsb < 0)
        erratum_ad(linea_num, "bfm: campus invalidus");
    int immr, imms;
    int regmax = w1 ? 32 : 64;
    if (est_bfi) {
        if (lsb >= regmax || width > regmax - lsb)
            erratum_ad(linea_num, "bfi: campus extra rangum");
        immr = (regmax - (int)lsb) & (regmax - 1);
        imms = (int)width - 1;
    } else {
        if (lsb + width > regmax)
            erratum_ad(linea_num, "bfx: campus extra rangum");
        immr = (int)lsb;
        imms = (int)lsb + (int)width - 1;
    }
    /* sf in bit 31; pro X-reg N=1 in bit 22, pro W-reg N=0 */
    uint32_t sf = w1 ? 0u : 1u;
    uint32_t op = base | (sf << 31) | (sf << 22)
        | ((immr & 0x3F) << 16) | ((imms & 0x3F) << 10) | (rn << 5) | rd;
    emit32(op);
}

/* ================================================================
 * instructiones floating-point (Annex F IEC 60559)
 * ================================================================ */

/* FMOV: variantes
 *   fmov dd, xn    — bit-pattern ex x ad d   (0x9E670000)
 *   fmov xd, dn    — bit-pattern ex d ad x   (0x9E660000)
 *   fmov dd, dn    — copia d-reg             (0x1E604000)
 *   fmov dd, xzr   — zerō (bit-pattern)      eadem ac variante dd,xn rn=31
 */
static void ins_fmov(void)
{
    saltus_spatii();
    int m1  = 0, m2 = 0, w;
    char c0 = spectus();
    if (c0 == 'd' || c0 == 'D' || c0 == 's' || c0 == 'S') {
        int rd = parse_fpreg(&m1);
        exige(',');
        saltus_spatii();
        char c1 = spectus();
        if (c1 == 'd' || c1 == 'D' || c1 == 's' || c1 == 'S') {
            int rn = parse_fpreg(&m2);
            if (m1 != m2)
                erratum_ad(linea_num, "fmov: mixta magnitudo fp");
            if (m1 == 8)
                emit32(0x1E604000 | (rn << 5) | rd);
            else if (m1 == 4)
                emit32(0x1E204000 | (rn << 5) | rd);
            else
                erratum_ad(linea_num, "fmov d/s solum supportatur");
            return;
        }
        /* fmov dd, xn (vel xzr)  aut  fmov sd, wn */
        int rn = parse_reg(&w);
        if (m1 == 8) {
            if (w)
                erratum_ad(linea_num, "fmov dd, Xn: x-reg requiritur");
            emit32(0x9E670000 | (rn << 5) | rd);
        } else if (m1 == 4) {
            if (!w)
                erratum_ad(linea_num, "fmov sd, Wn: w-reg requiritur");
            emit32(0x1E270000 | (rn << 5) | rd);
        } else
            erratum_ad(linea_num, "fmov d/s solum supportatur");
        return;
    }
    /* fmov xd, dn  aut  fmov wd, sn */
    int rd = parse_reg(&w);
    exige(',');
    int rn = parse_fpreg(&m2);
    if (m2 == 8) {
        if (w)
            erratum_ad(linea_num, "fmov Xd, Dn: x-reg requiritur");
        emit32(0x9E660000 | (rn << 5) | rd);
    } else if (m2 == 4) {
        if (!w)
            erratum_ad(linea_num, "fmov Wd, Sn: w-reg requiritur");
        emit32(0x1E260000 | (rn << 5) | rd);
    } else
        erratum_ad(linea_num, "fmov d/s solum supportatur");
}

/* fadd/fsub/fmul/fdiv dd, dn, dm */
static void ins_fp_arith(uint32_t op_d, uint32_t op_s)
{
    int m1, m2, m3;
    int rd = parse_fpreg(&m1);
    exige(',');
    int rn = parse_fpreg(&m2);
    exige(',');
    int rm = parse_fpreg(&m3);
    if (m1 != m2 || m1 != m3)
        erratum_ad(linea_num, "fp arith: mixta magnitudo");
    if (m1 == 8)
        emit32(op_d | (rm << 16) | (rn << 5) | rd);
    else if (m1 == 4)
        emit32(op_s | (rm << 16) | (rn << 5) | rd);
    else
        erratum_ad(linea_num, "fp arith: d/s solum supportatur");
}

/* fneg dd, dn */
static void ins_fneg(void)
{
    int m1, m2;
    int rd = parse_fpreg(&m1);
    exige(',');
    int rn = parse_fpreg(&m2);
    if (m1 != m2)
        erratum_ad(linea_num, "fneg: mixta magnitudo");
    if (m1 == 8)
        emit32(0x1E614000 | (rn << 5) | rd);
    else if (m1 == 4)
        emit32(0x1E214000 | (rn << 5) | rd);
    else
        erratum_ad(linea_num, "fneg d/s solum supportatur");
}

/* fcmp dn, dm */
static void ins_fcmp(void)
{
    int m1, m2;
    int rn = parse_fpreg(&m1);
    exige(',');
    int rm = parse_fpreg(&m2);
    if (m1 != m2)
        erratum_ad(linea_num, "fcmp: mixta magnitudo");
    if (m1 == 8)
        emit32(0x1E602000 | (rm << 16) | (rn << 5));
    else if (m1 == 4)
        emit32(0x1E202000 | (rm << 16) | (rn << 5));
    else
        erratum_ad(linea_num, "fcmp d/s solum supportatur");
}

/* fcvt: conversiones inter magnitudines fp */
static void ins_fcvt(void)
{
    int m1, m2;
    int rd = parse_fpreg(&m1);
    exige(',');
    int rn = parse_fpreg(&m2);
    if (m1 == 8 && m2 == 4) /* single → double */
        emit32(0x1E22C000 | (rn << 5) | rd);
    else if (m1 == 4 && m2 == 8) /* double → single */
        emit32(0x1E624000 | (rn << 5) | rd);
    else
        erratum_ad(linea_num, "fcvt: solum d↔s supportatur");
}

/* scvtf/ucvtf dd, xn/wn — conversiō integer → fp */
static void ins_cvt_to_fp(int est_unsigned)
{
    int m1, w;
    int rd = parse_fpreg(&m1);
    exige(',');
    int rn = parse_reg(&w);
    uint32_t base;
    if (m1 == 8) {
        /* sf=w?0:1 in bit 31 */
        base = est_unsigned ? 0x1E630000 : 0x1E620000;
        if (!w)
            base |= 0x80000000;
    } else if (m1 == 4) {
        base = est_unsigned ? 0x1E230000 : 0x1E220000;
        if (!w)
            base |= 0x80000000;
    } else {
        erratum_ad(linea_num, "scvtf/ucvtf: solum d/s supportatur");
        return;
    }
    emit32(base | (rn << 5) | rd);
}

/* fcvtzs xd, dn / wd, dn — conversio fp → integer (truncatio) */
static void ins_fcvtzs(void)
{
    int m1, w;
    int rd = parse_reg(&w);
    exige(',');
    int rn = parse_fpreg(&m1);
    uint32_t base;
    if (m1 == 8)
        base = 0x1E780000;
    else if (m1 == 4)
        base = 0x1E380000;
    else {
        erratum_ad(linea_num, "fcvtzs: d/s solum supportatur");
        return;
    }
    if (!w)
        base |= 0x80000000;
    emit32(base | (rn << 5) | rd);
}

/* ================================================================
 * dispatch
 * ================================================================ */

static void processa_instructionem(const char *op)
{
    /* condicio-branch */
    if (op[0] == 'b' && op[1] == '.') {
        int c = parse_cond(op + 2);
        if (c < 0)
            erratum_ad(linea_num, "b.%s condicio ignota", op+2);
        ins_bcond(c);
        return;
    }
    if (!strcmp(op, "mov")) {
        ins_mov();
        return;
    }
    if (!strcmp(op, "movz")) {
        ins_mov_variant("movz");
        return;
    }
    if (!strcmp(op, "movk")) {
        ins_mov_variant("movk");
        return;
    }
    if (!strcmp(op, "movn")) {
        ins_mov_variant("movn");
        return;
    }
    if (!strcmp(op, "add")) {
        ins_add_sub_generic(0, 0);
        return;
    }
    if (!strcmp(op, "sub")) {
        ins_add_sub_generic(1, 0);
        return;
    }
    if (!strcmp(op, "adds")) {
        ins_add_sub_generic(0, 1);
        return;
    }
    if (!strcmp(op, "subs")) {
        ins_add_sub_generic(1, 1);
        return;
    }
    if (!strcmp(op, "mul")) {
        ins_mul(0);
        return;
    }
    if (!strcmp(op, "sdiv")) {
        ins_div(1);
        return;
    }
    if (!strcmp(op, "udiv")) {
        ins_div(0);
        return;
    }
    if (!strcmp(op, "and")) {
        ins_logic("and");
        return;
    }
    if (!strcmp(op, "orr")) {
        ins_logic("orr");
        return;
    }
    if (!strcmp(op, "eor")) {
        ins_logic("eor");
        return;
    }
    if (!strcmp(op, "lsl") || !strcmp(op, "lslv")) {
        ins_logic("lslv");
        return;
    }
    if (!strcmp(op, "lsr") || !strcmp(op, "lsrv")) {
        ins_logic("lsrv");
        return;
    }
    if (!strcmp(op, "asr") || !strcmp(op, "asrv")) {
        ins_logic("asrv");
        return;
    }
    if (!strcmp(op, "neg")) {
        ins_neg(0);
        return;
    }
    if (!strcmp(op, "negs")) {
        ins_neg(1);
        return;
    }
    if (!strcmp(op, "mvn")) {
        ins_mvn();
        return;
    }
    if (!strcmp(op, "cmp")) {
        ins_cmp(0);
        return;
    }
    if (!strcmp(op, "cmn")) {
        ins_cmp(1);
        return;
    }
    if (!strcmp(op, "tst")) {
        ins_tst();
        return;
    }
    if (!strcmp(op, "cset")) {
        ins_cset();
        return;
    }
    if (!strcmp(op, "csel")) {
        ins_csel();
        return;
    }
    if (!strcmp(op, "csinc")) {
        ins_csinc();
        return;
    }
    if (
        !strcmp(op, "sxtw") || !strcmp(op, "uxtw")
        || !strcmp(op, "sxtb") || !strcmp(op, "sxth")
        || !strcmp(op, "uxtb") || !strcmp(op, "uxth")
    ) {
        ins_ext(op);
        return;
    }
    if (!strcmp(op, "b")) {
        ins_b(0);
        return;
    }
    if (!strcmp(op, "bl")) {
        ins_b(1);
        return;
    }
    if (!strcmp(op, "blr")) {
        ins_br_blr(1);
        return;
    }
    if (!strcmp(op, "br")) {
        ins_br_blr(0);
        return;
    }
    if (!strcmp(op, "ret")) {
        ins_ret();
        return;
    }
    if (!strcmp(op, "cbz")) {
        ins_cbz_cbnz(0);
        return;
    }
    if (!strcmp(op, "cbnz")) {
        ins_cbz_cbnz(1);
        return;
    }
    if (!strcmp(op, "tbz")) {
        ins_tbz_tbnz(0);
        return;
    }
    if (!strcmp(op, "tbnz")) {
        ins_tbz_tbnz(1);
        return;
    }
    if (!strcmp(op, "adrp")) {
        ins_adrp();
        return;
    }
    if (!strcmp(op, "adr")) {
        ins_adr();
        return;
    }
    if (!strcmp(op, "ldr")) {
        if (est_prox_fpreg()) {
            ins_fp_ldr_str(0, 0);
            return;
        }
        ins_ldr_str(0, 0, 0);
        return;
    }
    if (!strcmp(op, "str")) {
        if (est_prox_fpreg()) {
            ins_fp_ldr_str(1, 0);
            return;
        }
        ins_ldr_str(1, 0, 0);
        return;
    }
    if (!strcmp(op, "ldrb")) {
        ins_ldr_str(0, 1, 0);
        return;
    }
    if (!strcmp(op, "strb")) {
        ins_ldr_str(1, 1, 0);
        return;
    }
    if (!strcmp(op, "ldrh")) {
        ins_ldr_str(0, 2, 0);
        return;
    }
    if (!strcmp(op, "strh")) {
        ins_ldr_str(1, 2, 0);
        return;
    }
    if (!strcmp(op, "ldrsb")) {
        ins_ldr_str(0, 1, 1);
        return;
    }
    if (!strcmp(op, "ldrsh")) {
        ins_ldr_str(0, 2, 1);
        return;
    }
    if (!strcmp(op, "ldrsw")) {
        ins_ldr_str(0, 4, 1);
        return;
    }
    if (!strcmp(op, "ldur")) {
        if (est_prox_fpreg()) {
            ins_fp_ldr_str(0, 1);
            return;
        }
        ins_ur(0, 0, 0);
        return;
    }
    if (!strcmp(op, "stur")) {
        if (est_prox_fpreg()) {
            ins_fp_ldr_str(1, 1);
            return;
        }
        ins_ur(1, 0, 0);
        return;
    }
    if (!strcmp(op, "movi")) {
        ins_movi();
        return;
    }
    if (!strcmp(op, "ldurb")) {
        ins_ur(0, 1, 0);
        return;
    }
    if (!strcmp(op, "sturb")) {
        ins_ur(1, 1, 0);
        return;
    }
    if (!strcmp(op, "ldurh")) {
        ins_ur(0, 2, 0);
        return;
    }
    if (!strcmp(op, "sturh")) {
        ins_ur(1, 2, 0);
        return;
    }
    if (!strcmp(op, "ldursb")) {
        ins_ur(0, 1, 1);
        return;
    }
    if (!strcmp(op, "ldursh")) {
        ins_ur(0, 2, 1);
        return;
    }
    if (!strcmp(op, "ldursw")) {
        ins_ur(0, 4, 1);
        return;
    }
    if (!strcmp(op, "ldp")) {
        ins_ldp_stp(1);
        return;
    }
    if (!strcmp(op, "stp")) {
        ins_ldp_stp(0);
        return;
    }
    if (!strcmp(op, "madd")) {
        ins_madd_msub(0);
        return;
    }
    if (!strcmp(op, "msub")) {
        ins_madd_msub(1);
        return;
    }
    if (!strcmp(op, "nop")) {
        emit32(0xD503201F);
        return;
    }
    /* floating-point */
    if (!strcmp(op, "fmov")) {
        ins_fmov();
        return;
    }
    if (!strcmp(op, "fadd")) {
        ins_fp_arith(0x1E602800, 0x1E202800);
        return;
    }
    if (!strcmp(op, "fsub")) {
        ins_fp_arith(0x1E603800, 0x1E203800);
        return;
    }
    if (!strcmp(op, "fmul")) {
        ins_fp_arith(0x1E600800, 0x1E200800);
        return;
    }
    if (!strcmp(op, "fdiv")) {
        ins_fp_arith(0x1E601800, 0x1E201800);
        return;
    }
    if (!strcmp(op, "fneg")) {
        ins_fneg();
        return;
    }
    if (!strcmp(op, "fcmp")) {
        ins_fcmp();
        return;
    }
    if (!strcmp(op, "fcvt")) {
        ins_fcvt();
        return;
    }
    if (!strcmp(op, "scvtf")) {
        ins_cvt_to_fp(0);
        return;
    }
    if (!strcmp(op, "ucvtf")) {
        ins_cvt_to_fp(1);
        return;
    }
    if (!strcmp(op, "fcvtzs")) {
        ins_fcvtzs();
        return;
    }
    /* campus bitōrum */
    if (!strcmp(op, "bfi")) {
        ins_bfm_variant(0x33000000, 1);
        return;
    }
    if (!strcmp(op, "ubfx")) {
        ins_bfm_variant(0x53000000, 0);
        return;
    }
    if (!strcmp(op, "sbfx")) {
        ins_bfm_variant(0x13000000, 0);
        return;
    }
    erratum_ad(linea_num, "instructio ignota: '%s'", op);
}

/* ================================================================
 * processa lineam (pass 2)
 * ================================================================ */

static void processa_lineam(void)
{
    saltus_spatii();
    if (!adest() || spectus() == '\n')
        return;
    if (spectus() == ';' || (spectus() == '/' && pos+1 < fons_lon && fons[pos+1] == '/')) {
        saltus_ad_eol();
        return;
    }
    if (spectus() == '#') {
        /* # lineno "file" ... */
        saltus_ad_eol();
        return;
    }
    char nom[256];
    int save = pos;
    int n    = lege_nom(nom, 256);
    if (n == 0) {
        saltus_ad_eol();
        return;
    }
    saltus_spatii();
    if (adest() && fons[pos] == ':') {
        pos++;
        pone_labelum(nom);
        return;
    }
    /* directiva? */
    if (nom[0] == '.') {
        if (directiva_ignoranda(nom)) {
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".text")) {
            sectio_currens = SEC_TEXT;
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".data")) {
            sectio_currens = SEC_DATA;
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".const")) {
            sectio_currens = SEC_CONST;
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".cstring")) {
            sectio_currens = SEC_CSTRING;
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".bss")) {
            sectio_currens = SEC_BSS;
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".section")) {
            char seg[64], sct[64];
            seg[0] = sct[0] = 0;
            lege_nom(seg, 64);
            if (matcha(','))
                lege_nom(sct, 64);
            /* attributes ignorantur */
            saltus_ad_eol();
            if (!strcmp(sct, "__text"))
                sectio_currens = SEC_TEXT;
            else if (!strcmp(sct, "__cstring"))
                sectio_currens = SEC_CSTRING;
            else if (!strcmp(sct, "__const"))
                sectio_currens = SEC_CONST;
            else if (!strcmp(sct, "__data"))
                sectio_currens = SEC_DATA;
            else if (!strcmp(sct, "__bss"))
                sectio_currens = SEC_BSS;
            else if (
                !strcmp(sct, "__literal4") || !strcmp(sct, "__literal8")
                || !strcmp(sct, "__literal16")
            )
                sectio_currens = SEC_CONST;
            else
                erratum_ad(linea_num, "sectio ignota: %s,%s", seg, sct);
            return;
        }
        if (!strcmp(nom, ".globl") || !strcmp(nom, ".global")) {
            char sn[256];
            lege_nom(sn, 256);
            int si      = sym_quaere_vel_crea(sn);
            symbola[si] .est_globalis = 1;
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".p2align")) {
            dir_p2align();
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".align")) {
            long a = lege_num();
            /* in darwin, .align est potentia 2 directe */
            int col = 1 << a;
            colineatio_currens = col;
            if (sectio_currens != SEC_TEXT && sectio_currens != SEC_CSTRING)
                alinea_data(col);
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".long")) {
            dir_long();
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".word")) {
            dir_long();
            saltus_ad_eol();
            return;
        }
        if (
            !strcmp(nom, ".short") || !strcmp(nom, ".half")
            || !strcmp(nom, ".2byte")
        ) {
            dir_short();
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".quad") || !strcmp(nom, ".8byte")) {
            dir_quad();
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".byte")) {
            dir_byte();
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".asciz") || !strcmp(nom, ".string")) {
            dir_asciz(1);
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".ascii")) {
            dir_asciz(0);
            saltus_ad_eol();
            return;
        }
        if (
            !strcmp(nom, ".zero") || !strcmp(nom, ".space")
            || !strcmp(nom, ".skip")
        ) {
            dir_zero_or_space();
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".comm") || !strcmp(nom, ".lcomm")) {
            /* iam in pass1 tractatum */
            saltus_ad_eol();
            return;
        }
        if (0) {
            char sn[256];
            lege_nom(sn, 256);
            exige(',');
            long sz  = lege_num();
            long col = 1;
            if (matcha(','))
                col = lege_num();
            int si = sym_quaere_vel_crea(sn);
            if (symbola[si].genus == SYM_IGNOTUS) {
                if (num_globalium >= MAX_GLOBALES)
                    erratum("nimis multae globales");
                int gi         = num_globalium++;
                const char *nn = (sn[0] == '_') ? sn + 1 : sn;
                strncpy(globales[gi].nomen, nn, 255);
                globales[gi] .typus = NULL;
                globales[gi] .magnitudo = (int)sz;
                globales[gi] .colineatio = (int)(1 << col);
                globales[gi] .est_bss = 1;
                globales[gi] .est_staticus = (!strcmp(nom, ".lcomm"));
                globales[gi] .valor_initialis = 0;
                globales[gi] .habet_valorem = 0;
                globales[gi] .data_offset = 0;
                symbola[si]  .genus = SYM_GLOB;
                symbola[si]  .id = gi;
                symbola[si]  .definitum = 1;
            }
            saltus_ad_eol();
            return;
        }
        if (!strcmp(nom, ".zerofill")) {
            /* iam in pass1 tractatum */
            saltus_ad_eol();
            return;
        }
        if (0) {
            /* .zerofill SEG,SECT,_sym,size,align */
            char seg[32], sct[32], sn[256];
            lege_nom(seg, 32);
            exige(',');
            lege_nom(sct, 32);
            exige(',');
            lege_nom(sn, 256);
            exige(',');
            long sz  = lege_num();
            long col = 0;
            if (matcha(','))
                col = lege_num();
            int si = sym_quaere_vel_crea(sn);
            if (num_globalium >= MAX_GLOBALES)
                erratum("nimis globales");
            int gi         = num_globalium++;
            const char *nn = (sn[0] == '_') ? sn + 1 : sn;
            strncpy(globales[gi].nomen, nn, 255);
            globales[gi] .magnitudo = (int)sz;
            globales[gi] .colineatio = (int)(1 << col);
            globales[gi] .est_bss = 1;
            globales[gi] .est_staticus = !symbola[si].est_globalis;
            globales[gi] .data_offset = 0;
            symbola[si]  .genus = SYM_GLOB;
            symbola[si]  .id = gi;
            symbola[si]  .definitum = 1;
            (void)seg;
            (void)sct;
            saltus_ad_eol();
            return;
        }
        /* directiva ignota — sed non fallere: haec sunt multae directivae
         * debugs (.cfi_*). Si non in lista ignoratus → erratum.
         * saltus_ad_eol iam non sufficit — exigimus strict. */
        erratum_ad(linea_num, "directiva ignota: '%s'", nom);
        return;
    }
    /* instructio */
    (void)save;
    processa_instructionem(nom);
    saltus_spatii();
    if (!est_eol()) {
        char trail[64];
        int tn = 0;
        while (adest() && fons[pos] != '\n' && tn + 1 < (int)sizeof(trail))
            trail[tn++] = fons[pos++];
        trail[tn] = 0;
        erratum_ad(
            linea_num, "instructio '%s': residuum non consumptum: '%s'",
            nom, trail
        );
    }
    saltus_ad_eol();
}

/* ================================================================
 * main
 * ================================================================ */

static void usus(void)
{
    fprintf(stderr, "usus: imm <plica.s> [-o <plica.o>] [-P <praefixum>]\n");
    exit(1);
}

int main(int argc, char *argv[])
{
    nomen_programmi = "imm";
    const char      *plica_in = NULL;
    const char      *plica_out = NULL;
    const char      *praefixum = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o")) {
            if (++i >= argc)
                usus();
            plica_out = argv[i];
        } else if (!strcmp(argv[i], "-P")) {
            if (++i >= argc)
                usus();
            praefixum = argv[i];
        } else if (!strncmp(argv[i], "-P", 2)) {
            praefixum = argv[i] + 2;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usus();
        } else if (argv[i][0] == '-') {
            erratum("vexillum ignotum: %s", argv[i]);
        } else {
            if (plica_in)
                erratum("nimis multae plicae fontis");
            plica_in = argv[i];
        }
    }
    scribo_praefixum_pone(praefixum);
    if (!plica_in)
        usus();
    int lin = (int)strlen(plica_in);
    if (lin < 3 || plica_in[lin-2] != '.' || plica_in[lin-1] != 's')
        erratum("plica fontis debet finire in .s");
    char out_buf[1024];
    if (!plica_out) {
        if (lin + 1 >= (int)sizeof(out_buf))
            erratum("nomen plicae nimis longum");
        memcpy(out_buf, plica_in, lin);
        out_buf[lin  -1] = 'o';
        out_buf[lin]     = 0;
        plica_out        = out_buf;
    }
    plica_currentis = plica_in;

    emitte_initia();

    int flon;
    char *buf = lege_plicam(plica_in, &flon);
    if (!buf)
        erratum("non possum legere '%s'", plica_in);
    fons      = buf;
    fons_lon  = flon;
    pos       = 0;
    linea_num = 1;

    /* pass 1: collige omnes labels */
    pass1();

    /* pass 2: processa */
    pos       = 0;
    linea_num = 1;
    while (adest()) {
        processa_lineam();
        if (adest() && fons[pos] == '\n') {
            pos++;
            linea_num++;
        }
    }

    /* finalize ultimum globalis magnitudinem */
    if (
        ultimum_data_sym >= 0
        && symbola[ultimum_data_sym].genus == SYM_GLOB
    ) {
        int gi = symbola[ultimum_data_sym].id;
        if (gi >= 0 && globales[gi].magnitudo == 0)
            globales[gi].magnitudo = init_data_lon - globales[gi].data_offset;
    }

    scribo_obiectum(plica_out);
    return 0;
}
