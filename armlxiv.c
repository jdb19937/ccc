/*
 * armlxiv.c — imitator machinae AArch64 pro binariis Mach-O Darwinis.
 *
 * Nomenclatura:
 *   machina    — status processoris (capsae, vexilla, PC).
 *   capsa      — cella registri (64 bit pro X0..X30, SP; 128 pro V0..V31).
 *   pagina     — unitas memoriae virtualis (16 KiB in Darwin ARM, sed nos 4 KiB).
 *   iussum     — singulum mandatum (32 bit fixum in AArch64).
 *   resolvere  — mandatum ex quattuor octetis decodicare et classificare.
 *   exsequi    — mandatum iam resolutum implere.
 *   vocatio    — vocatio systematis (SVC #0x80, numerus in X16).
 *   culpa      — error fatalis: imitator cum nuntio desinit.
 *
 * Principium fundamentale: si iussum aut vocatio non cognoscitur, numquam
 * silentio progredimur; semper cum culpa desinimus ne executio invalida fiat.
 *
 * Binaria recepta: Mach-O 64-bit, CPU_TYPE_ARM64, MH_EXECUTE, statica
 * (nullum LC_LOAD_DYLINKER, nullum LC_LOAD_DYLIB). Punctum introitus per
 * LC_MAIN vel LC_UNIXTHREAD. Comitans bibliotheca 'vulcanus' haec
 * confecit.
 *
 * Aedificatio imitatoris (nativa in Darwin arm64):
 *
 *     cc -Wall -Wextra -std=c99 -O2 armlxiv.c -o armlxiv
 *
 * Usus:
 *
 *     ./armlxiv [-v|-vv] PROGRAMMA [argumenta...]
 *
 * Aedificatio peregrini programmatis (PROG.c) cum vulcano, ut armlxiv
 * eum exsequi possit:
 *
 *     clang -arch arm64 -std=c99 -O2                                 \
 *           -nostdinc -ffreestanding -fno-stack-protector             \
 *           -fno-vectorize -fno-slp-vectorize                         \
 *           -Ivulcanus/include                                        \
 *           -nostdlib -Wl,-static -Wl,-e,_start                       \
 *           PROG.c vulcanus/vulcanus.c -o PROG
 *     ./armlxiv PROG
 *
 * Si vulcanus iam ut obiectum (vulcanus.o) compilatus est, 'vulcanus.c'
 * in iussu supra per 'vulcanus/vulcanus.o' substituere possumus.
 */

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <dirent.h>
#include <signal.h>

/* ======================================================================== *
 * 0.  Arithmetica 128 bitorum (pro MUL/DIV longis: SMULH, UMULH).
 * ======================================================================== */

static void mul64_128u(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
    uint64_t al = (uint32_t)a, ah = a >> 32;
    uint64_t bl = (uint32_t)b, bh = b >> 32;
    uint64_t ll = al * bl;
    uint64_t lh = al * bh;
    uint64_t hl = ah * bl;
    uint64_t hh = ah * bh;
    uint64_t mid = (ll >> 32) + (uint32_t)lh + (uint32_t)hl;
    *lo = (ll & 0xffffffffu) | (mid << 32);
    *hi = hh + (lh >> 32) + (hl >> 32) + (mid >> 32);
}

static void mul64_128s(int64_t a, int64_t b, int64_t *hi, uint64_t *lo)
{
    uint64_t uh;
    mul64_128u((uint64_t)a, (uint64_t)b, &uh, lo);
    if (a < 0)
        uh -= (uint64_t)b;
    if (b < 0)
        uh -= (uint64_t)a;
    *hi = (int64_t)uh;
}

/* ======================================================================== *
 * I.  Structurae Mach-O (manu definitae, ne ab capitibus systematis pendeamus).
 * ======================================================================== */

#define MH_MAGIC_64             0xfeedfacfu
#define CPU_TYPE_ARM64          0x0100000cu
#define MH_EXECUTE              0x2u

#define LC_SEGMENT_64           0x19u
#define LC_UNIXTHREAD           0x05u
#define LC_MAIN                 0x80000028u
#define LC_LOAD_DYLIB           0x0cu
#define LC_LOAD_DYLINKER        0x0eu
#define LC_ID_DYLIB             0x0du
#define LC_SYMTAB               0x02u
#define LC_DYSYMTAB             0x0bu
#define LC_UUID                 0x1bu
#define LC_VERSION_MIN_MACOSX   0x24u
#define LC_SOURCE_VERSION       0x2au
#define LC_FUNCTION_STARTS      0x26u
#define LC_DATA_IN_CODE         0x29u
#define LC_CODE_SIGNATURE       0x1du
#define LC_BUILD_VERSION        0x32u
#define LC_DYLD_INFO            0x22u
#define LC_DYLD_INFO_ONLY       0x80000022u
#define LC_DYLD_CHAINED_FIXUPS  0x80000034u
#define LC_DYLD_EXPORTS_TRIE    0x80000033u
#define LC_SEGMENT_SPLIT_INFO   0x1eu
#define LC_ENCRYPTION_INFO_64   0x2cu

#define ARM_THREAD_STATE64 6

typedef struct {
    uint32_t magic;
    int32_t  cputype;
    int32_t  cpusubtype;
    uint32_t filetype;
    uint32_t ncmds;
    uint32_t sizeofcmds;
    uint32_t flags;
    uint32_t reserved;
} MachCaput64;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
} MachIussum;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    int32_t  maxprot;
    int32_t  initprot;
    uint32_t nsects;
    uint32_t flags;
} MachSegmentum64;

typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    uint64_t entryoff;
    uint64_t stacksize;
} MachIntroitus;

/* ======================================================================== *
 * II.  Culpa et diagnostica.
 * ======================================================================== */

static const char *imitator_nomen = "armlxiv";
static int verbositas = 0;

static void
culpa(const char *forma, ...)
{
    va_list ap;
    fprintf(stderr, "%s: culpa: ", imitator_nomen);
    va_start(ap, forma);
    vfprintf(stderr, forma, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(2);
}

static void
nuntio(const char *forma, ...)
{
    if (verbositas < 1)
        return;
    va_list ap;
    fprintf(stderr, "%s: ", imitator_nomen);
    va_start(ap, forma);
    vfprintf(stderr, forma, ap);
    va_end(ap);
    fputc('\n', stderr);
}

/* ======================================================================== *
 * III.  Memoria virtualis.
 *
 * Unum spatium continuum allocatur; accessus fit per compensationem
 * a basi virtuali. Mach-O binaria plerumque vmaddr 0x100000000 vel supra
 * usurpant (post __PAGEZERO), quod nos respectamus.
 * ======================================================================== */

#define PAGINA     0x4000u           /* 16 KiB, ut Darwin ARM usitat. */
#define SPATII_MAG (1ull << 34)
#define PILA_MAG   (8ull << 20)

typedef struct {
    uint8_t *caro;
    uint64_t basis;
    uint64_t terminus;        /* summa congerei (sub pila). */
    uint64_t limen_superum;   /* basis + SPATII_MAG. */
    uint64_t entry;
    uint64_t text_basis;      /* basis segmenti __TEXT (pro LC_MAIN). */
} Memoria;

static int
valet_locus(Memoria *m, uint64_t va, uint64_t mag)
{
    if (mag == 0)
        return 1;
    if (va < m->basis)
        return 0;
    if (va - m->basis + mag > SPATII_MAG)
        return 0;
    if (va + mag < va)
        return 0;
    return 1;
}

static uint8_t *
    hosp(Memoria *m, uint64_t va)
{
    if (va < m->basis || va - m->basis >= SPATII_MAG)
        culpa("accessus memoriae extra spatium: 0x%016llx", (unsigned long long)va);
    return m->caro + (va - m->basis);
}

static void
lege_mem(Memoria *m, uint64_t va, void *dst, size_t n)
{
    if (!valet_locus(m, va, n))
        culpa("legere extra spatium: [0x%016llx, +%zu)", (unsigned long long)va, n);
    memcpy(dst, hosp(m, va), n);
}

static void
scribe_mem(Memoria *m, uint64_t va, const void *src, size_t n)
{
    if (!valet_locus(m, va, n))
        culpa("scribere extra spatium: [0x%016llx, +%zu)", (unsigned long long)va, n);
    memcpy(hosp(m, va), src, n);
}

static uint8_t  lege_u8 (Memoria *m, uint64_t va) {
    uint8_t  v;
    lege_mem(m, va, &v, 1);
    return v;
}
static uint16_t lege_u16(Memoria *m, uint64_t va) {
    uint16_t v;
    lege_mem(m, va, &v, 2);
    return v;
}
static uint32_t lege_u32(Memoria *m, uint64_t va) {
    uint32_t v;
    lege_mem(m, va, &v, 4);
    return v;
}
static uint64_t lege_u64(Memoria *m, uint64_t va) {
    uint64_t v;
    lege_mem(m, va, &v, 8);
    return v;
}

static void scribe_u8 (Memoria *m, uint64_t va, uint8_t  v) { scribe_mem(m, va, &v, 1); }
static void scribe_u16(Memoria *m, uint64_t va, uint16_t v) { scribe_mem(m, va, &v, 2); }
static void scribe_u32(Memoria *m, uint64_t va, uint32_t v) { scribe_mem(m, va, &v, 4); }
static void scribe_u64(Memoria *m, uint64_t va, uint64_t v) { scribe_mem(m, va, &v, 8); }

/* ======================================================================== *
 * IV.  Loader Mach-O.
 *
 * Receptio rigorosa: accipimus MH_EXECUTE statice connexum. Si
 * LC_LOAD_DYLINKER aut LC_LOAD_DYLIB (praeter selbiseipsum) adest, culpa.
 * Segmenta copiamus ad vmaddr + (offsetum PIE, si opus est). Punctum
 * introitus ex LC_MAIN aut LC_UNIXTHREAD trahimus.
 * ======================================================================== */

static void
onera_macho(const char *iter, Memoria *m, int *habet_lc_main)
{
    FILE *fp = fopen(iter, "rb");
    if (!fp)
        culpa("aperire '%s': %s", iter, strerror(errno));
    fseek(fp, 0, SEEK_END);
    long magnitudo = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (magnitudo < (long)sizeof(MachCaput64))
        culpa("'%s' nimis parvum ad Mach-O validum", iter);
    uint8_t *tota = (uint8_t *)malloc((size_t)magnitudo);
    if (!tota)
        culpa("memoriam pro '%s' dare non possum", iter);
    if (fread(tota, 1, (size_t)magnitudo, fp) != (size_t)magnitudo)
        culpa("'%s' legi non potuit", iter);
    fclose(fp);

    MachCaput64 caput;
    memcpy(&caput, tota, sizeof caput);
    if (caput.magic != MH_MAGIC_64)
        culpa("'%s' signaturam Mach-O 64 non habet (0x%08x)", iter, caput.magic);
    if ((uint32_t)caput.cputype != CPU_TYPE_ARM64)
        culpa("cputype non est arm64 (0x%08x)", (uint32_t)caput.cputype);
    if (caput.filetype != MH_EXECUTE)
        culpa("filetype non est MH_EXECUTE (%u)", caput.filetype);

    /* Prima iteratio: invenire minimum/maximum segmentorum (praeter __PAGEZERO). */
    uint64_t va_min     = UINT64_MAX, va_max = 0;
    uint64_t text_basis = 0;
    int text_inventus   = 0;

    uint32_t off = sizeof(MachCaput64);
    for (uint32_t i = 0; i < caput.ncmds; i++) {
        MachIussum lc;
        if (off + sizeof lc > (uint32_t)magnitudo)
            culpa("load command excedit fasciculum");
        memcpy(&lc, tota + off, sizeof lc);
        if (lc.cmdsize < sizeof lc || off + lc.cmdsize > (uint32_t)magnitudo)
            culpa("load command #%u cmdsize invalidus", i);

        switch (lc.cmd) {
        case LC_SEGMENT_64: {
                MachSegmentum64 s;
                if (lc.cmdsize < sizeof s)
                    culpa("LC_SEGMENT_64 cmdsize");
                memcpy(&s, tota + off, sizeof s);
                if (strcmp(s.segname, "__PAGEZERO") == 0)
                    break;
                if (s.vmsize == 0)
                    break;
                if (s.vmaddr < va_min)
                    va_min = s.vmaddr;
                if (s.vmaddr + s.vmsize > va_max)
                    va_max = s.vmaddr + s.vmsize;
                if (strcmp(s.segname, "__TEXT") == 0) {
                    text_basis    = s.vmaddr;
                    text_inventus = 1;
                }
                break;
            }
        case LC_LOAD_DYLINKER:
            culpa("binarium LC_LOAD_DYLINKER habet; tantum staticum sustinetur");
        case LC_LOAD_DYLIB:
            culpa("binarium LC_LOAD_DYLIB habet; tantum staticum sustinetur");
        case LC_DYLD_INFO:
        case LC_DYLD_INFO_ONLY:
        case LC_DYLD_CHAINED_FIXUPS:
            culpa("binarium fixuras dyld habet; tantum staticum sustinetur");
        }
        off += lc.cmdsize;
    }
    if (va_min == UINT64_MAX)
        culpa("nullum segmentum exsequendum inventum");
    if (!text_inventus)
        culpa("__TEXT segmentum non inventum");

    uint64_t basis = va_min & ~(uint64_t)(PAGINA - 1);

    void *regio = mmap(
        (void *)basis, SPATII_MAG,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON | MAP_FIXED,
        -1, 0
    );
    if (regio == MAP_FAILED || (uint64_t)regio != basis) {
        if (regio != MAP_FAILED)
            munmap(regio, SPATII_MAG);
        regio = mmap(
            NULL, SPATII_MAG, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANON, -1, 0
        );
        if (regio == MAP_FAILED)
            culpa("spatium virtuale allocari non potuit: %s", strerror(errno));
        nuntio(
            "basis hospitis 0x%llx non adepta; utimur %p (VA peregrini manet)",
            (unsigned long long)basis, regio
        );
    }
    m->caro          = (uint8_t *)regio;
    m->basis         = basis;
    m->limen_superum = basis + SPATII_MAG;
    m->terminus      = (va_max + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
    m->text_basis    = text_basis;

    /* Secunda iteratio: copia segmenta, trahe entry. */
    m->entry = 0;
    *habet_lc_main = 0;
    off = sizeof(MachCaput64);
    for (uint32_t i = 0; i < caput.ncmds; i++) {
        MachIussum lc;
        memcpy(&lc, tota + off, sizeof lc);
        switch (lc.cmd) {
        case LC_SEGMENT_64: {
                MachSegmentum64 s;
                memcpy(&s, tota + off, sizeof s);
                if (strcmp(s.segname, "__PAGEZERO") == 0)
                    break;
                if (s.filesize > 0) {
                    if (s.fileoff + s.filesize > (uint64_t)magnitudo)
                        culpa("segmentum '%s' extra fasciculum", s.segname);
                    scribe_mem(m, s.vmaddr, tota + s.fileoff, (size_t)s.filesize);
                }
                nuntio(
                    "segmentum '%s' va=0x%llx mag=%llu prot=%d",
                    s.segname, (unsigned long long)s.vmaddr,
                    (unsigned long long)s.vmsize, s.initprot
                );
                break;
            }
        case LC_MAIN: {
                MachIntroitus mi;
                if (lc.cmdsize < sizeof mi)
                    culpa("LC_MAIN cmdsize");
                memcpy(&mi, tota + off, sizeof mi);
                m->entry       = text_basis + mi.entryoff;
                *habet_lc_main = 1;
                break;
            }
        case LC_UNIXTHREAD: {
            /* cmd | cmdsize | flavor | count | state... */
                if (lc.cmdsize < 16)
                    culpa("LC_UNIXTHREAD cmdsize");
                uint32_t flavor = *(uint32_t *)(tota + off + 8);
                uint32_t count  = *(uint32_t *)(tota + off + 12);
                if (flavor != ARM_THREAD_STATE64)
                    culpa("LC_UNIXTHREAD flavor non est ARM_THREAD_STATE64 (%u)", flavor);
            /* ARM_THREAD_STATE64: 29 * x, fp, lr, sp, pc, cpsr, flags = 68 uint32_t. */
                if (count < 68)
                    culpa("LC_UNIXTHREAD count parvum");
            /* PC in octetis [16 + 8*32 .. 16 + 8*32 + 8) */
                const uint8_t *st = tota + off + 16;
                uint64_t pc;
                memcpy(&pc, st + 8 * 32, 8);
                m->entry = pc;
                break;
            }
        }
        off += lc.cmdsize;
    }
    if (m->entry == 0)
        culpa("nullus LC_MAIN vel LC_UNIXTHREAD inventus");

    free(tota);
}

/* ======================================================================== *
 * V.  Status processoris.
 *
 * AArch64: 31 capsae integrales (X0..X30), SP separatus, PC separatus,
 * NZCV vexilla, 32 capsae SIMD/FP (V0..V31, 128 bit). Index 31 in maxime
 * iussis significat XZR (capsa nulla); in paucis (LD/ST, ADD SP) significat
 * SP. TPIDRRO_EL0 servit pro filo (thread pointer).
 * ======================================================================== */

typedef union {
    struct {
        uint64_t lo, hi;
    } q;
    uint8_t  b[16];
    uint16_t w[8];
    uint32_t d[4];
    uint64_t o[2];
} CapsaV;

typedef struct {
    uint64_t x[31];       /* X0..X30 (X30 = LR) */
    uint64_t sp;
    uint64_t pc;
    uint32_t nzcv;        /* bita 31..28: N Z C V */
    uint64_t tpidrro_el0;
    CapsaV   v[32];
    uint32_t fpcr, fpsr;
} Machina;

#define VN (1u << 31)
#define VZ (1u << 30)
#define VC (1u << 29)
#define VV (1u << 28)

static Machina
mm_init(void)
{
    Machina x;
    memset(&x, 0, sizeof x);
    return x;
}

/* Legere X_i respectans XZR=31. Pro iussis ubi 31 = SP, vocator utitur
 * directe mac->sp vel functione cap_sp. */
static uint64_t
cap_lege(const Machina *m, unsigned i, int sp_est_xzr)
{
    if (i == 31)
        return sp_est_xzr ? 0 : m->sp;
    return m->x[i];
}

static void
cap_scribe(Machina *m, unsigned i, uint64_t v, int sp_est_xzr, int sexaginta_quattuor)
{
    if (!sexaginta_quattuor)
        v &= 0xffffffffu;
    if (i == 31) {
        if (sp_est_xzr)
            return;  /* XZR: scriptura neglecta. */
        m->sp = v;
        return;
    }
    m->x[i] = v;
}

/* ======================================================================== *
 * VI.  Vexilla et conditiones.
 * ======================================================================== */

static void
pone_nzcv_add(Machina *m, uint64_t a, uint64_t b, uint64_t cin, int bit64)
{
    uint64_t r;
    uint64_t c_out, v_out;
    if (bit64) {
        r = a + b + cin;
        c_out = (r < a) || (cin && r == a);
        uint64_t sa = a >> 63, sb = b >> 63, sr = r >> 63;
        v_out = (sa == sb) && (sr != sa);
    } else {
        uint32_t aa = (uint32_t)a, bb = (uint32_t)b, cc = (uint32_t)cin;
        uint64_t wide = (uint64_t)aa + bb + cc;
        r = (uint32_t)wide;
        c_out = (wide >> 32) & 1;
        uint32_t sa = aa >> 31, sb = bb >> 31, sr = ((uint32_t)r) >> 31;
        v_out = (sa == sb) && (sr != sa);
    }
    uint32_t n = bit64 ? (r >> 63) & 1 : ((uint32_t)r >> 31) & 1;
    uint32_t z = bit64 ? (r == 0) : (((uint32_t)r) == 0);
    m->nzcv    = (n << 31) | (z << 30) | ((uint32_t)c_out << 29) | ((uint32_t)v_out << 28);
}

/* SUBS: A - B = A + ~B + 1. Ita cin=1 cum ~B. */
static void
pone_nzcv_sub(Machina *m, uint64_t a, uint64_t b, int bit64)
{
    pone_nzcv_add(m, a, ~b, 1, bit64);
}

static void
pone_nzcv_logic(Machina *m, uint64_t r, int bit64)
{
    uint32_t n = bit64 ? (r >> 63) & 1 : ((uint32_t)r >> 31) & 1;
    uint32_t z = bit64 ? (r == 0) : (((uint32_t)r) == 0);
    m->nzcv    = (n << 31) | (z << 30);
}

static int
conditio(uint32_t nzcv, unsigned cond)
{
    int n = (nzcv >> 31) & 1;
    int z = (nzcv >> 30) & 1;
    int c = (nzcv >> 29) & 1;
    int v = (nzcv >> 28) & 1;
    int r;
    switch (cond >> 1) {
    case 0:
        r = z;
        break;
        /* EQ / NE */
    case 1:
        r = c;
        break;
        /* CS / CC */
    case 2:
        r = n;
        break;
        /* MI / PL */
    case 3:
        r = v;
        break;
        /* VS / VC */
    case 4:
        r = c && !z;
        break;
        /* HI / LS */
    case 5:
        r = (n == v);
        break;
        /* GE / LT */
    case 6:
        r = (n == v) && !z;
        break;
        /* GT / LE */
    case 7:
        r = 1;
        break;
        /* AL / NV */
    default:
        r = 0;
        break;
    }
    /* Nota: AL non vertit (1110 et 1111 utrumque 'verum'). */
    if ((cond & 1) && ((cond >> 1) != 7))
        r = !r;
    return r;
}

/* ======================================================================== *
 * VII.  Decodicatio iussorum AArch64.
 *
 * Iussa sunt 32 bit fixa, minima endia. Primum genus ex bitis 28:25.
 * ======================================================================== */

static uint32_t
extr(uint32_t x, int hi, int lo)
{
    return (x >> lo) & ((1u << (hi - lo + 1)) - 1);
}

/* Signum-extendere i de n bitorum ad 64. */
static int64_t
signext(uint64_t v, int n)
{
    uint64_t m = 1ull << (n - 1);
    v &= (m << 1) - 1;
    return (int64_t)((v ^ m) - m);
}

/* Decodicare immediatum logicum (N:immr:imms) in 64 bit. Reddit 0 si
 * encoding invalidum, 1 si validum; scribit in *out. */
static int
decodica_log_imm(unsigned N, unsigned immr, unsigned imms, int bit64, uint64_t *out)
{
    unsigned len;
    unsigned combi = (N << 6) | ((~imms) & 0x3f);
    if (N) {
        if (!bit64)
            return 0;
        len = 6;
    } else {
        if (combi == 0)
            return 0;
        len        = 0;
        unsigned c = combi;
        while (c >>= 1)
            len++;
        if (len == 0)
            return 0;
    }
    unsigned esize  = 1u << len;
    unsigned levels = esize - 1;
    unsigned S      = imms & levels;
    unsigned R      = immr & levels;
    if (S == levels)
        return 0;
    unsigned d     = (S - R) & levels;
    uint64_t welem = (S + 1 >= 64) ? ~0ull : ((1ull << (S + 1)) - 1);
    /* ROR_esize welem per R. */
    uint64_t welem_r;
    if (esize == 64) {
        welem_r = (R == 0) ? welem : ((welem >> R) | (welem << (64 - R)));
    } else {
        uint64_t mask_e = (esize == 64) ? ~0ull : ((1ull << esize) - 1);
        welem &= mask_e;
        welem_r = ((welem >> R) | (welem << (esize - R))) & mask_e;
    }
    (void)d;
    /* Replica welem_r per 64 / esize. */
    uint64_t res = 0;
    for (unsigned i = 0; i < 64; i += esize)
        res |= welem_r << i;
    if (!bit64)
        res &= 0xffffffffu;
    *out = res;
    return 1;
}

/* Extensiones pro iussis 'extended register': UXTB/H/W/X, SXTB/H/W/X. */
static uint64_t
extende(uint64_t v, unsigned opt, unsigned shift)
{
    switch (opt) {
    case 0:
        v = (uint8_t)v;
        break;
        /* UXTB */
    case 1:
        v = (uint16_t)v;
        break;
        /* UXTH */
    case 2:
        v = (uint32_t)v;
        break;
        /* UXTW */
    case 3:
        break;
        /* UXTX */
    case 4:
        v = (uint64_t)(int8_t)v;
        break;
        /* SXTB */
    case 5:
        v = (uint64_t)(int16_t)v;
        break;
        /* SXTH */
    case 6:
        v = (uint64_t)(int32_t)v;
        break;
        /* SXTW */
    case 7:
        break;
        /* SXTX */
    }
    return v << shift;
}

/* ======================================================================== *
 * VIII.  Grupus: iussa immediata.
 *
 * 100x xxxx xxxx xxxx (op0 = 100x): PC-rel addr, add/sub imm, logical imm,
 * move wide, bitfield, extract.
 * ======================================================================== */

static void exse_imm(Machina *mac, Memoria *mem, uint32_t ins);
static void exse_branch_sys(Machina *mac, Memoria *mem, uint32_t ins);
static void exse_loadstore(Machina *mac, Memoria *mem, uint32_t ins);
static void exse_dpreg(Machina *mac, Memoria *mem, uint32_t ins);
static void exse_dpfp(Machina *mac, Memoria *mem, uint32_t ins);
static void voca_sys(Machina *mac, Memoria *mem);

static void
step(Machina *mac, Memoria *mem)
{
    uint64_t pc0 = mac->pc;
    uint32_t ins = lege_u32(mem, pc0);
    if (verbositas >= 2)
        fprintf(
            stderr, "%s: pc=0x%llx ins=0x%08x\n",
            imitator_nomen, (unsigned long long)pc0, ins
        );
    /* Testatio bitorum 28:25 (op0). */
    unsigned op0 = extr(ins, 28, 25);
    switch (op0) {
    case 0x0:
        if (ins == 0)
            culpa("iussum 0x00000000 ad pc=0x%llx", (unsigned long long)pc0);
        culpa("iussum reservatum 0x%08x ad pc=0x%llx", ins, (unsigned long long)pc0);
    case 0x1: case 0x3:
        culpa("iussum non allocatum 0x%08x ad pc=0x%llx", ins, (unsigned long long)pc0);
    case 0x2:
        culpa("SVE non sustinetur: 0x%08x ad pc=0x%llx", ins, (unsigned long long)pc0);
    case 0x8: case 0x9:
        exse_imm(mac, mem, ins);
        break;
    case 0xA: case 0xB:
        exse_branch_sys(mac, mem, ins);
        break;
    case 0x4: case 0x6: case 0xC: case 0xE:
        exse_loadstore(mac, mem, ins);
        break;
    case 0x5: case 0xD:
        exse_dpreg(mac, mem, ins);
        break;
    case 0x7: case 0xF:
        exse_dpfp(mac, mem, ins);
        break;
    default:
        culpa("op0 invalidus 0x%x ad pc=0x%llx", op0, (unsigned long long)pc0);
    }
}

static void
exsequi(Machina *mac, Memoria *mem)
{
    for (;;)
        step(mac, mem);
}

/* ======================================================================== *
 * IX.  Executio: iussa immediata.
 *    op0 = 100x (bita 28..26 = 100), distinctio per bita 25..23.
 * ======================================================================== */

static void
exse_imm(Machina *mac, Memoria *mem, uint32_t ins)
{
    (void)mem;
    unsigned op = extr(ins, 25, 23);  /* 3 bit: 000..111 */
    unsigned sf = extr(ins, 31, 31);
    int bit64   = sf != 0;

    /* 000: PC-rel (ADR/ADRP).
     * 010/011: Add/Sub (immediate).
     * 100: Logical (immediate).
     * 101: Move wide (immediate).
     * 110: Bitfield.
     * 111: Extract. */
    switch (op) {
    case 0x0: {
        /* ADR/ADRP */
            unsigned opci  = extr(ins, 31, 31);  /* 0=ADR 1=ADRP */
            unsigned immlo = extr(ins, 30, 29);
            uint32_t immhi = extr(ins, 23, 5);
            unsigned rd    = extr(ins, 4, 0);
            int64_t imm    = signext(((uint64_t)immhi << 2) | immlo, 21);
            uint64_t base  = mac->pc;
            uint64_t r;
            if (opci) {
                imm <<= 12;
                r = (base & ~0xfffull) + (uint64_t)imm;
            } else {
                r = base + (uint64_t)imm;
            }
            cap_scribe(mac, rd, r, 1, 1);
            mac->pc += 4;
            return;
        }
    case 0x2: case 0x3: {
        /* Add/Sub (immediate). bit30: op (0=ADD 1=SUB). bit29: S. */
            unsigned opS   = extr(ins, 30, 29);
            unsigned shift = extr(ins, 23, 22);
            if (shift > 1)
                culpa("add/sub imm shift invalidus (0x%08x)", ins);
            uint32_t imm12 = extr(ins, 21, 10);
            unsigned rn    = extr(ins, 9, 5);
            unsigned rd    = extr(ins, 4, 0);
            uint64_t imm   = (uint64_t)imm12 << (shift ? 12 : 0);
            uint64_t a     = cap_lege(mac, rn, 0);  /* rn=31 => SP */
            uint64_t r;
            int is_sub = (opS >> 1) & 1;
            int set    = opS & 1;
            if (is_sub)
                r = a - imm;
            else
                r = a + imm;
            if (!bit64)
                r &= 0xffffffffu;
            if (set) {
                if (is_sub)
                    pone_nzcv_sub(mac, a, imm, bit64);
                else
                    pone_nzcv_add(mac, a, imm, 0, bit64);
                cap_scribe(mac, rd, r, 1, bit64);  /* set => rd=31 est XZR */
            } else {
                cap_scribe(mac, rd, r, 0, bit64);  /* non-set => rd=31 est SP */
            }
            mac->pc += 4;
            return;
        }
    case 0x4: {
        /* Logical (immediate). bits 30:29: opc (00=AND 01=ORR 10=EOR 11=ANDS). */
            unsigned opc  = extr(ins, 30, 29);
            unsigned N    = extr(ins, 22, 22);
            unsigned immr = extr(ins, 21, 16);
            unsigned imms = extr(ins, 15, 10);
            unsigned rn   = extr(ins, 9, 5);
            unsigned rd   = extr(ins, 4, 0);
            uint64_t imm;
            if (!decodica_log_imm(N, immr, imms, bit64, &imm))
                culpa("log-imm invalidum (0x%08x)", ins);
            uint64_t a = cap_lege(mac, rn, 1);  /* XZR pro rn=31 */
            uint64_t r;
            switch (opc) {
            case 0:
                r = a & imm;
                break;
            case 1:
                r = a | imm;
                break;
            case 2:
                r = a ^ imm;
                break;
            case 3:
                r = a & imm;
                break;
            default:
                r = 0;
                break;
            }
            if (!bit64)
                r &= 0xffffffffu;
            if (opc == 3) {
                pone_nzcv_logic(mac, r, bit64);
                cap_scribe(mac, rd, r, 1, bit64);
            } else {
            /* AND/ORR/EOR (imm): rd=31 significat SP (non XZR). */
                cap_scribe(mac, rd, r, 0, bit64);
            }
            mac->pc += 4;
            return;
        }
    case 0x5: {
        /* Move wide (immediate). bits 30:29: opc (00=MOVN 10=MOVZ 11=MOVK). */
            unsigned opc = extr(ins, 30, 29);
            unsigned hw  = extr(ins, 22, 21);
            if (!bit64 && hw > 1)
                culpa("MOV{N,Z,K} 32 hw invalidum (0x%08x)", ins);
            uint32_t imm16 = extr(ins, 20, 5);
            unsigned rd    = extr(ins, 4, 0);
            uint64_t sh    = (uint64_t)hw * 16;
            uint64_t imm   = (uint64_t)imm16 << sh;
            uint64_t r;
            if (opc == 0) {         /* MOVN */
                r = ~imm;
                if (!bit64)
                    r &= 0xffffffffu;
            } else if (opc == 2) {  /* MOVZ */
                r = imm;
            } else if (opc == 3) {  /* MOVK */
                uint64_t cur = cap_lege(mac, rd, 1);
                r = (cur & ~(0xffffull << sh)) | imm;
            } else {
                culpa("MOV wide opc invalidum (0x%08x)", ins);
                return;  /* impossibile */
            }
            cap_scribe(mac, rd, r, 1, bit64);
            mac->pc += 4;
            return;
        }
    case 0x6: {
        /* Bitfield: SBFM (00) / BFM (01) / UBFM (10).
         * Semantica per divisionem imms >= immr:
         *   campus: (Rn >> immr) & ((1 << (imms-immr+1)) - 1)
         *   in Rd positio 0. (UBFX / LSR casus.)
         * alioqui (imms < immr):
         *   campus: Rn & ((1 << (imms+1)) - 1)
         *   deponitur in Rd ad positionem (esize - immr). (BFI / LSL casus.)
         */
            unsigned opc  = extr(ins, 30, 29);
            unsigned N    = extr(ins, 22, 22);
            unsigned immr = extr(ins, 21, 16);
            unsigned imms = extr(ins, 15, 10);
            unsigned rn   = extr(ins, 9, 5);
            unsigned rd   = extr(ins, 4, 0);
            if (bit64 && N != 1)
                culpa("BFM N invalidum (0x%08x)", ins);
            if (!bit64 && N != 0)
                culpa("BFM N invalidum (0x%08x)", ins);
            unsigned esize  = bit64 ? 64 : 32;
            uint64_t mask_e = bit64 ? ~0ull : 0xffffffffull;
            uint64_t src    = cap_lege(mac, rn, 1) & mask_e;
            uint64_t dst    = (opc == 1) ? (cap_lege(mac, rd, 1) & mask_e) : 0;

            unsigned nbits;
            uint64_t field;
            unsigned dst_lsb;
            int sign_bit;
            if (imms >= immr) {
                nbits    = imms - immr + 1;
                field    = (src >> immr) & (nbits >= 64 ? mask_e : ((1ull << nbits) - 1));
                dst_lsb  = 0;
                sign_bit = (int)((src >> imms) & 1);
            } else {
                nbits    = imms + 1;
                field    = src & (nbits >= 64 ? mask_e : ((1ull << nbits) - 1));
                dst_lsb  = esize - immr;
                sign_bit = (int)((src >> imms) & 1);
            }
            uint64_t field_mask = (nbits >= 64 ? mask_e : ((1ull << nbits) - 1)) << dst_lsb;
            uint64_t placed     = (field << dst_lsb) & mask_e;

            uint64_t out;
            if (opc == 0) {
            /* SBFM: bits supra campum = signum bit; bits infra = 0. */
                out = placed;
                if (sign_bit) {
                /* exteriora campi ad 1. */
                    out |= (~field_mask) & mask_e;
                }
            } else if (opc == 1) {
            /* BFM: reliqui bits Rd manent. */
                out = (dst & ~field_mask) | placed;
            } else if (opc == 2) {
            /* UBFM: exteriora = 0. */
                out = placed;
            } else {
                culpa("BFM opc invalidum (0x%08x)", ins);
                return;  /* impossibile */
            }
            cap_scribe(mac, rd, out, 1, bit64);
            mac->pc += 4;
            return;
        }
    case 0x7: {
        /* EXTR: R = (concat(Rn,Rm) >> imms)[31:0 vel 63:0]. */
            unsigned op21 = extr(ins, 30, 29);
            unsigned N    = extr(ins, 22, 22);
            unsigned o0   = extr(ins, 21, 21);
            unsigned rm   = extr(ins, 20, 16);
            unsigned imms = extr(ins, 15, 10);
            unsigned rn   = extr(ins, 9, 5);
            unsigned rd   = extr(ins, 4, 0);
            if (op21 != 0 || o0 != 0)
                culpa("EXTR opc invalidum (0x%08x)", ins);
            if (bit64 && N != 1)
                culpa("EXTR N invalidum (0x%08x)", ins);
            if (!bit64 && N != 0)
                culpa("EXTR N invalidum (0x%08x)", ins);
            uint64_t a = cap_lege(mac, rn, 1);
            uint64_t b = cap_lege(mac, rm, 1);
            uint64_t r;
            if (bit64) {
                if (imms == 0)
                    r = b;
                else
                    r = (b >> imms) | (a << (64 - imms));
            } else {
                uint32_t aa = (uint32_t)a, bb = (uint32_t)b;
                if (imms == 0)
                    r = bb;
                else
                    r = (bb >> imms) | (aa << (32 - imms));
            }
            cap_scribe(mac, rd, r, 1, bit64);
            mac->pc += 4;
            return;
        }
    default:
        culpa("iussum imm non cognitum 0x%08x", ins);
    }
}

/* ======================================================================== *
 * X.  Executio: branch, exception, system.
 *    op0 = 101x.
 * ======================================================================== */

static void
exse_branch_sys(Machina *mac, Memoria *mem, uint32_t ins)
{
    unsigned top = extr(ins, 31, 29);

    /* B / BL: 000101 / 100101 */
    if ((ins & 0x7c000000u) == 0x14000000u) {
        int64_t imm     = signext(extr(ins, 25, 0), 26) << 2;
        int link        = extr(ins, 31, 31);
        uint64_t target = mac->pc + (uint64_t)imm;
        if (link)
            mac->x[30] = mac->pc + 4;
        mac->pc = target;
        return;
    }

    /* CBZ/CBNZ: xxx34 x01 xxxxx... bit24=1, bits 30:25=011010. */
    if ((ins & 0x7e000000u) == 0x34000000u) {
        int bit64   = extr(ins, 31, 31);
        int nz      = extr(ins, 24, 24);
        int64_t imm = signext(extr(ins, 23, 5), 19) << 2;
        unsigned rt = extr(ins, 4, 0);
        uint64_t v  = cap_lege(mac, rt, 1);
        if (!bit64)
            v &= 0xffffffffu;
        int take = nz ? (v != 0) : (v == 0);
        mac->pc  = take ? (mac->pc + (uint64_t)imm) : (mac->pc + 4);
        return;
    }

    /* TBZ/TBNZ: bit24=1, bits30:25 = 011011 */
    if ((ins & 0x7e000000u) == 0x36000000u) {
        unsigned b5   = extr(ins, 31, 31);
        int nz        = extr(ins, 24, 24);
        unsigned b40  = extr(ins, 23, 19);
        int64_t imm   = signext(extr(ins, 18, 5), 14) << 2;
        unsigned rt   = extr(ins, 4, 0);
        unsigned bitn = (b5 << 5) | b40;
        uint64_t v    = cap_lege(mac, rt, 1);
        int bit       = (v >> bitn) & 1;
        int take      = nz ? (bit != 0) : (bit == 0);
        mac->pc       = take ? (mac->pc + (uint64_t)imm) : (mac->pc + 4);
        return;
    }

    /* B.cond / BC.cond: 01010100 ... */
    if ((ins & 0xff000010u) == 0x54000000u) {
        unsigned cond = extr(ins, 3, 0);
        int64_t imm   = signext(extr(ins, 23, 5), 19) << 2;
        int take      = conditio(mac->nzcv, cond);
        mac->pc       = take ? (mac->pc + (uint64_t)imm) : (mac->pc + 4);
        return;
    }

    /* SVC/HVC/SMC (excepta): 11010100 000 imm16 ... */
    if ((ins & 0xffe0001fu) == 0xd4000001u) {
        /* SVC #imm — vocatio systematis. voca_sys ipsa pc movet
         * (plerumque +4, sed pro delivery signali exceptio est). */
        voca_sys(mac, mem);
        return;
    }
    if ((ins & 0xffe0001fu) == 0xd4000000u) {
        culpa("iussum 'UDF'/UD ad pc=0x%llx", (unsigned long long)mac->pc);
    }

    /* System register access (MRS/MSR reg) & hints:
     *   MRS: 1101 0101 0011 ...   = 0xd5300000 masca 0xfff00000
     *   MSR: 1101 0101 0001 ...   = 0xd5100000
     *   Hints (NOP, YIELD, etc.): 0xd503201f NOP.
     */
    if ((ins & 0xfff00000u) == 0xd5300000u) {
        /* MRS Xt, sysreg */
        unsigned o0  = extr(ins, 19, 19);
        unsigned op1 = extr(ins, 18, 16);
        unsigned crn = extr(ins, 15, 12);
        unsigned crm = extr(ins, 11, 8);
        unsigned op2 = extr(ins, 7, 5);
        unsigned rt  = extr(ins, 4, 0);
        /* TPIDRRO_EL0: o0=3 op1=3 crn=13 crm=0 op2=3 — in MRS encoding,
         * bit23..19 contains (op0=1x, o0 bit). Nos tractamus TPIDRRO_EL0
         * et TPIDR_EL0 ut idem valor. */
        if (o0 == 1 && op1 == 3 && crn == 13 && crm == 0 && (op2 == 2 || op2 == 3)) {
            mac->x[rt] = mac->tpidrro_el0;
            mac->pc += 4;
            return;
        }
        /* NZCV: o0=1 op1=3 crn=4 crm=2 op2=0 */
        if (o0 == 1 && op1 == 3 && crn == 4 && crm == 2 && op2 == 0) {
            mac->x[rt] = (uint64_t)mac->nzcv;
            mac->pc += 4;
            return;
        }
        /* FPCR: o0=1 op1=3 crn=4 crm=4 op2=0; FPSR op2=1 */
        if (o0 == 1 && op1 == 3 && crn == 4 && crm == 4 && op2 == 0) {
            mac->x[rt] = (uint64_t)mac->fpcr;
            mac->pc += 4;
            return;
        }
        if (o0 == 1 && op1 == 3 && crn == 4 && crm == 4 && op2 == 1) {
            mac->x[rt] = (uint64_t)mac->fpsr;
            mac->pc += 4;
            return;
        }
        culpa(
            "MRS sysreg non cognitus o0=%u op1=%u crn=%u crm=%u op2=%u (0x%08x)",
            o0, op1, crn, crm, op2, ins
        );
    }
    if ((ins & 0xfff00000u) == 0xd5100000u) {
        unsigned o0  = extr(ins, 19, 19);
        unsigned op1 = extr(ins, 18, 16);
        unsigned crn = extr(ins, 15, 12);
        unsigned crm = extr(ins, 11, 8);
        unsigned op2 = extr(ins, 7, 5);
        unsigned rt  = extr(ins, 4, 0);
        uint64_t v   = cap_lege(mac, rt, 1);
        if (o0 == 1 && op1 == 3 && crn == 4 && crm == 2 && op2 == 0) {
            mac->nzcv = (uint32_t)(v & 0xf0000000u);
            mac->pc += 4;
            return;
        }
        if (o0 == 1 && op1 == 3 && crn == 4 && crm == 4 && op2 == 0) {
            mac->fpcr = (uint32_t)v;
            mac->pc += 4;
            return;
        }
        if (o0 == 1 && op1 == 3 && crn == 4 && crm == 4 && op2 == 1) {
            mac->fpsr = (uint32_t)v;
            mac->pc += 4;
            return;
        }
        if (o0 == 1 && op1 == 3 && crn == 13 && crm == 0 && op2 == 2) {
            /* MSR TPIDR_EL0, Xt — scribimus in tpidrro_el0 idem. */
            mac->tpidrro_el0 = v;
            mac->pc += 4;
            return;
        }
        culpa(
            "MSR sysreg non cognitus o0=%u op1=%u crn=%u crm=%u op2=%u (0x%08x)",
            o0, op1, crn, crm, op2, ins
        );
    }
    /* Hints: 1101 0101 0000 0011 0010 xxxx xxx1 1111.
     * NOP=0xd503201f, YIELD=0xd503203f, ISB=0xd5033fdf, DMB/DSB etc. */
    if (
        (ins & 0xfffff01fu) == 0xd503201fu || /* hint space */
        (ins & 0xfffff0ffu) == 0xd50330bfu || /* DMB */
        (ins & 0xfffff0ffu) == 0xd503309fu || /* DSB */
        (ins & 0xffffffffu) == 0xd5033fdfu
    ) { /* ISB */
        mac->pc += 4;
        return;
    }

    /* Unconditional branch (register): 11010110 0 00 1 1111 0000 00 Rn 00000
     *   BR  rn: 0xd61f0000 | (rn<<5)
     *   BLR rn: 0xd63f0000 | (rn<<5)
     *   RET rn: 0xd65f0000 | (rn<<5)  (rn defalta 30)
     */
    if ((ins & 0xfffffc1fu) == 0xd61f0000u) {
        unsigned rn = extr(ins, 9, 5);
        mac->pc     = mac->x[rn];
        return;
    }
    if ((ins & 0xfffffc1fu) == 0xd63f0000u) {
        unsigned rn = extr(ins, 9, 5);
        uint64_t tg = mac->x[rn];
        mac->x[30]  = mac->pc + 4;
        mac->pc     = tg;
        return;
    }
    if ((ins & 0xfffffc1fu) == 0xd65f0000u) {
        unsigned rn = extr(ins, 9, 5);
        mac->pc     = mac->x[rn];
        return;
    }

    (void)top;
    culpa("branch/sys non cognitum 0x%08x ad pc=0x%llx", ins, (unsigned long long)mac->pc);
}

/* ======================================================================== *
 * XI.  Executio: Loads and Stores.
 *    op0 = x1x0 (bit 27 = 1, bit 25 = 0).
 *
 * Forma usitata:
 *   LDR/STR (immediate, unsigned offset): size:10:1x0 :: 11 101 .. imm12 Rn Rt
 *   LDR/STR (immediate, pre/post):         ditto cum op2=10 (00=unscaled, 01=post, 11=pre)
 *   LDR (literal):                         00/01 011 0 00 imm19 Rt
 *   LDR/STR (register):                    size:111 0 1 1 option S 10 Rm ...
 *   LDP/STP:                               opc : 101 0 010/001/011 imm7 Rt2 Rn Rt
 * ======================================================================== */

static uint64_t
calcula_extreg(const Machina *mac, unsigned rm, unsigned opt, unsigned S, unsigned size)
{
    uint64_t m  = mac->x[rm & 31];
    unsigned sh = S ? size : 0;
    return extende(m, opt, sh);
}

static void
ldst_salubritas(Memoria *mem, uint64_t addr, unsigned bytes)
{
    if (!valet_locus(mem, addr, bytes))
        culpa("LD/ST extra spatium: 0x%llx +%u", (unsigned long long)addr, bytes);
}

static void
exse_loadstore(Machina *mac, Memoria *mem, uint32_t ins)
{
    /* LDR (literal): 0x18000000 masca 0x3b000000 */
    if ((ins & 0x3b000000u) == 0x18000000u) {
        unsigned opc  = extr(ins, 31, 30);
        int64_t imm   = signext(extr(ins, 23, 5), 19) << 2;
        unsigned rt   = extr(ins, 4, 0);
        uint64_t addr = mac->pc + (uint64_t)imm;
        if (opc == 0) {
            uint32_t v = lege_u32(mem, addr);
            cap_scribe(mac, rt, v, 1, 0);
        } else if (opc == 1) {
            uint64_t v = lege_u64(mem, addr);
            cap_scribe(mac, rt, v, 1, 1);
        } else if (opc == 2) {
            uint32_t v = lege_u32(mem, addr);
            cap_scribe(mac, rt, (uint64_t)(int32_t)v, 1, 1);
        } else {
            culpa("PRFM literal non sustinetur (0x%08x)", ins);
        }
        mac->pc += 4;
        return;
    }

    /* LDP/STP: bits 29:27 = 101, bit 26 = 0, bits 25:23 = 0xx (010 signed, 011 preidx, 001 postidx). */
    if ((ins & 0x3a000000u) == 0x28000000u) {
        unsigned opc = extr(ins, 31, 30);
        unsigned V   = extr(ins, 26, 26);
        unsigned idx = extr(ins, 24, 23);  /* 01 post, 10 signed, 11 pre, 00 nontemp */
        unsigned L   = extr(ins, 22, 22);
        int64_t imm7 = signext(extr(ins, 21, 15), 7);
        unsigned rt2 = extr(ins, 14, 10);
        unsigned rn  = extr(ins, 9, 5);
        unsigned rt  = extr(ins, 4, 0);
        if (V) {
            /* SIMD/FP LDP/STP: opc=00 4 octeti, 01 8, 10 16. */
            unsigned size;
            if (opc == 0)
                size = 4;
            else if (opc == 1)
                size = 8;
            else if (opc == 2)
                size = 16;
            else {
                culpa("LDP/STP V opc invalidum (0x%08x)", ins);
                return;
                /* impossibile */
            }
            int64_t off   = imm7 * (int64_t)size;
            uint64_t base = cap_lege(mac, rn, 0);
            uint64_t addr = (idx == 1) ? base : base + (uint64_t)off;
            if (L) {
                uint8_t buf[16*2];
                lege_mem(mem, addr, buf, size*2);
                memset(&mac->v[rt], 0, sizeof(CapsaV));
                memset(&mac->v[rt2], 0, sizeof(CapsaV));
                memcpy(&mac->v[rt], buf, size);
                memcpy(&mac->v[rt2], buf + size, size);
            } else {
                uint8_t buf[16*2];
                memcpy(buf, &mac->v[rt], size);
                memcpy(buf + size, &mac->v[rt2], size);
                scribe_mem(mem, addr, buf, size*2);
            }
            if (idx == 1 || idx == 3)
                cap_scribe(mac, rn, base + (uint64_t)off, 0, 1);
            mac->pc += 4;
            return;
        }
        unsigned size;
        int sign32 = 0;
        switch (opc) {
        case 0:
            size = 4;
            break;
        case 1:
            size   = 4;
            sign32 = 1;
            break;
            /* LDPSW */
        case 2:
            size = 8;
            break;
        default:
            culpa("LDP/STP opc 11 invalidum (0x%08x)", ins);
            return;
            /* impossibile */
        }
        int64_t off   = imm7 * (int64_t)size;
        uint64_t base = cap_lege(mac, rn, 0);
        uint64_t addr = (idx == 1) ? base : base + (uint64_t)off;
        if (L) {
            uint64_t a, b;
            if (size == 4) {
                a = lege_u32(mem, addr);
                b = lege_u32(mem, addr + 4);
                if (sign32) {
                    a = (uint64_t)(int32_t)a;
                    b = (uint64_t)(int32_t)b;
                    cap_scribe(mac, rt,  a, 1, 1);
                    cap_scribe(mac, rt2, b, 1, 1);
                } else {
                    cap_scribe(mac, rt,  a, 1, 0);
                    cap_scribe(mac, rt2, b, 1, 0);
                }
            } else {
                a = lege_u64(mem, addr);
                b = lege_u64(mem, addr + 8);
                cap_scribe(mac, rt,  a, 1, 1);
                cap_scribe(mac, rt2, b, 1, 1);
            }
        } else {
            uint64_t a = cap_lege(mac, rt,  1);
            uint64_t b = cap_lege(mac, rt2, 1);
            if (size == 4) {
                scribe_u32(mem, addr,     (uint32_t)a);
                scribe_u32(mem, addr + 4, (uint32_t)b);
            } else {
                scribe_u64(mem, addr,     a);
                scribe_u64(mem, addr + 8, b);
            }
        }
        if (idx == 1 || idx == 3)
            cap_scribe(mac, rn, base + (uint64_t)off, 0, 1);
        mac->pc += 4;
        return;
    }

    /* LDR/STR (immediate + register + unscaled) family:
     *   bits 31:30 size, 29:27 = 111, 26 V, 25:24 = 00 (unsize/unscaled/reg/imm)
     *   in canonical: size:111 V 00 op11 op10 ...  (patterns differ)
     * Encoding classes:
     *   Unscaled imm (LDUR/STUR):  size:111 V 00 opc 0 imm9 00 Rn Rt
     *   Imm post/pre-idx:          size:111 V 00 opc 0 imm9 01/11 Rn Rt
     *   Register:                  size:111 V 00 opc 1 Rm option S 10 Rn Rt
     *   Unsigned offset:           size:111 V 01 opc imm12 Rn Rt
     */
    if ((ins & 0x3b000000u) == 0x38000000u) {
        /* 00: unscaled, post, pre, register. */
        unsigned size  = extr(ins, 31, 30);
        unsigned V     = extr(ins, 26, 26);
        unsigned opc   = extr(ins, 23, 22);
        unsigned op1   = extr(ins, 21, 21);
        unsigned imm9  = extr(ins, 20, 12);
        unsigned op2   = extr(ins, 11, 10);
        unsigned rn    = extr(ins, 9, 5);
        unsigned rt    = extr(ins, 4, 0);
        unsigned bytes = 1u << size;
        int64_t off    = signext(imm9, 9);
        uint64_t base  = cap_lege(mac, rn, 0);
        uint64_t addr;
        int write_back = 0;
        int reg_form   = 0;
        unsigned rm    = 0, option = 0, S = 0;
        if (op1 == 0) {
            if (op2 == 0) { addr = base + (uint64_t)off; }       /* unscaled */
            else if (op2 == 1) {
                addr       = base;
                write_back = 1;
            }/* post */
            else if (op2 == 3) {
                addr       = base + (uint64_t)off;
                write_back = 1;
            }/* pre */
            else
                culpa("LDR imm op2=10 non valet (0x%08x)", ins);
        } else {
            if (op2 != 2)
                culpa("LDR reg op2 invalidum (0x%08x)", ins);
            reg_form = 1;
            rm       = extr(ins, 20, 16);
            option   = extr(ins, 15, 13);
            S        = extr(ins, 12, 12);
            addr     = base + calcula_extreg(mac, rm, option, S, size);
            (void)reg_form;
        }
        int is_load   = (opc & 1) || (opc == 2 && bytes < 8);
        int is_signed = (opc == 2 || opc == 3);
        int bit64_res = is_signed ? (opc == 2) : (size == 3);
        if (V) {
            /* SIMD: bytes might be 16 if size=0 and opc=2/3. */
            unsigned simd_bytes = bytes;
            if (opc & 2)
                simd_bytes = 16;  /* opc=10 or 11 => 128-bit */
            if (opc == 2) {
                /* STR/LDR 128-bit: actually opc encoding is size/opc dependent:
                 *   for V=1: size=00 opc=10 => 128-bit store/load (STR Qt/LDR Qt).
                 * Pro simplicitate: (size,V=1,opc=10) => 16 byte. */
                if (size == 0)
                    simd_bytes = 16;
            }
            ldst_salubritas(mem, addr, simd_bytes);
            if (opc & 1) {  /* load */
                memset(&mac->v[rt], 0, sizeof(CapsaV));
                lege_mem(mem, addr, &mac->v[rt], simd_bytes);
            } else {        /* store */
                scribe_mem(mem, addr, &mac->v[rt], simd_bytes);
            }
        } else if (is_load) {
            uint64_t v;
            switch (bytes) {
            case 1:
                v = lege_u8 (mem, addr);
                break;
            case 2:
                v = lege_u16(mem, addr);
                break;
            case 4:
                v = lege_u32(mem, addr);
                break;
            case 8:
                v = lege_u64(mem, addr);
                break;
            default:
                culpa("LD bytes invalidum");
            }
            if (is_signed) {
                switch (bytes) {
                case 1:
                    v = (uint64_t)(int64_t)(int8_t)v;
                    break;
                case 2:
                    v = (uint64_t)(int64_t)(int16_t)v;
                    break;
                case 4:
                    v = (uint64_t)(int64_t)(int32_t)v;
                    break;
                }
                if (!bit64_res)
                    v &= 0xffffffffu;
            }
            cap_scribe(mac, rt, v, 1, bit64_res);
        } else {
            uint64_t v = cap_lege(mac, rt, 1);
            switch (bytes) {
            case 1:
                scribe_u8 (mem, addr, (uint8_t)v);
                break;
            case 2:
                scribe_u16(mem, addr, (uint16_t)v);
                break;
            case 4:
                scribe_u32(mem, addr, (uint32_t)v);
                break;
            case 8:
                scribe_u64(mem, addr, v);
                break;
            default:
                culpa("ST bytes invalidum");
            }
        }
        if (write_back)
            cap_scribe(mac, rn, base + (uint64_t)off, 0, 1);
        mac->pc += 4;
        return;
    }

    if ((ins & 0x3b000000u) == 0x39000000u) {
        /* Unsigned offset: size:111 V 01 opc imm12 Rn Rt */
        unsigned size  = extr(ins, 31, 30);
        unsigned V     = extr(ins, 26, 26);
        unsigned opc   = extr(ins, 23, 22);
        uint32_t imm12 = extr(ins, 21, 10);
        unsigned rn    = extr(ins, 9, 5);
        unsigned rt    = extr(ins, 4, 0);
        unsigned bytes = 1u << size;
        uint64_t base  = cap_lege(mac, rn, 0);
        uint64_t addr;
        if (V) {
            unsigned simd_bytes = bytes;
            if ((opc & 2) && size == 0)
                simd_bytes = 16;
            addr = base + (uint64_t)imm12 * simd_bytes;
            if (opc & 1) {
                memset(&mac->v[rt], 0, sizeof(CapsaV));
                lege_mem(mem, addr, &mac->v[rt], simd_bytes);
            } else {
                scribe_mem(mem, addr, &mac->v[rt], simd_bytes);
            }
            mac->pc += 4;
            return;
        }
        addr = base + (uint64_t)imm12 * bytes;
        int is_load = (opc & 1) || (opc == 2 && bytes < 8);
        int is_signed = (opc == 2 || opc == 3);
        int bit64_res = is_signed ? (opc == 2) : (size == 3);
        if (opc == 3 && bytes == 8)
            culpa("PRFM non sustinetur (0x%08x)", ins);
        if (is_load) {
            uint64_t v;
            switch (bytes) {
            case 1:
                v = lege_u8 (mem, addr);
                break;
            case 2:
                v = lege_u16(mem, addr);
                break;
            case 4:
                v = lege_u32(mem, addr);
                break;
            case 8:
                v = lege_u64(mem, addr);
                break;
            default:
                culpa("LD bytes invalidum");
            }
            if (is_signed) {
                switch (bytes) {
                case 1:
                    v = (uint64_t)(int64_t)(int8_t)v;
                    break;
                case 2:
                    v = (uint64_t)(int64_t)(int16_t)v;
                    break;
                case 4:
                    v = (uint64_t)(int64_t)(int32_t)v;
                    break;
                }
                if (!bit64_res)
                    v &= 0xffffffffu;
            }
            cap_scribe(mac, rt, v, 1, bit64_res);
        } else {
            uint64_t v = cap_lege(mac, rt, 1);
            switch (bytes) {
            case 1:
                scribe_u8 (mem, addr, (uint8_t)v);
                break;
            case 2:
                scribe_u16(mem, addr, (uint16_t)v);
                break;
            case 4:
                scribe_u32(mem, addr, (uint32_t)v);
                break;
            case 8:
                scribe_u64(mem, addr, v);
                break;
            }
        }
        mac->pc += 4;
        return;
    }

    culpa("LD/ST non cognitum 0x%08x ad pc=0x%llx", ins, (unsigned long long)mac->pc);
}

/* ======================================================================== *
 * XII.  Executio: Data processing (register).
 *    op0 = x101.
 * ======================================================================== */

static uint64_t
shiftus(uint64_t v, unsigned type, unsigned amt, int bit64)
{
    unsigned esize = bit64 ? 64 : 32;
    uint64_t m     = bit64 ? ~0ull : 0xffffffffull;
    v &= m;
    amt &= esize - 1;
    switch (type) {
    case 0:
        v = (v << amt) & m;
        break;
        /* LSL */
    case 1:
        v = (v >> amt);
        break;
        /* LSR */
    case 2:
        if (bit64)
            v = (uint64_t)((int64_t)v >> amt);
        else
            v = (uint64_t)((uint32_t)((int32_t)(uint32_t)v >> amt));
        break;                                                       /* ASR */
    case 3:
        if (amt == 0)
            break;
        if (bit64)
            v = (v >> amt) | (v << (64 - amt));
        else {
            uint32_t w = (uint32_t)v;
            w = (w >> amt) | (w << (32 - amt));
            v = w;
        }
        break;                                                       /* ROR */
    }
    return v & m;
}

static void
exse_dpreg(Machina *mac, Memoria *mem, uint32_t ins)
{
    (void)mem;
    unsigned sf = extr(ins, 31, 31);
    int bit64   = sf != 0;

    /* Logical (shifted register): bits 31:24 = sf:0:0 01010 shift:N...
     *   pattern: sf:op:11010:shift(2):N:Rm:imm6:Rn:Rd
     *   mask 0x1f000000 == 0x0a000000 */
    if ((ins & 0x1f000000u) == 0x0a000000u) {
        unsigned opc   = extr(ins, 30, 29);
        unsigned shift = extr(ins, 23, 22);
        unsigned N     = extr(ins, 21, 21);
        unsigned rm    = extr(ins, 20, 16);
        unsigned imm6  = extr(ins, 15, 10);
        unsigned rn    = extr(ins, 9, 5);
        unsigned rd    = extr(ins, 4, 0);
        uint64_t a     = cap_lege(mac, rn, 1);
        uint64_t b     = shiftus(cap_lege(mac, rm, 1), shift, imm6, bit64);
        if (N)
            b = ~b;
        uint64_t r;
        switch (opc) {
        case 0:
            r = a & b;
            break;
        case 1:
            r = a | b;
            break;
        case 2:
            r = a ^ b;
            break;
        case 3:
            r = a & b;
            break;
        }
        if (!bit64)
            r &= 0xffffffffu;
        if (opc == 3) {
            pone_nzcv_logic(mac, r, bit64);
        }
        cap_scribe(mac, rd, r, 1, bit64);
        mac->pc += 4;
        return;
    }

    /* Add/Sub (shifted register): sf:op:S:01011:shift:0:Rm:imm6:Rn:Rd
     * mask 0x1f200000 == 0x0b000000 */
    if ((ins & 0x1f200000u) == 0x0b000000u) {
        unsigned op_   = extr(ins, 30, 30);
        unsigned S     = extr(ins, 29, 29);
        unsigned shift = extr(ins, 23, 22);
        unsigned rm    = extr(ins, 20, 16);
        unsigned imm6  = extr(ins, 15, 10);
        unsigned rn    = extr(ins, 9, 5);
        unsigned rd    = extr(ins, 4, 0);
        if (shift == 3)
            culpa("add/sub shift=11 reservatum (0x%08x)", ins);
        uint64_t a = cap_lege(mac, rn, 1);
        uint64_t b = shiftus(cap_lege(mac, rm, 1), shift, imm6, bit64);
        uint64_t r = op_ ? (a - b) : (a + b);
        if (!bit64)
            r &= 0xffffffffu;
        if (S) {
            if (op_)
                pone_nzcv_sub(mac, a, b, bit64);
            else
                pone_nzcv_add(mac, a, b, 0, bit64);
        }
        cap_scribe(mac, rd, r, 1, bit64);
        mac->pc += 4;
        return;
    }

    /* Add/Sub (extended register): sf:op:S:01011:001:Rm:option:imm3:Rn:Rd
     * mask 0x1f200000 == 0x0b200000 */
    if ((ins & 0x1f200000u) == 0x0b200000u) {
        unsigned op_    = extr(ins, 30, 30);
        unsigned S      = extr(ins, 29, 29);
        unsigned rm     = extr(ins, 20, 16);
        unsigned option = extr(ins, 15, 13);
        unsigned imm3   = extr(ins, 12, 10);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        /* rn=31 => SP (non XZR). Similiter pro non-set rd. */
        uint64_t a = cap_lege(mac, rn, 0);
        uint64_t b = extende(cap_lege(mac, rm, 1), option, imm3);
        uint64_t r = op_ ? (a - b) : (a + b);
        if (!bit64)
            r &= 0xffffffffu;
        if (S) {
            if (op_)
                pone_nzcv_sub(mac, a, b, bit64);
            else
                pone_nzcv_add(mac, a, b, 0, bit64);
            cap_scribe(mac, rd, r, 1, bit64);
        } else {
            cap_scribe(mac, rd, r, 0, bit64);
        }
        mac->pc += 4;
        return;
    }

    /* Add/Sub (with carry): sf:op:S:11010000:Rm:000000:Rn:Rd
     * mask 0x1fe00000 == 0x1a000000 */
    if ((ins & 0x1fe00000u) == 0x1a000000u) {
        unsigned op_ = extr(ins, 30, 30);
        unsigned S   = extr(ins, 29, 29);
        unsigned rm  = extr(ins, 20, 16);
        unsigned rn  = extr(ins, 9, 5);
        unsigned rd  = extr(ins, 4, 0);
        uint64_t a   = cap_lege(mac, rn, 1);
        uint64_t b   = cap_lege(mac, rm, 1);
        uint64_t cin = (mac->nzcv >> 29) & 1;
        uint64_t r;
        if (op_) {
            /* SBC: a + ~b + C */
            r = a + ~b + cin;
            if (S)
                pone_nzcv_add(mac, a, ~b, cin, bit64);
        } else {
            r = a + b + cin;
            if (S)
                pone_nzcv_add(mac, a, b, cin, bit64);
        }
        if (!bit64)
            r &= 0xffffffffu;
        cap_scribe(mac, rd, r, 1, bit64);
        mac->pc += 4;
        return;
    }

    /* Conditional select: sf:op:S:11010100:Rm:cond:op2:Rn:Rd
     * mask 0x1fe00000 == 0x1a800000 */
    if ((ins & 0x1fe00000u) == 0x1a800000u) {
        unsigned op_  = extr(ins, 30, 30);
        unsigned S    = extr(ins, 29, 29);
        unsigned rm   = extr(ins, 20, 16);
        unsigned cond = extr(ins, 15, 12);
        unsigned op2  = extr(ins, 11, 10);
        unsigned rn   = extr(ins, 9, 5);
        unsigned rd   = extr(ins, 4, 0);
        if (S)
            culpa("CSEL S=1 reservatum (0x%08x)", ins);
        uint64_t a = cap_lege(mac, rn, 1);
        uint64_t b = cap_lege(mac, rm, 1);
        uint64_t r;
        if (conditio(mac->nzcv, cond)) {
            r = a;
        } else {
            if (op_ == 0 && op2 == 0)
                r = b;                      /* CSEL  */
            else if (op_ == 0 && op2 == 1)
                r = b + 1;                  /* CSINC */
            else if (op_ == 1 && op2 == 0)
                r = ~b;                     /* CSINV */
            else if (op_ == 1 && op2 == 1)
                r = (uint64_t)(-(int64_t)b);/* CSNEG */
            else {
                culpa("CSEL variant invalidum (0x%08x)", ins);
                return;
                /* impossibile */
            }
        }
        if (!bit64)
            r &= 0xffffffffu;
        cap_scribe(mac, rd, r, 1, bit64);
        mac->pc += 4;
        return;
    }

    /* Conditional compare (register): sf:op:S:11010010:Rm:cond:0:0:Rn:0:nzcv
     * mask 0x1fe00800 == 0x1a400000 */
    if ((ins & 0x1fe00800u) == 0x1a400000u) {
        unsigned op_ = extr(ins, 30, 30);
        unsigned S   = extr(ins, 29, 29);
        unsigned rm  = extr(ins, 20, 16);
        unsigned cond = extr(ins, 15, 12);
        unsigned rn  = extr(ins, 9, 5);
        unsigned nzcv_if_false = extr(ins, 3, 0);
        if (!S)
            culpa("CCMP S=0 reservatum (0x%08x)", ins);
        uint64_t a = cap_lege(mac, rn, 1);
        uint64_t b = cap_lege(mac, rm, 1);
        if (conditio(mac->nzcv, cond)) {
            if (op_)
                pone_nzcv_sub(mac, a, b, bit64);
            else
                pone_nzcv_add(mac, a, b, 0, bit64);
        } else {
            mac->nzcv = nzcv_if_false << 28;
        }
        mac->pc += 4;
        return;
    }
    /* Conditional compare (immediate): mask 0x1fe00800 == 0x1a400800 */
    if ((ins & 0x1fe00800u) == 0x1a400800u) {
        unsigned op_ = extr(ins, 30, 30);
        unsigned S   = extr(ins, 29, 29);
        unsigned imm5 = extr(ins, 20, 16);
        unsigned cond = extr(ins, 15, 12);
        unsigned rn   = extr(ins, 9, 5);
        unsigned nzcv_if_false = extr(ins, 3, 0);
        if (!S)
            culpa("CCMP-imm S=0 reservatum (0x%08x)", ins);
        uint64_t a = cap_lege(mac, rn, 1);
        uint64_t b = imm5;
        if (conditio(mac->nzcv, cond)) {
            if (op_)
                pone_nzcv_sub(mac, a, b, bit64);
            else
                pone_nzcv_add(mac, a, b, 0, bit64);
        } else {
            mac->nzcv = nzcv_if_false << 28;
        }
        mac->pc += 4;
        return;
    }

    /* Data-processing (1 source): sf:1:S:11010110:opcode2:opcode:Rn:Rd
     * mask 0x5fe00000 == 0x5ac00000 */
    if ((ins & 0x5fe00000u) == 0x5ac00000u) {
        unsigned opcode = extr(ins, 15, 10);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        uint64_t a      = cap_lege(mac, rn, 1);
        if (!bit64)
            a &= 0xffffffffu;
        uint64_t r;
        switch (opcode) {
        case 0x00: /* RBIT */
            r = 0;
            for (unsigned i = 0; i < (bit64 ? 64u : 32u); i++) {
                uint64_t bit = (a >> i) & 1;
                r |= bit << ((bit64 ? 63 : 31) - i);
            }
            break;
        case 0x01: /* REV16 */
            if (bit64) {
                r = 0;
                for (int i = 0; i < 4; i++) {
                    uint64_t w = (a >> (i*16)) & 0xffff;
                    w = ((w & 0xff) << 8) | ((w >> 8) & 0xff);
                    r |= w << (i*16);
                }
            } else {
                uint32_t w = (uint32_t)a;
                r = (((w >> 8) & 0x00ff00ff) | ((w << 8) & 0xff00ff00));
            }
            break;
        case 0x02: /* REV32 (in 64) or REV (in 32) */
            if (bit64) {
                r = 0;
                for (int i = 0; i < 2; i++) {
                    uint64_t w = (a >> (i*32)) & 0xffffffff;
                    w = __builtin_bswap32((uint32_t)w);
                    r |= w << (i*32);
                }
            } else {
                r = __builtin_bswap32((uint32_t)a);
            }
            break;
        case 0x03: /* REV (64) */
            if (!bit64)
                culpa("REV non-64 opcode=3 invalidum");
            r = __builtin_bswap64(a);
            break;
        case 0x04: /* CLZ */
            if (a == 0)
                r = bit64 ? 64 : 32;
            else
                r = bit64 ? __builtin_clzll(a) : __builtin_clz((uint32_t)a);
            break;
        case 0x05: /* CLS */
            {
                uint64_t m = bit64 ? ~0ull : 0xffffffffull;
                uint64_t s = (a & (1ull << (bit64 ? 63 : 31))) ? (~a & m) : (a & m);
                if (s == 0)
                    r = (bit64 ? 63 : 31);
                else
                    r = (bit64 ? __builtin_clzll(s) : __builtin_clz((uint32_t)s)) - 1;
            }
            break;
        default:
            culpa("DP-1src opcode %02x non sustinetur (0x%08x)", opcode, ins);
            return;  /* impossibile */
        }
        if (!bit64)
            r &= 0xffffffffu;
        cap_scribe(mac, rd, r, 1, bit64);
        mac->pc += 4;
        return;
    }

    /* Data-processing (2 source): sf:0:S:11010110:Rm:opcode:Rn:Rd
     * mask 0x5fe00000 == 0x1ac00000 */
    if ((ins & 0x5fe00000u) == 0x1ac00000u) {
        unsigned S      = extr(ins, 29, 29);
        unsigned rm     = extr(ins, 20, 16);
        unsigned opcode = extr(ins, 15, 10);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        if (S)
            culpa("DP-2src S=1 (0x%08x)", ins);
        uint64_t a = cap_lege(mac, rn, 1);
        uint64_t b = cap_lege(mac, rm, 1);
        if (!bit64) {
            a &= 0xffffffffu;
            b &= 0xffffffffu;
        }
        uint64_t r;
        switch (opcode) {
        case 0x02: /* UDIV */
            if (b == 0)
                r = 0;
            else
                r = bit64 ? (a / b) : ((uint32_t)a / (uint32_t)b);
            break;
        case 0x03: /* SDIV */
            if (b == 0)
                r = 0;
            else if (bit64) {
                int64_t sa = (int64_t)a, sb = (int64_t)b;
                if (sa == INT64_MIN && sb == -1)
                    r = (uint64_t)INT64_MIN;
                else
                    r = (uint64_t)(sa / sb);
            } else {
                int32_t sa = (int32_t)a, sb = (int32_t)b;
                if (sa == INT32_MIN && sb == -1)
                    r = (uint32_t)INT32_MIN;
                else
                    r = (uint32_t)(sa / sb);
            }
            break;
        case 0x08: /* LSLV */
            r = shiftus(a, 0, (unsigned)b, bit64);
            break;
        case 0x09: /* LSRV */
            r = shiftus(a, 1, (unsigned)b, bit64);
            break;
        case 0x0a: /* ASRV */
            r = shiftus(a, 2, (unsigned)b, bit64);
            break;
        case 0x0b: /* RORV */
            r = shiftus(a, 3, (unsigned)b, bit64);
            break;
        default:
            culpa("DP-2src opcode %02x non sustinetur (0x%08x)", opcode, ins);
            return;  /* impossibile */
        }
        if (!bit64)
            r &= 0xffffffffu;
        cap_scribe(mac, rd, r, 1, bit64);
        mac->pc += 4;
        return;
    }

    /* Data-processing (3 source): sf:00:11011:op31:Rm:o0:Ra:Rn:Rd
     * mask 0x1f000000 == 0x1b000000 */
    if ((ins & 0x1f000000u) == 0x1b000000u) {
        unsigned op31 = extr(ins, 23, 21);
        unsigned rm   = extr(ins, 20, 16);
        unsigned o0   = extr(ins, 15, 15);
        unsigned ra   = extr(ins, 14, 10);
        unsigned rn   = extr(ins, 9, 5);
        unsigned rd   = extr(ins, 4, 0);
        uint64_t va   = cap_lege(mac, ra, 1);
        uint64_t vn   = cap_lege(mac, rn, 1);
        uint64_t vm   = cap_lege(mac, rm, 1);
        uint64_t r    = 0;
        /* 0: MADD/MSUB (sf determines 32/64).
         * 001: SMADDL 010 SMULH 101 UMADDL 110 UMULH (X only). */
        if (op31 == 0) {
            if (!bit64) {
                vn &= 0xffffffffu;
                vm &= 0xffffffffu;
                va &= 0xffffffffu;
            }
            uint64_t prod = vn * vm;
            r = o0 ? (va - prod) : (va + prod);
        } else if (bit64 && op31 == 0x1) {
            int64_t p = (int64_t)(int32_t)vn * (int64_t)(int32_t)vm;
            r         = o0 ? (va - (uint64_t)p) : (va + (uint64_t)p);
        } else if (bit64 && op31 == 0x5) {
            uint64_t p = (uint64_t)(uint32_t)vn * (uint64_t)(uint32_t)vm;
            r = o0 ? (va - p) : (va + p);
        } else if (bit64 && op31 == 0x2) {
            int64_t hi;
            uint64_t lo;
            mul64_128s((int64_t)vn, (int64_t)vm, &hi, &lo);
            r = (uint64_t)hi;
        } else if (bit64 && op31 == 0x6) {
            uint64_t hi, lo;
            mul64_128u(vn, vm, &hi, &lo);
            r = hi;
        } else {
            culpa("DP-3src variant invalidum (0x%08x)", ins);
        }
        if (!bit64)
            r &= 0xffffffffu;
        cap_scribe(mac, rd, r, 1, bit64);
        mac->pc += 4;
        return;
    }

    culpa("DP-reg non cognitum 0x%08x ad pc=0x%llx", ins, (unsigned long long)mac->pc);
}

/* ======================================================================== *
 * XIII.  Executio: FP/SIMD (minima sustentatio).
 *
 * Plerumque ccc+vulcanus non SIMD generant; sed oportet scrutari LDR/STR
 * de V-regibus (iam in LD/ST tractata) et aliquot movs FP. Si iussum
 * incognitum, culpa.
 * ======================================================================== */

/* Expandere imm8 VFP ad duplicem (IEEE 754).  Forma imm8:
 *   bit 7   = signum
 *   bit 6   = b
 *   bits 5:4 = cd
 *   bits 3:0 = efgh (mantissa alta, residuum zero)
 * Exponens 11 bit = NOT(b) : Replicate(b, 8) : cd
 * Mantissa 52 bit = efgh : Zeros(48). */
static double
vfp_expand_double(unsigned imm8)
{
    uint64_t sign = (imm8 >> 7) & 1;
    uint64_t b    = (imm8 >> 6) & 1;
    uint64_t cd   = (imm8 >> 4) & 3;
    uint64_t efgh = imm8 & 0xf;
    uint64_t exp11 = b
        ? ((uint64_t)0 << 10) | ((uint64_t)0xff << 2) | cd
        : ((uint64_t)1 << 10) | ((uint64_t)0x00 << 2) | cd;
    uint64_t bits = (sign << 63) | (exp11 << 52) | (efgh << 48);
    double d;
    memcpy(&d, &bits, 8);
    return d;
}

/* Simile pro simplici (32-bit). Exp 5 bit = NOT(b):b:b:cd. */
static float
vfp_expand_single(unsigned imm8)
{
    uint32_t sign = (imm8 >> 7) & 1;
    uint32_t b    = (imm8 >> 6) & 1;
    uint32_t cd   = (imm8 >> 4) & 3;
    uint32_t efgh = imm8 & 0xf;
    uint32_t exp5 = b
        ? ((uint32_t)0 << 4) | ((uint32_t)0x3 << 2) | cd
        : ((uint32_t)1 << 4) | ((uint32_t)0x0 << 2) | cd;
    uint32_t bits = (sign << 31) | (exp5 << 23) | (efgh << 19);
    float f;
    memcpy(&f, &bits, 4);
    return f;
}

static void
scribe_fp_scalar(Machina *mac, unsigned rd, unsigned type, uint64_t bits)
{
    memset(&mac->v[rd], 0, sizeof(CapsaV));
    if (type == 0) {
        uint32_t w = (uint32_t)bits;
        memcpy(&mac->v[rd], &w, 4);
    } else if (type == 1) {
        memcpy(&mac->v[rd], &bits, 8);
    } else {
        culpa("FP type %u non sustinetur", type);
    }
}

static uint64_t
lege_fp_scalar(const Machina *mac, unsigned rn, unsigned type)
{
    uint64_t v     = 0;
    unsigned bytes = (type == 0) ? 4 : (type == 1) ? 8 : 0;
    if (bytes == 0)
        culpa("FP type %u non sustinetur", type);
    memcpy(&v, &mac->v[rn], bytes);
    return v;
}

static void
exse_dpfp(Machina *mac, Memoria *mem, uint32_t ins)
{
    (void)mem;

    /* FMOV (scalar, immediate): 00011110:type:1:imm8(8):100:imm5(5):Rd.
     * 12:10 = 100, 9:5 = imm5 (debet 00000). */
    if ((ins & 0xff201fe0u) == 0x1e201000u && extr(ins, 9, 5) == 0) {
        unsigned type = extr(ins, 23, 22);
        unsigned imm8 = extr(ins, 20, 13);
        unsigned rd   = extr(ins, 4, 0);
        if (type == 0) {
            float f = vfp_expand_single(imm8);
            uint32_t u;
            memcpy(&u, &f, 4);
            scribe_fp_scalar(mac, rd, 0, u);
        } else if (type == 1) {
            double d = vfp_expand_double(imm8);
            uint64_t u;
            memcpy(&u, &d, 8);
            scribe_fp_scalar(mac, rd, 1, u);
        } else {
            culpa("FMOV imm type %u non sustinetur (0x%08x)", type, ins);
        }
        mac->pc += 4;
        return;
    }

    /* FP data-processing (1 source): 00011110:type:1:opcode(6):10000:Rn:Rd
     * 14:10 = 10000. opcode ex 20:15. */
    if ((ins & 0xff207c00u) == 0x1e204000u) {
        unsigned type   = extr(ins, 23, 22);
        unsigned opcode = extr(ins, 20, 15);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        uint64_t src    = lege_fp_scalar(mac, rn, type);
        if (opcode == 0) {
            /* FMOV (register). */
            scribe_fp_scalar(mac, rd, type, src);
        } else if (type == 1 && opcode == 1) {
            double d;
            memcpy(&d, &src, 8);
            d = d < 0 ? -d : d;
            uint64_t u;
            memcpy(&u, &d, 8);
            scribe_fp_scalar(mac, rd, 1, u);  /* FABS */
        } else if (type == 1 && opcode == 2) {
            double d;
            memcpy(&d, &src, 8);
            d = -d;
            uint64_t u;
            memcpy(&u, &d, 8);
            scribe_fp_scalar(mac, rd, 1, u);  /* FNEG */
        } else if (type == 0 && opcode == 1) {
            float f;
            memcpy(&f, &src, 4);
            f = f < 0 ? -f : f;
            uint32_t u;
            memcpy(&u, &f, 4);
            scribe_fp_scalar(mac, rd, 0, u);
        } else if (type == 0 && opcode == 2) {
            float f;
            memcpy(&f, &src, 4);
            f = -f;
            uint32_t u;
            memcpy(&u, &f, 4);
            scribe_fp_scalar(mac, rd, 0, u);
        } else {
            culpa("FP-1src opcode %u type %u non sustinetur (0x%08x)", opcode, type, ins);
        }
        mac->pc += 4;
        return;
    }

    /* FP data-processing (2 source): 00011110:type:1:Rm:opcode(4):10:Rn:Rd */
    if ((ins & 0xff200c00u) == 0x1e200800u) {
        unsigned type   = extr(ins, 23, 22);
        unsigned rm     = extr(ins, 20, 16);
        unsigned opcode = extr(ins, 15, 12);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        uint64_t an     = lege_fp_scalar(mac, rn, type);
        uint64_t am     = lege_fp_scalar(mac, rm, type);
        if (type == 1) {
            double a, b, r;
            memcpy(&a, &an, 8);
            memcpy(&b, &am, 8);
            switch (opcode) {
            case 0:
                r = a * b;
                break;
                /* FMUL */
            case 1:
                r = a / b;
                break;
                /* FDIV */
            case 2:
                r = a + b;
                break;
                /* FADD */
            case 3:
                r = a - b;
                break;
                /* FSUB */
            default:
                culpa("FP-2src opcode %u non sustinetur (0x%08x)", opcode, ins);
                return;
            }
            uint64_t u;
            memcpy(&u, &r, 8);
            scribe_fp_scalar(mac, rd, 1, u);
        } else if (type == 0) {
            float a, b, r;
            memcpy(&a, &an, 4);
            memcpy(&b, &am, 4);
            switch (opcode) {
            case 0:
                r = a * b;
                break;
            case 1:
                r = a / b;
                break;
            case 2:
                r = a + b;
                break;
            case 3:
                r = a - b;
                break;
            default:
                culpa("FP-2src opcode %u non sustinetur (0x%08x)", opcode, ins);
                return;
            }
            uint32_t u;
            memcpy(&u, &r, 4);
            scribe_fp_scalar(mac, rd, 0, u);
        } else {
            culpa("FP-2src type %u non sustinetur", type);
        }
        mac->pc += 4;
        return;
    }

    /* FP compare: 00011110:type:1:Rm:001000:Rn:opcode2(5).
     * 15:14 = 00, 13:10 = 1000. opcode2 pattern: 00000 FCMP, 01000 FCMP zero. */
    if ((ins & 0xff20fc07u) == 0x1e202000u) {
        unsigned type = extr(ins, 23, 22);
        unsigned rm   = extr(ins, 20, 16);
        unsigned rn   = extr(ins, 9, 5);
        unsigned op2  = extr(ins, 4, 0);
        uint64_t an   = lege_fp_scalar(mac, rn, type);
        uint64_t am   = (op2 & 0x8) ? 0 : lege_fp_scalar(mac, rm, type);
        int lt, eq, unord;
        if (type == 1) {
            double a, b;
            memcpy(&a, &an, 8);
            memcpy(&b, &am, 8);
            unord = (a != a) || (b != b);
            lt    = !unord && (a < b);
            eq    = !unord && (a == b);
        } else if (type == 0) {
            float a, b;
            memcpy(&a, &an, 4);
            memcpy(&b, &am, 4);
            unord = (a != a) || (b != b);
            lt    = !unord && (a < b);
            eq    = !unord && (a == b);
        } else {
            culpa("FCMP type %u", type);
            return;
        }
        uint32_t nzcv;
        if (unord)
            nzcv = 0x3 << 28;          /* NZCV = 0011 */
        else if (lt)
            nzcv = 0x8 << 28;          /*       = 1000 */
        else if (eq)
            nzcv = 0x6 << 28;          /*       = 0110 */
        else
            nzcv = 0x2 << 28;          /*       = 0010 (GT) */
        mac->nzcv = nzcv;
        mac->pc += 4;
        return;
    }

    /* SCVTF/UCVTF (scalar, integer -> FP): sf:0:S:11110:type:1:rmode:opcode:000000:Rn:Rd
     * opcode 010=SCVTF, 011=UCVTF; rmode 00. */
    if ((ins & 0x7f3ffc00u) == 0x1e220000u || (ins & 0x7f3ffc00u) == 0x1e230000u) {
        unsigned sf     = extr(ins, 31, 31);
        unsigned type   = extr(ins, 23, 22);
        unsigned opcode = extr(ins, 18, 16);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        uint64_t src    = sf ? mac->x[rn] : (mac->x[rn] & 0xffffffffu);
        if (type == 1) {
            double d;
            if (opcode == 2)
                d = sf ? (double)(int64_t)src : (double)(int32_t)(uint32_t)src;
            else if (opcode == 3)
                d = sf ? (double)src : (double)(uint32_t)src;
            else {
                culpa("CVTF opcode %u (0x%08x)", opcode, ins);
                return;
            }
            uint64_t u;
            memcpy(&u, &d, 8);
            scribe_fp_scalar(mac, rd, 1, u);
        } else if (type == 0) {
            float f;
            if (opcode == 2)
                f = sf ? (float)(int64_t)src : (float)(int32_t)(uint32_t)src;
            else if (opcode == 3)
                f = sf ? (float)src : (float)(uint32_t)src;
            else {
                culpa("CVTF opcode %u (0x%08x)", opcode, ins);
                return;
            }
            uint32_t u;
            memcpy(&u, &f, 4);
            scribe_fp_scalar(mac, rd, 0, u);
        } else {
            culpa("CVTF type %u (0x%08x)", type, ins);
        }
        mac->pc += 4;
        return;
    }

    /* FCVTZS/FCVTZU (FP -> int, truncate): sf:0:S:11110:type:1:11:000:000000:Rn:Rd
     * rmode=11 opcode=000 FCVTZS; opcode=001 FCVTZU. */
    if ((ins & 0x7f3ffc00u) == 0x1e380000u || (ins & 0x7f3ffc00u) == 0x1e390000u) {
        unsigned sf     = extr(ins, 31, 31);
        unsigned type   = extr(ins, 23, 22);
        unsigned opcode = extr(ins, 18, 16);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        uint64_t src    = lege_fp_scalar(mac, rn, type);
        uint64_t r;
        if (type == 1) {
            double d;
            memcpy(&d, &src, 8);
            if (opcode == 0)
                r = (uint64_t)(int64_t)d;
            else
                r = (uint64_t)d;
        } else if (type == 0) {
            float f;
            memcpy(&f, &src, 4);
            if (opcode == 0)
                r = (uint64_t)(int64_t)f;
            else
                r = (uint64_t)f;
        } else {
            culpa("FCVTZ type %u", type);
            return;
        }
        if (!sf)
            r &= 0xffffffffu;
        mac->x[rd] = r;
        mac->pc += 4;
        return;
    }

    /* FMOV (general, FP<->GPR): sf:0:S:11110:type:1:rmode:opcode:000000:Rn:Rd
     *   rmode=00, opcode=110 => FMOV Wn/Xn, Sn/Dn (FP->GPR)
     *   rmode=00, opcode=111 => FMOV Sn/Dn, Wn/Xn (GPR->FP)
     */
    if ((ins & 0x7f3ffc00u) == 0x1e260000u) {  /* GPR <- FP */
        unsigned sf   = extr(ins, 31, 31);
        unsigned type = extr(ins, 23, 22);
        unsigned rn   = extr(ins, 9, 5);
        unsigned rd   = extr(ins, 4, 0);
        uint64_t src  = lege_fp_scalar(mac, rn, type);
        if (!sf)
            src &= 0xffffffffu;
        mac->x[rd] = src;
        mac->pc += 4;
        return;
    }
    if ((ins & 0x7f3ffc00u) == 0x1e270000u) {  /* FP <- GPR */
        unsigned sf   = extr(ins, 31, 31);
        unsigned type = extr(ins, 23, 22);
        unsigned rn   = extr(ins, 9, 5);
        unsigned rd   = extr(ins, 4, 0);
        uint64_t src  = sf ? mac->x[rn] : (mac->x[rn] & 0xffffffffu);
        scribe_fp_scalar(mac, rd, type, src);
        mac->pc += 4;
        return;
    }

    /* AdvSIMD scalar two-reg misc (FP variants):
     *   01 U 11110 size 10000 opcode(5) 10 Rn Rd
     * Fixa: bits 31:30=01, 28:24=11110, 21:17=10000, 11:10=10. */
    if ((ins & 0xdf3e0c00u) == 0x5e200800u) {
        unsigned U      = extr(ins, 29, 29);
        unsigned size   = extr(ins, 23, 22);
        unsigned opcode = extr(ins, 16, 12);
        unsigned rn     = extr(ins, 9, 5);
        unsigned rd     = extr(ins, 4, 0);
        /* FCVTZS/FCVTZU (opcode 0x1b) et SCVTF/UCVTF (opcode 0x1d):
         * bit 22 = precisio (0 = single, 1 = double). */
        unsigned prec = size & 1;
        uint64_t src  = lege_fp_scalar(mac, rn, prec);
        if (opcode == 0x1b) {
            /* FCVTZS/U scalar FP->int, in reg FP. */
            uint64_t r;
            if (prec == 1) {
                double d;
                memcpy(&d, &src, 8);
                r = U ? (uint64_t)d : (uint64_t)(int64_t)d;
            } else {
                float f;
                memcpy(&f, &src, 4);
                r = U ? (uint64_t)f : (uint64_t)(int64_t)f;
            }
            memset(&mac->v[rd], 0, sizeof(CapsaV));
            if (prec == 1)
                memcpy(&mac->v[rd], &r, 8);
            else {
                uint32_t w = (uint32_t)r;
                memcpy(&mac->v[rd], &w, 4);
            }
        } else if (opcode == 0x1d) {
            /* SCVTF/UCVTF scalar int(in FP reg)->FP. */
            if (prec == 1) {
                double d = U ? (double)src : (double)(int64_t)src;
                uint64_t u;
                memcpy(&u, &d, 8);
                scribe_fp_scalar(mac, rd, 1, u);
            } else {
                uint32_t w = (uint32_t)src;
                float f    = U ? (float)w : (float)(int32_t)w;
                uint32_t u;
                memcpy(&u, &f, 4);
                scribe_fp_scalar(mac, rd, 0, u);
            }
        } else {
            culpa("AdvSIMD scalar 2reg opcode=0x%x non sustinetur (0x%08x)", opcode, ins);
            return;  /* impossibile */
        }
        mac->pc += 4;
        return;
    }

    /* FP conditional select (FCSEL):
     *   00011110:type:1:Rm:cond(4):11:Rn:Rd
     * Masca 0xff200c00, valor 0x1e200c00. */
    if ((ins & 0xff200c00u) == 0x1e200c00u) {
        unsigned type = extr(ins, 23, 22);
        unsigned rm   = extr(ins, 20, 16);
        unsigned cond = extr(ins, 15, 12);
        unsigned rn   = extr(ins, 9, 5);
        unsigned rd   = extr(ins, 4, 0);
        unsigned src  = conditio(mac->nzcv, cond) ? rn : rm;
        uint64_t v    = lege_fp_scalar(mac, src, type);
        scribe_fp_scalar(mac, rd, type, v);
        mac->pc += 4;
        return;
    }

    /* FP data-processing (3 source): FMADD/FMSUB/FNMADD/FNMSUB.
     *   31:24=00011111 (0x1f), 23:22=type, 21=o1, 20:16=Rm, 15=o0,
     *   14:10=Ra, 9:5=Rn, 4:0=Rd. */
    if ((ins & 0xff000000u) == 0x1f000000u) {
        unsigned type = extr(ins, 23, 22);
        unsigned o1   = extr(ins, 21, 21);
        unsigned rm   = extr(ins, 20, 16);
        unsigned o0   = extr(ins, 15, 15);
        unsigned ra   = extr(ins, 14, 10);
        unsigned rn   = extr(ins, 9, 5);
        unsigned rd   = extr(ins, 4, 0);
        uint64_t an   = lege_fp_scalar(mac, rn, type);
        uint64_t am   = lege_fp_scalar(mac, rm, type);
        uint64_t aa   = lege_fp_scalar(mac, ra, type);
        if (type == 1) {
            double n_, m_, a_, r;
            memcpy(&n_, &an, 8);
            memcpy(&m_, &am, 8);
            memcpy(&a_, &aa, 8);
            double prod = n_ * m_;
            if (o1 == 0 && o0 == 0)
                r = a_ + prod;       /* FMADD */
            else if (o1 == 0 && o0 == 1)
                r = a_ - prod;       /* FMSUB */
            else if (o1 == 1 && o0 == 0)
                r = -a_ - prod;      /* FNMADD */
            else
                r = -a_ + prod;      /* FNMSUB */
            uint64_t u;
            memcpy(&u, &r, 8);
            scribe_fp_scalar(mac, rd, 1, u);
        } else if (type == 0) {
            float n_, m_, a_, r;
            memcpy(&n_, &an, 4);
            memcpy(&m_, &am, 4);
            memcpy(&a_, &aa, 4);
            float prod = n_ * m_;
            if (o1 == 0 && o0 == 0)
                r = a_ + prod;
            else if (o1 == 0 && o0 == 1)
                r = a_ - prod;
            else if (o1 == 1 && o0 == 0)
                r = -a_ - prod;
            else
                r = -a_ + prod;
            uint32_t u;
            memcpy(&u, &r, 4);
            scribe_fp_scalar(mac, rd, 0, u);
        } else {
            culpa("FP-3src type %u non sustinetur (0x%08x)", type, ins);
        }
        mac->pc += 4;
        return;
    }

    /* AdvSIMD modified immediate: 0 Q op 0111100000 abc cmode 01 defgh Rd.
     * Bits fixae: 28:19 = 0111100000; 11:10 = 01; bit 31 = 0.
     * Masca = 0x9ff80c00, valor = 0x0f000400. */
    if ((ins & 0x9ff80c00u) == 0x0f000400u) {
        unsigned Q     = extr(ins, 30, 30);
        unsigned op    = extr(ins, 29, 29);
        unsigned abc   = extr(ins, 18, 16);
        unsigned cmode = extr(ins, 15, 12);
        unsigned defgh = extr(ins, 9, 5);
        unsigned rd    = extr(ins, 4, 0);
        unsigned imm8  = (abc << 5) | defgh;
        uint64_t imm64 = 0;
        /* Decodica imm64 secundum cmode/op.  Tantum casus communes
         * tractantur; ceteri culpa. */
        if ((cmode & 0x9) == 0x0) {
            /* 0xx0: 32-bit immediatum shift-able (ORR/MOVI/MVNI ad .4S etc). */
            unsigned sh = ((cmode >> 1) & 3) * 8;
            uint32_t w  = (uint32_t)imm8 << sh;
            imm64       = ((uint64_t)w << 32) | w;
        } else if ((cmode & 0xd) == 0x8) {
            /* 10x0: 16-bit immediatum shift-able. */
            unsigned sh = ((cmode >> 1) & 1) * 8;
            uint16_t h  = (uint16_t)imm8 << sh;
            imm64 = ((uint64_t)h << 48) | ((uint64_t)h << 32)
                | ((uint64_t)h << 16) | h;
        } else if (cmode == 0xe && op == 0) {
            /* MOVI Vd.16B, #imm8 — replica octeti. */
            uint64_t b = imm8;
            imm64 = b | (b<<8) | (b<<16) | (b<<24)
                | (b<<32) | (b<<40) | (b<<48) | (b<<56);
        } else if (cmode == 0xe && op == 1) {
            /* MOVI Dd, #imm64 aut MOVI Vd.2D, #imm64 — imm8 bit-per-byte. */
            for (int i = 0; i < 8; i++) {
                if (imm8 & (1u << i))
                    imm64 |= 0xffull << (i * 8);
            }
        } else if (cmode == 0xf && op == 0) {
            /* FMOV Vd.xS, #imm — 32-bit FP immediatum replicatum. */
            float f = vfp_expand_single(imm8);
            uint32_t u;
            memcpy(&u, &f, 4);
            imm64 = ((uint64_t)u << 32) | u;
        } else if (cmode == 0xf && op == 1 && Q == 1) {
            /* FMOV Vd.2D, #imm — 64-bit FP immediatum. */
            double d = vfp_expand_double(imm8);
            memcpy(&imm64, &d, 8);
        } else {
            culpa(
                "AdvSIMD modimm cmode=%u op=%u non sustinetur (0x%08x)",
                cmode, op, ins
            );
            return;  /* impossibile */
        }
        memset(&mac->v[rd], 0, sizeof(CapsaV));
        mac->v[rd].q.lo = imm64;
        if (Q)
            mac->v[rd].q.hi = imm64;
        mac->pc += 4;
        return;
    }

    culpa("FP/SIMD iussum non sustinetur 0x%08x ad pc=0x%llx", ins, (unsigned long long)mac->pc);
}

/* ======================================================================== *
 * XIV.  Vocationes systematis (Darwin arm64 BSD syscalls).
 *
 * Convention: SVC #0x80, numerus in X16, args X0..X5, result X0, C vexillum
 * positum in errore. Numeri ex <sys/syscall.h> BSD (positivi). Tractamus
 * subset; alioqui culpa.
 * ======================================================================== */

#define D_exit         1
#define D_fork         2
#define D_read         3
#define D_write        4
#define D_open         5
#define D_close        6
#define D_wait4        7
#define D_unlink      10
#define D_getpid      20
#define D_getuid      24
#define D_geteuid     25
#define D_recvfrom    29
#define D_accept      30
#define D_access      33
#define D_kill        37
#define D_getppid     39
#define D_dup         41
#define D_pipe        42
#define D_getegid     43
#define D_sigaction   46
#define D_getgid      47
#define D_sigprocmask 48
#define D_ioctl       54
#define D_readlink    58
#define D_execve      59
#define D_munmap      73
#define D_mprotect    74
#define D_madvise     75
#define D_msync       65
#define D_dup2        90
#define D_fcntl       92
#define D_select      93
#define D_fsync       95
#define D_socket      97
#define D_connect     98
#define D_bind       104
#define D_listen     106
#define D_sendto     133
#define D_socketpair 135
#define D_mkdir      136
#define D_mkfifo     132
#define D_rmdir      137
#define D_gettimeofday 116
#define D_readv      120
#define D_writev     121
#define D_flock      131
#define D_setsockopt 105
#define D_getsockopt 118
#define D_shutdown   134
#define D_setitimer   83
#define D_mmap       197
#define D_lseek      199
#define D_truncate   200
#define D_ftruncate  201
#define D_getrlimit  194
#define D_setrlimit  195
#define D_fstat      339   /* fstat64 */
#define D_stat       338   /* stat64 */
#define D_lstat      340
#define D_pause      0x200 /* numerus ficticius pro signalibus */
#define D_opendir    0x210 /* ficticius */
#define D_readdir    0x211
#define D_closedir   0x212

static uint64_t mmap_cursor = 0;

/* Signa: locum functionis dispositae (VA apud peregrinum) servamus;
 * minimam tantum sustentationem praebemus. Dispositio structurae
 * sigaction apud peregrinum: (void*)sa_handler, long sa_mask, int sa_flags. */
#define SIG_MAX 32
static uint64_t signal_handlers[SIG_MAX];

static long
inv_sigaction(Memoria *mem, int sig, uint64_t act_va, uint64_t oact_va)
{
    if (sig < 0 || sig >= SIG_MAX)
        return -EINVAL;
    if (oact_va) {
        scribe_u64(mem, oact_va, signal_handlers[sig]);
    }
    if (act_va) {
        signal_handlers[sig] = lege_u64(mem, act_va);
    }
    return 0;
}

static long
inv_read(Memoria *mem, int fd, uint64_t buf, size_t n)
{
    uint8_t tmp[4096];
    long tot = 0;
    while (n > 0) {
        size_t chunk = n > sizeof tmp ? sizeof tmp : n;
        ssize_t r    = read(fd, tmp, chunk);
        if (r < 0)
            return -errno;
        if (r == 0)
            break;
        scribe_mem(mem, buf + tot, tmp, (size_t)r);
        tot += r;
        n -= (size_t)r;
        if ((size_t)r < chunk)
            break;
    }
    return tot;
}

static long
inv_write(Memoria *mem, int fd, uint64_t buf, size_t n)
{
    uint8_t tmp[4096];
    long tot = 0;
    while (n > 0) {
        size_t chunk = n > sizeof tmp ? sizeof tmp : n;
        lege_mem(mem, buf + tot, tmp, chunk);
        ssize_t w = write(fd, tmp, chunk);
        if (w < 0)
            return -errno;
        tot += w;
        if ((size_t)w < chunk)
            break;
        n -= (size_t)w;
    }
    return tot;
}

static long
inv_writev(Memoria *mem, int fd, uint64_t iov, int cnt)
{
    long tot = 0;
    for (int i = 0; i < cnt; i++) {
        uint64_t ba = lege_u64(mem, iov + (uint64_t)i * 16);
        uint64_t ln = lege_u64(mem, iov + (uint64_t)i * 16 + 8);
        long r      = inv_write(mem, fd, ba, (size_t)ln);
        if (r < 0)
            return r;
        tot += r;
        if ((uint64_t)r < ln)
            break;
    }
    return tot;
}

static long
inv_readv(Memoria *mem, int fd, uint64_t iov, int cnt)
{
    long tot = 0;
    for (int i = 0; i < cnt; i++) {
        uint64_t ba = lege_u64(mem, iov + (uint64_t)i * 16);
        uint64_t ln = lege_u64(mem, iov + (uint64_t)i * 16 + 8);
        long r      = inv_read(mem, fd, ba, (size_t)ln);
        if (r < 0)
            return r;
        tot += r;
        if ((uint64_t)r < ln)
            break;
    }
    return tot;
}

/* Tabula regionum per mmap mappatarum cum fasciculo subiacente, ut
 * msync et munmap scripturas in fasciculum originis reverti possint. */
#define MMAP_REC_MAG 64
typedef struct {
    uint64_t va;
    uint64_t len;
    int      fd;
    off_t    off;
    int      writable;
    int      shared;
} MmapRec;
static MmapRec mmap_recs[MMAP_REC_MAG];

static void
mmap_rec_adde(uint64_t va, uint64_t len, int fd, off_t off, int writable, int shared)
{
    for (int i = 0; i < MMAP_REC_MAG; i++) {
        if (mmap_recs[i].len == 0) {
            mmap_recs[i].va       = va;
            mmap_recs[i].len      = len;
            mmap_recs[i].fd       = fd;
            mmap_recs[i].off      = off;
            mmap_recs[i].writable = writable;
            mmap_recs[i].shared   = shared;
            return;
        }
    }
}

static MmapRec *
    mmap_rec_quaere(uint64_t va)
{
    for (int i = 0; i < MMAP_REC_MAG; i++) {
        if (mmap_recs[i].len != 0 && mmap_recs[i].va == va)
            return &mmap_recs[i];
    }
    return NULL;
}

static void
mmap_rec_flush(Memoria *mem, MmapRec *r)
{
    if (!r->writable || !r->shared || r->fd < 0)
        return;
    uint8_t buf[4096];
    uint64_t done = 0;
    while (done < r->len) {
        size_t chunk = r->len - done > sizeof buf ? sizeof buf : r->len - done;
        lege_mem(mem, r->va + done, buf, chunk);
        ssize_t w = pwrite(r->fd, buf, chunk, r->off + (off_t)done);
        if (w < 0)
            break;
        done += (uint64_t)w;
        if ((size_t)w < chunk)
            break;
    }
}

static long
inv_mmap(Memoria *mem, uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags, long fd, uint64_t off)
{
    (void)addr;
    int anon   = (flags & 0x1000) != 0;
    int shared = (flags & 1) != 0;
    uint64_t n = (len + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
    if (mmap_cursor + n > mem->limen_superum - PILA_MAG - PAGINA)
        return -ENOMEM;
    uint64_t v = mmap_cursor;
    mmap_cursor += n;
    if (!anon) {
        if (fd < 0)
            return -EBADF;
        uint8_t buf[4096];
        uint64_t done = 0;
        while (done < len) {
            size_t chunk = len - done > sizeof buf ? sizeof buf : len - done;
            ssize_t r    = pread((int)fd, buf, chunk, (off_t)off + (off_t)done);
            if (r <= 0)
                break;
            scribe_mem(mem, v + done, buf, (size_t)r);
            done += (uint64_t)r;
            if ((size_t)r < chunk)
                break;
        }
        int writable = (prot & 2) != 0;
        mmap_rec_adde(v, len, (int)fd, (off_t)off, writable, shared);
    }
    return (long)v;
}

static long
inv_munmap(Memoria *mem, uint64_t addr, uint64_t len)
{
    (void)len;
    MmapRec *r = mmap_rec_quaere(addr);
    if (r) {
        mmap_rec_flush(mem, r);
        r->len = 0;
    }
    return 0;
}

static long
inv_msync(Memoria *mem, uint64_t addr, uint64_t len, int flags)
{
    (void)len;
    (void)flags;
    MmapRec *r = mmap_rec_quaere(addr);
    if (r)
        mmap_rec_flush(mem, r);
    return 0;
}

static long
inv_fstat(Memoria *mem, int fd, uint64_t buf)
{
    struct stat s;
    if (fstat(fd, &s) < 0)
        return -errno;
    /* Dispositio structurae stat64 Darwiniana: dev(4) mode(2) nlink(2) ino(8) uid(4) gid(4)
     * rdev(4) accesstimespec(16) modtimespec(16) ctimespec(16) birthspec(16)
     * size(8) blocks(8) blksize(4) flags(4) gen(4) lspare(4) qspare(16). */
    uint8_t z[144];
    memset(z, 0, sizeof z);
    uint32_t dev = (uint32_t)s.st_dev;
    memcpy(z + 0, &dev, 4);
    uint16_t mod = (uint16_t)s.st_mode;
    memcpy(z + 4, &mod, 2);
    uint16_t nlk = (uint16_t)s.st_nlink;
    memcpy(z + 6, &nlk, 2);
    uint64_t ino = (uint64_t)s.st_ino;
    memcpy(z + 8, &ino, 8);
    uint32_t uid = s.st_uid;
    memcpy(z + 16, &uid, 4);
    uint32_t gid = s.st_gid;
    memcpy(z + 20, &gid, 4);
    int64_t sz = s.st_size;
    memcpy(z + 96, &sz, 8);
    int64_t bkc = s.st_blocks;
    memcpy(z + 104, &bkc, 8);
    int32_t bsz = s.st_blksize;
    memcpy(z + 112, &bsz, 4);
    scribe_mem(mem, buf, z, sizeof z);
    return 0;
}

static long
inv_open(Memoria *mem, uint64_t pva, int flags, int mode)
{
    char path[1024];
    size_t i = 0;
    for (; i < sizeof path - 1; i++) {
        uint8_t c = lege_u8(mem, pva + i);
        path[i]   = (char)c;
        if (c == 0)
            break;
    }
    path[sizeof path - 1] = 0;
    int fd = open(path, flags, mode);
    if (fd < 0)
        return -errno;
    return fd;
}

static long
inv_readlink(Memoria *mem, uint64_t pva, uint64_t buf, uint64_t sz)
{
    char path[1024];
    size_t i = 0;
    for (; i < sizeof path - 1; i++) {
        uint8_t c = lege_u8(mem, pva + i);
        path[i]   = (char)c;
        if (c == 0)
            break;
    }
    path[sizeof path - 1] = 0;
    char dst[1024];
    if (sz > sizeof dst)
        sz = sizeof dst;
    ssize_t r = readlink(path, dst, (size_t)sz);
    if (r < 0)
        return -errno;
    scribe_mem(mem, buf, dst, (size_t)r);
    return r;
}

static long
inv_gettimeofday(Memoria *mem, uint64_t tv, uint64_t tz)
{
    (void)tz;
    struct timeval t;
    if (gettimeofday(&t, NULL) < 0)
        return -errno;
    if (tv) {
        scribe_u64(mem, tv,     (uint64_t)t.tv_sec);
        scribe_u64(mem, tv + 8, (uint64_t)t.tv_usec);
    }
    return 0;
}

/* Transcribit chordam NUL-terminatam ex spatio peregrini in alveum hospitis. */
static int
lege_chorda(Memoria *mem, uint64_t va, char *dst, size_t mag)
{
    for (size_t i = 0; i < mag; i++) {
        uint8_t c = lege_u8(mem, va + i);
        dst[i]    = (char)c;
        if (c == 0)
            return (int)i;
    }
    dst[mag - 1] = 0;
    return -1;
}

/* Scribit structuram stat (forma Darwiniana stat64) in memoriam peregrini
 * ex structura 'struct stat' hospitis. */
static void
scribe_stat64(Memoria *mem, uint64_t buf, const struct stat *s)
{
    uint8_t z[144];
    memset(z, 0, sizeof z);
    uint32_t dev = (uint32_t)s->st_dev;
    memcpy(z + 0, &dev, 4);
    uint16_t mod = (uint16_t)s->st_mode;
    memcpy(z + 4, &mod, 2);
    uint16_t nlk = (uint16_t)s->st_nlink;
    memcpy(z + 6, &nlk, 2);
    uint64_t ino = (uint64_t)s->st_ino;
    memcpy(z + 8, &ino, 8);
    uint32_t uid = s->st_uid;
    memcpy(z + 16, &uid, 4);
    uint32_t gid = s->st_gid;
    memcpy(z + 20, &gid, 4);
    int64_t sz = s->st_size;
    memcpy(z + 96, &sz, 8);
    int64_t bkc = s->st_blocks;
    memcpy(z + 104, &bkc, 8);
    int32_t bsz = s->st_blksize;
    memcpy(z + 112, &bsz, 4);
    scribe_mem(mem, buf, z, sizeof z);
}

static long
inv_stat_path(Memoria *mem, uint64_t pva, uint64_t buf, int follow)
{
    char path[1024];
    if (lege_chorda(mem, pva, path, sizeof path) < 0)
        return -ENAMETOOLONG;
    struct stat s;
    int r = follow ? stat(path, &s) : lstat(path, &s);
    if (r < 0)
        return -errno;
    scribe_stat64(mem, buf, &s);
    return 0;
}

static long
inv_unlink(Memoria *mem, uint64_t pva)
{
    char path[1024];
    if (lege_chorda(mem, pva, path, sizeof path) < 0)
        return -ENAMETOOLONG;
    return unlink(path) < 0 ? -errno : 0;
}

static long
inv_mkdir(Memoria *mem, uint64_t pva, int mode)
{
    char path[1024];
    if (lege_chorda(mem, pva, path, sizeof path) < 0)
        return -ENAMETOOLONG;
    return mkdir(path, (mode_t)mode) < 0 ? -errno : 0;
}

static long
inv_mkfifo(Memoria *mem, uint64_t pva, int mode)
{
    char path[1024];
    if (lege_chorda(mem, pva, path, sizeof path) < 0)
        return -ENAMETOOLONG;
    return mkfifo(path, (mode_t)mode) < 0 ? -errno : 0;
}

static long
inv_pipe(Memoria *mem, uint64_t va)
{
    int fds[2];
    if (pipe(fds) < 0)
        return -errno;
    scribe_u32(mem, va,     (uint32_t)fds[0]);
    scribe_u32(mem, va + 4, (uint32_t)fds[1]);
    return 0;
}

static long
inv_socketpair(Memoria *mem, int dom, int tp, int proto, uint64_t va)
{
    int fds[2];
    if (socketpair(dom, tp, proto, fds) < 0)
        return -errno;
    scribe_u32(mem, va,     (uint32_t)fds[0]);
    scribe_u32(mem, va + 4, (uint32_t)fds[1]);
    return 0;
}

static long
inv_wait4(Memoria *mem, int pid, uint64_t sva, int opts)
{
    int status = 0;
    pid_t r    = waitpid(pid, &status, opts);
    if (r < 0)
        return -errno;
    if (sva)
        scribe_u32(mem, sva, (uint32_t)status);
    return r;
}

static long
inv_execve(Memoria *mem, uint64_t pva, uint64_t argv_va, uint64_t envp_va)
{
    char path[1024];
    if (lege_chorda(mem, pva, path, sizeof path) < 0)
        return -ENAMETOOLONG;
    /* Numera argv, envp. */
    int argc = 0, envc = 0;
    while (lege_u64(mem, argv_va + (uint64_t)argc * 8))
        argc++;
    if (envp_va)
        while (lege_u64(mem, envp_va + (uint64_t)envc * 8))
            envc++;
    char **argv = calloc((size_t)argc + 1, sizeof(char *));
    char **envp = envp_va ? calloc((size_t)envc + 1, sizeof(char *)) : NULL;
    for (int i = 0; i < argc; i++) {
        uint64_t sva = lege_u64(mem, argv_va + (uint64_t)i * 8);
        argv[i]      = malloc(1024);
        if (lege_chorda(mem, sva, argv[i], 1024) < 0) {
            free(argv[i]);
            argv[i] = NULL;
        }
    }
    if (envp_va)
        for (int i = 0; i < envc; i++) {
        uint64_t sva = lege_u64(mem, envp_va + (uint64_t)i * 8);
        envp[i]      = malloc(1024);
        if (lege_chorda(mem, sva, envp[i], 1024) < 0) {
            free(envp[i]);
            envp[i] = NULL;
        }
    }
    /* Si path nomen simplex (nullum '/'), quaerimus in PATH. */
    extern char **environ;
    int r;
    if (strchr(path, '/'))
        r = execve(path, argv, envp ? envp : environ);
    else
        r = execvp(path, argv);
    /* si revertitur, error. */
    int e = errno;
    for (int i = 0; i < argc; i++)
        free(argv[i]);
    free(argv);
    if (envp) {
        for (int i = 0; i < envc; i++)
            free(envp[i]);
        free(envp);
    }
    (void)r;
    return -e;
}

static long
inv_fcntl(Memoria *mem, int fd, int cmd, uint64_t arg)
{
    /* Valores F_* peregrini: 0=DUPFD 1=GETFD 2=SETFD 3=GETFL 4=SETFL
     * 7=GETLK 8=SETLK 9=SETLKW.  Vertimus in valores F_* hospitis. */
    switch (cmd) {
    case 0:
        {
            int r = fcntl(fd, F_DUPFD, (int)arg);
            return r < 0 ? -errno : r;
        }
    case 1:
        {
            int r = fcntl(fd, F_GETFD);
            return r < 0 ? -errno : r;
        }
    case 2:
        {
            int r = fcntl(fd, F_SETFD, (int)arg);
            return r < 0 ? -errno : r;
        }
    case 3:
        {
            int r = fcntl(fd, F_GETFL);
            return r < 0 ? -errno : r;
        }
    case 4:
        {
            int r = fcntl(fd, F_SETFL, (int)arg);
            return r < 0 ? -errno : r;
        }
    case 7: case 8: case 9: {
        /* Dispositio structurae flock apud peregrinum: off_t l_start, l_len;
         * pid_t l_pid; short l_type, l_whence. */
            struct flock fl;
            memset(&fl, 0, sizeof fl);
            fl.l_start = (off_t)lege_u64(mem, arg + 0);
            fl.l_len   = (off_t)lege_u64(mem, arg + 8);
            fl.l_pid   = (pid_t)lege_u32(mem, arg + 16);
            uint16_t t = lege_u16(mem, arg + 20);
            uint16_t w = lege_u16(mem, arg + 22);
        /* vulcanus: F_RDLCK=1 F_UNLCK=2 F_WRLCK=3; Darwin eadem valores. */
            fl.l_type    = (short)t;
            fl.l_whence  = (short)w;
            int host_cmd = (cmd == 7) ? F_GETLK : (cmd == 8) ? F_SETLK : F_SETLKW;
            int r        = fcntl(fd, host_cmd, &fl);
            if (r < 0)
                return -errno;
            if (cmd == 7) {
                scribe_u64(mem, arg + 0, (uint64_t)fl.l_start);
                scribe_u64(mem, arg + 8, (uint64_t)fl.l_len);
                scribe_u32(mem, arg + 16, (uint32_t)fl.l_pid);
                scribe_u16(mem, arg + 20, (uint16_t)fl.l_type);
                scribe_u16(mem, arg + 22, (uint16_t)fl.l_whence);
            }
            return r;
        }
    default:
        return -EINVAL;
    }
}

/* opendir/readdir/closedir: tabulam DIR* indiciumque apud hospitem
 * retinemus, ut peregrinus tantum integrum indicem tractet. */
#define DIR_TAB_MAG 256
static DIR *dir_tab[DIR_TAB_MAG];

static long
inv_opendir(Memoria *mem, uint64_t pva)
{
    char path[1024];
    if (lege_chorda(mem, pva, path, sizeof path) < 0)
        return -ENAMETOOLONG;
    DIR *d = opendir(path);
    if (!d)
        return -errno;
    for (int i = 1; i < DIR_TAB_MAG; i++) {
        if (!dir_tab[i]) {
            dir_tab[i] = d;
            return i;
        }
    }
    closedir(d);
    return -EMFILE;
}

static long
inv_readdir(Memoria *mem, int h, uint64_t ent_va)
{
    if (h <= 0 || h >= DIR_TAB_MAG || !dir_tab[h])
        return -EBADF;
    errno = 0;
    struct dirent *e = readdir(dir_tab[h]);
    if (!e)
        return errno ? -errno : 1;  /* 1 = EOF */
    /* Dispositio structurae dirent apud peregrinum: ino(8) seekoff(8)
     * reclen(2) namlen(2) type(1) name[1024]. */
    uint8_t buf[8 + 8 + 2 + 2 + 1 + 1024];
    memset(buf, 0, sizeof buf);
    uint64_t ino = e->d_ino;
    memcpy(buf + 0, &ino, 8);
    uint64_t off = 0;
    memcpy(buf + 8, &off, 8);
    size_t nl = strlen(e->d_name);
    if (nl > 1023)
        nl = 1023;
    uint16_t reclen = (uint16_t)(21 + nl + 1);
    memcpy(buf + 16, &reclen, 2);
    uint16_t nm = (uint16_t)nl;
    memcpy(buf + 18, &nm, 2);
    buf[20] = e->d_type;
    memcpy(buf + 21, e->d_name, nl);
    buf[21 + nl] = 0;
    scribe_mem(mem, ent_va, buf, sizeof buf);
    return 0;
}

static long
inv_closedir(int h)
{
    if (h <= 0 || h >= DIR_TAB_MAG || !dir_tab[h])
        return -EBADF;
    int r      = closedir(dir_tab[h]);
    dir_tab[h] = NULL;
    return r < 0 ? -errno : 0;
}

/* Vocatio 'select': transfert fd_set ex dispositione peregrini
 * (FD_SETSIZE=1024, fds_bits = uint32_t[32]) in fd_set hospitis. */
static void
copia_fdset(Memoria *mem, uint64_t va, fd_set *out, int nfds)
{
    FD_ZERO(out);
    if (!va)
        return;
    for (int i = 0; i < nfds; i++) {
        uint32_t w = lege_u32(mem, va + (uint64_t)(i / 32) * 4);
        if (w & (1u << (i % 32)))
            FD_SET(i, out);
    }
}

static void
scribe_fdset(Memoria *mem, uint64_t va, const fd_set *in, int nfds)
{
    if (!va)
        return;
    for (int wi = 0; wi < (nfds + 31) / 32; wi++) {
        uint32_t w = 0;
        for (int b = 0; b < 32; b++) {
            int fd = wi * 32 + b;
            if (fd >= nfds)
                break;
            if (FD_ISSET(fd, in))
                w |= 1u << b;
        }
        scribe_u32(mem, va + (uint64_t)wi * 4, w);
    }
}

static long
inv_select(Memoria *mem, int nfds, uint64_t rva, uint64_t wva, uint64_t eva, uint64_t tova)
{
    fd_set rs, ws, es;
    copia_fdset(mem, rva, &rs, nfds);
    copia_fdset(mem, wva, &ws, nfds);
    copia_fdset(mem, eva, &es, nfds);
    struct timeval tv, *ptv = NULL;
    if (tova) {
        tv.tv_sec  = (long)lege_u64(mem, tova + 0);
        tv.tv_usec = (long)lege_u64(mem, tova + 8);
        ptv        = &tv;
    }
    int r = select(nfds, rva ? &rs : NULL, wva ? &ws : NULL, eva ? &es : NULL, ptv);
    if (r < 0)
        return -errno;
    if (rva)
        scribe_fdset(mem, rva, &rs, nfds);
    if (wva)
        scribe_fdset(mem, wva, &ws, nfds);
    if (eva)
        scribe_fdset(mem, eva, &es, nfds);
    return r;
}

/* Socket syscalls: plerique directe pertranseunt. */
static long
inv_sendto(
    Memoria *mem, int fd, uint64_t buf, size_t n, int flags,
    uint64_t addrva, int addrlen
) {
    (void)addrva;
    (void)addrlen;
    uint8_t *tmp = malloc(n);
    lege_mem(mem, buf, tmp, n);
    ssize_t r = send(fd, tmp, n, flags);
    free(tmp);
    return r < 0 ? -errno : r;
}

static long
inv_recvfrom(
    Memoria *mem, int fd, uint64_t buf, size_t n, int flags,
    uint64_t addrva, uint64_t addrlenva
) {
    (void)addrva;
    (void)addrlenva;
    uint8_t *tmp = malloc(n);
    ssize_t r    = recv(fd, tmp, n, flags);
    if (r > 0)
        scribe_mem(mem, buf, tmp, (size_t)r);
    free(tmp);
    return r < 0 ? -errno : r;
}

static void
voca_sys(Machina *mac, Memoria *mem)
{
    uint64_t n = mac->x[16];
    uint64_t a = mac->x[0], b = mac->x[1], c = mac->x[2];
    uint64_t d = mac->x[3], e = mac->x[4], f = mac->x[5];
    nuntio(
        "vocatio #%llu (x0=0x%llx x1=0x%llx x2=0x%llx)",
        (unsigned long long)n, (unsigned long long)a,
        (unsigned long long)b, (unsigned long long)c
    );
    long rv;
    int err = 0;
    switch (n) {
    case D_read:
        rv = inv_read(mem, (int)a, b, (size_t)c);
        break;
    case D_write:
        rv = inv_write(mem, (int)a, b, (size_t)c);
        break;
    case D_open:
        rv = inv_open(mem, a, (int)b, (int)c);
        break;
    case D_close:
        rv = close((int)a);
        if (rv < 0)
            rv = -errno;
        break;
    case D_readv:
        rv = inv_readv(mem, (int)a, b, (int)c);
        break;
    case D_writev:
        rv = inv_writev(mem, (int)a, b, (int)c);
        break;
    case D_lseek:
        rv = (long)lseek((int)a, (off_t)b, (int)c);
        if (rv < 0)
            rv = -errno;
        break;
    case D_fstat:
        rv = inv_fstat(mem, (int)a, b);
        break;
    case D_ioctl:
        (void)d;
        (void)e;
        (void)f;
        rv = -ENOTTY;
        break;
    case D_mmap:
        rv = inv_mmap(mem, a, b, c, d, (long)e, f);
        break;
    case D_munmap:
        rv = inv_munmap(mem, a, b);
        break;
    case D_mprotect:
    case D_madvise:
        rv = 0;
        break;
    case D_msync:
        rv = inv_msync(mem, a, b, (int)c);
        break;
    case D_access:
        rv = -EACCES;
        break;
    case D_readlink:
        rv = inv_readlink(mem, a, b, c);
        break;
    case D_getpid:
        rv = getpid();
        break;
    case D_getuid:
        rv = getuid();
        break;
    case D_geteuid:
        rv = geteuid();
        break;
    case D_getgid:
        rv = getgid();
        break;
    case D_getegid:
        rv = getegid();
        break;
    case D_gettimeofday:
        rv = inv_gettimeofday(mem, a, b);
        break;
    case D_fcntl:
        rv = inv_fcntl(mem, (int)a, (int)b, c);
        break;
    case D_fork: {
            pid_t pid = fork();
            rv        = (pid < 0) ? -errno : (long)pid;
            break;
        }
    case D_wait4:
        rv = inv_wait4(mem, (int)a, b, (int)c);
        break;
    case D_kill:
        rv = kill((pid_t)a, (int)b);
        if (rv < 0)
            rv = -errno;
        break;
    case D_getppid:
        rv = getppid();
        break;
    case D_unlink:
        rv = inv_unlink(mem, a);
        break;
    case D_stat:
        rv = inv_stat_path(mem, a, b, 1);
        break;
    case D_lstat:
        rv = inv_stat_path(mem, a, b, 0);
        break;
    case D_mkdir:
        rv = inv_mkdir(mem, a, (int)b);
        break;
    case D_mkfifo:
        rv = inv_mkfifo(mem, a, (int)b);
        break;
    case D_rmdir: {
            char path[1024];
            if (lege_chorda(mem, a, path, sizeof path) < 0) {
                rv = -ENAMETOOLONG;
                break;
            }
            rv = rmdir(path);
            if (rv < 0)
                rv = -errno;
            break;
        }
    case D_pipe:
        rv = inv_pipe(mem, a);
        break;
    case D_dup:
        rv = dup((int)a);
        if (rv < 0)
            rv = -errno;
        break;
    case D_dup2:
        rv = dup2((int)a, (int)b);
        if (rv < 0)
            rv = -errno;
        break;
    case D_execve:
        rv = inv_execve(mem, a, b, c);
        break;
    case D_truncate: {
            char path[1024];
            if (lege_chorda(mem, a, path, sizeof path) < 0) {
                rv = -ENAMETOOLONG;
                break;
            }
            rv = truncate(path, (off_t)b);
            if (rv < 0)
                rv = -errno;
            break;
        }
    case D_ftruncate:
        rv = ftruncate((int)a, (off_t)b);
        if (rv < 0)
            rv = -errno;
        break;
    case D_flock:
        rv = flock((int)a, (int)b);
        if (rv < 0)
            rv = -errno;
        break;
    case D_socket:
        rv = socket((int)a, (int)b, (int)c);
        if (rv < 0)
            rv = -errno;
        break;
    case D_socketpair:
        rv = inv_socketpair(mem, (int)a, (int)b, (int)c, d);
        break;
    case D_bind: {
            uint8_t tmp[110];
            size_t ln = (size_t)c;
            if (ln > sizeof tmp)
                ln = sizeof tmp;
            lege_mem(mem, b, tmp, ln);
            rv = bind((int)a, (struct sockaddr *)tmp, (socklen_t)c);
            if (rv < 0)
                rv = -errno;
            break;
        }
    case D_connect: {
            uint8_t tmp[110];
            size_t ln = (size_t)c;
            if (ln > sizeof tmp)
                ln = sizeof tmp;
            lege_mem(mem, b, tmp, ln);
            rv = connect((int)a, (struct sockaddr *)tmp, (socklen_t)c);
            if (rv < 0)
                rv = -errno;
            break;
        }
    case D_listen:
        rv = listen((int)a, (int)b);
        if (rv < 0)
            rv = -errno;
        break;
    case D_accept: {
            uint8_t tmp[110];
            socklen_t slen = sizeof tmp;
            rv = accept((int)a, (struct sockaddr *)tmp, &slen);
            if (rv < 0)
                rv = -errno;
            break;
        }
    case D_sendto:
        rv = inv_sendto(mem, (int)a, b, (size_t)c, (int)d, e, (int)f);
        break;
    case D_recvfrom:
        rv = inv_recvfrom(mem, (int)a, b, (size_t)c, (int)d, e, f);
        break;
    case D_shutdown:
        rv = shutdown((int)a, (int)b);
        if (rv < 0)
            rv = -errno;
        break;
    case D_select:
        rv = inv_select(mem, (int)a, b, c, d, e);
        break;
    case D_opendir:
        rv = inv_opendir(mem, a);
        break;
    case D_readdir:
        rv = inv_readdir(mem, (int)a, b);
        break;
    case D_closedir:
        rv = inv_closedir((int)a);
        break;
    case D_sigaction:
        rv = inv_sigaction(mem, (int)a, b, c);
        break;
    case D_sigprocmask:
    case D_setitimer:
        /* setitimer: nos real timer non installamus, sed functionaliter
         * pause delivery instantaneous habemus. */
        rv = 0;
        break;
    case D_pause:
        /* Sine signalibus kernelis, traditionem manu facimus: si
         * functio pro SIGALRM iam disposita est, eam statim invocamus;
         * postquam revertitur, vocatio 'pause' cum EINTR perficit.
         * Modus: ponimus LR ad iussum post svc (mac->pc + 4), pc ad
         * functionem dispositam, x0 ad numerum signi, et pc += 4
         * consuetum omittimus. Postquam functio RETatur, vulcanus ad
         * vocatorem 'pause' revertitur. */
        if (signal_handlers[14 /*SIGALRM*/]) {
            mac->x[30] = mac->pc + 4;
            mac->x[0]  = 14;
            mac->pc    = signal_handlers[14];
            /* Nota: x0 signum continet, quod functio ut primum argumentum
             * accipit. */
            return;  /* pc iam movetur, non +4 */
        }
        rv = -EINTR;
        break;
    case D_exit:
        nuntio("exit %ld", (long)a);
        exit((int)a);
    default:
        culpa(
            "vocatio non permissa: #%llu (pc=0x%llx)",
            (unsigned long long)n, (unsigned long long)mac->pc
        );
        return;  /* impossibile */
    }
    if (rv < 0) {
        err       = 1;
        mac->x[0] = (uint64_t)(-rv);
    } else {
        mac->x[0] = (uint64_t)rv;
    }
    /* Darwin: vexillum C positum in errore. */
    mac->nzcv = (mac->nzcv & ~VC) | (err ? VC : 0);
    mac->pc += 4;
}

/* ======================================================================== *
 * XV.  Compositio pilae initialis.
 *
 * Nos propriam convention statuimus (simplicem, a Linux styli inspiratam):
 *   sp ->  argc                 (8 octeti)
 *          argv[0..argc-1]      (8 octeti quisque)
 *          NULL                 (8 octeti)
 *          envp[0..envc-1]      (8 octeti quisque)
 *          NULL                 (8 octeti)
 *          (strings)
 * vulcanus hanc dispositionem petit. PC initialis vocat _start, qui
 * pilam explorat.
 * ======================================================================== */

static uint64_t
construe_pilam(Memoria *mem, int argc, char **argv, char **envp)
{
    uint64_t sp_top = mem->limen_superum - PAGINA;
    uint64_t sp     = sp_top;

    int envc = 0;
    while (envp[envc])
        envc++;
    uint64_t *argv_va = calloc((size_t)argc + 1, sizeof(uint64_t));
    uint64_t *envp_va = calloc((size_t)envc + 1, sizeof(uint64_t));

    for (int i = argc - 1; i >= 0; i--) {
        size_t len = strlen(argv[i]) + 1;
        sp -= len;
        scribe_mem(mem, sp, argv[i], len);
        argv_va[i] = sp;
    }
    for (int i = envc - 1; i >= 0; i--) {
        size_t len = strlen(envp[i]) + 1;
        sp -= len;
        scribe_mem(mem, sp, envp[i], len);
        envp_va[i] = sp;
    }

    /* Aligna ad 16 octeti. */
    sp &= ~(uint64_t)0xf;
    size_t total = 8 + 8ull*(argc+1) + 8ull*(envc+1);
    if ((sp - total) & 0xf)
        sp -= 8;

    /* envp NULL. */
    sp -= 8;
    scribe_u64(mem, sp, 0);
    for (int i = envc - 1; i >= 0; i--) {
        sp -= 8;
        scribe_u64(mem, sp, envp_va[i]);
    }
    /* argv NULL. */
    sp -= 8;
    scribe_u64(mem, sp, 0);
    for (int i = argc - 1; i >= 0; i--) {
        sp -= 8;
        scribe_u64(mem, sp, argv_va[i]);
    }
    /* argc. */
    sp -= 8;
    scribe_u64(mem, sp, (uint64_t)argc);

    free(argv_va);
    free(envp_va);

    mmap_cursor = ((sp - PILA_MAG) & ~(uint64_t)(PAGINA - 1)) - (4ull << 20);
    if (mmap_cursor < mem->terminus + PAGINA)
        mmap_cursor = mem->terminus + PAGINA;
    return sp;
}

/* ======================================================================== *
 * XVI.  Principalis.
 * ======================================================================== */

static void
usage(void)
{
    fprintf(stderr, "usus: %s [-v] [-vv] PROGRAMMA [argumenta...]\n", imitator_nomen);
    exit(1);
}

extern char **environ;

int
main(int argc, char **argv)
{
    int i = 1;
    while (i < argc && argv[i][0] == '-') {
        if (strcmp(argv[i], "-v") == 0) {
            verbositas = 1;
            i++;
        } else if (strcmp(argv[i], "-vv") == 0) {
            verbositas = 2;
            i++;
        } else if (strcmp(argv[i], "--") == 0) {
            i++;
            break;
        } else
            usage();
    }
    if (i >= argc)
        usage();

    Memoria mem = {0};
    int habet_main = 0;
    onera_macho(argv[i], &mem, &habet_main);

    Machina mac = mm_init();
    mac.pc      = mem.entry;
    mac.sp      = construe_pilam(&mem, argc - i, argv + i, environ);
    nuntio("entry=0x%llx sp=0x%llx", (unsigned long long)mac.pc, (unsigned long long)mac.sp);

    exsequi(&mac, &mem);
    return 0;
}
