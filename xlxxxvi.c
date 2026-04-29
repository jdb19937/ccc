/*
 * xlxxxvi.c — imitator machinae x86_64 pro binariis ELF Linucis.
 *
 * Nomenclatura:
 *   machina    — status processoris (capsae, vexilla, RIP).
 *   capsa      — cella registri (64 bitorum pro universo usu, 128 pro XMM).
 *   pagina     — unitas memoriae virtualis (4 KiB).
 *   iussum     — singulum mandatum machinale.
 *   resolvere  — mandatum ex octetis legere et interpretari.
 *   exsequi    — mandatum iam resolutum implere.
 *   vocatio    — vocatio systematis (syscall).
 *   culpa      — error fatalis: imitator cum nuntio desinit.
 *
 * Principium fundamentale: si iussum aut vocatio non cognoscitur, numquam
 * silentio progredimur; semper cum culpa desinimus ne executio invalida fiat.
 */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE
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
// #include <sys/uio.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>

/* ======================================================================== *
 * 0.  Arithmetica 128 bitorum (ne ab __int128 pendeamus).
 *     Multiplicatio 64x64 -> 128 et divisio 128/64 manu scriptae.
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

/* Productum signatum capit in 64 bitis? (i.e. extensio signi congruit) */
static int imul_capit_64(int64_t hi, uint64_t lo)
{
    return hi == ((int64_t)lo >> 63);
}

/* Divisio non signata 128/64 -> 64 quot, 64 rem.
 * Si quotiens 64 bita superat, reddit 0; alioquin 1. */
static int udiv_128_64(
    uint64_t nhi, uint64_t nlo, uint64_t d,
    uint64_t *quot, uint64_t *rem
) {
    if (nhi >= d)
        return 0;
    uint64_t r = nhi, q = 0;
    for (int i = 63; i >= 0; i--) {
        int rtop = (int)(r >> 63);
        r        = (r << 1) | ((nlo >> i) & 1);
        q <<= 1;
        if (rtop || r >= d) {
            r -= d;
            q |= 1;
        }
    }
    *quot = q;
    *rem  = r;
    return 1;
}

/* Divisio signata 128/64 -> 64 quot, 64 rem. Reddit 0 si debordat. */
static int sdiv_128_64(
    int64_t nhi, uint64_t nlo, int64_t d,
    int64_t *quot, int64_t *rem
) {
    int neg_n    = nhi < 0;
    int neg_d    = d < 0;
    uint64_t uhi = (uint64_t)nhi, ulo = nlo;
    if (neg_n) {
        ulo = ~ulo + 1;
        uhi = ~uhi + (ulo == 0 ? 1 : 0);
    }
    uint64_t ud = neg_d ? (uint64_t)-d : (uint64_t)d;
    uint64_t uq, ur;
    if (!udiv_128_64(uhi, ulo, ud, &uq, &ur))
        return 0;
    /* quotiens signatus ne (2^63) superet (nisi -2^63 cum signo neg) */
    int q_neg = neg_n ^ neg_d;
    if (q_neg) {
        if (uq > (uint64_t)INT64_MAX + 1)
            return 0;
        *quot = -(int64_t)uq;
    } else {
        if (uq > (uint64_t)INT64_MAX)
            return 0;
        *quot = (int64_t)uq;
    }
    *rem = neg_n ? -(int64_t)ur : (int64_t)ur;
    return 1;
}

/* ======================================================================== *
 * I.  Structurae ELF (definitae manu, ne ab <elf.h> pendeamus).
 * ======================================================================== */

typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machina;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf64Caput;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64Segmentum;

#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_INTERP   3
#define PT_NOTE     4
#define PT_PHDR     6
#define PT_TLS      7
#define PT_GNU_STACK 0x6474e551
#define PT_GNU_RELRO 0x6474e552
#define PT_GNU_PROPERTY 0x6474e553

#define PF_X 1
#define PF_W 2
#define PF_R 4

#define EI_CLASS 4
#define EI_DATA  5
#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define ET_EXEC  2
#define ET_DYN   3
#define EM_X86_64 62

/* Auxiliarii vectores ad processum proferendos. */
#define AT_NULL 0
#define AT_PHDR 3
#define AT_PHENT 4
#define AT_PHNUM 5
#define AT_PAGESZ 6
#define AT_BASE 7
#define AT_ENTRY 9
#define AT_UID  11
#define AT_EUID 12
#define AT_GID  13
#define AT_EGID 14
#define AT_RANDOM 25
#define AT_SECURE 23

/* ======================================================================== *
 * II.  Culpa et diagnostica.
 * ======================================================================== */

static const char *imitator_nomen = "xlxxxvi";
static int verbositas = 0;  /* 0 = silens, 1 = nuntii, 2 = singula iussa. */

static void /* __attribute__((noreturn, format(printf, 1, 2))) */
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

static void /* __attribute__((format(printf, 1, 2))) */
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
 * Usus: imitator unum spatium continuum virtuale accipit, a BASIS ad BASIS +
 * SPATII_MAG, per mmap anonymum. Executabilia non ipsiusmet positionalia
 * ('ET_EXEC') eo loco ponuntur ubi ELF eos vult; positionalia ('ET_DYN')
 * ad basem electam referuntur.
 * ======================================================================== */

#define PAGINA      4096u
#define SPATII_MAG  (1ull << 34)   /* XVI GiB — satis largum. */
#define PILA_MAG    (8ull << 20)   /* VIII MiB pilae. */

typedef struct {
    uint8_t *caro;           /* locus reipsa mapped (hospite). */
    uint64_t basis;          /* virtualis basis exsequendi. */
    uint64_t terminus;       /* fine congerei (pro brk). */
    uint64_t limen_superum;  /* maximum addressable (basis + SPATII_MAG). */
    uint64_t entry;          /* RIP initialis. */
    uint64_t phdr_va;        /* locus tabulae phdr in spatio virtuali. */
    uint16_t phdr_num;
    uint16_t phdr_ent;
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
        culpa("accessus memoriae extra spatium: 0x%016lx", (unsigned long)va);
    return m->caro + (va - m->basis);
}

static void
lege_mem(Memoria *m, uint64_t va, void *dst, size_t n)
{
    if (!valet_locus(m, va, n))
        culpa("legere extra spatium: [0x%016lx, +%zu)", (unsigned long)va, n);
    memcpy(dst, hosp(m, va), n);
}

static void
scribe_mem(Memoria *m, uint64_t va, const void *src, size_t n)
{
    if (!valet_locus(m, va, n))
        culpa("scribere extra spatium: [0x%016lx, +%zu)", (unsigned long)va, n);
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
 * IV.  Loader ELF.
 *
 * Lectorem binarium accipimus, validamus, segmenta PT_LOAD in memoriam
 * virtualem exponimus. Non sustinentur: dynamic linking (DT_NEEDED), TLS
 * multiplex, ELF relocations ad tempus exsequendi (pro PIE, offsetum
 * simpliciter applicamus: ET_DYN binarium ad basem 0x400000 ponimus ut
 * ET_EXEC).
 * ======================================================================== */

static void
onera_elf(const char *iter, Memoria *m)
{
    FILE *fp = fopen(iter, "rb");
    if (!fp)
        culpa("aperire '%s': %s", iter, strerror(errno));
    fseek(fp, 0, SEEK_END);
    long magnitudo = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (magnitudo < (long)sizeof(Elf64Caput))
        culpa("'%s' nimis parvum ad ELF validum", iter);
    uint8_t *tota = (uint8_t *)malloc((size_t)magnitudo);
    if (!tota)
        culpa("memoriam pro '%s' dare non possum", iter);
    if (fread(tota, 1, (size_t)magnitudo, fp) != (size_t)magnitudo)
        culpa("'%s' legi non potuit", iter);
    fclose(fp);

    Elf64Caput caput;
    memcpy(&caput, tota, sizeof caput);
    if (
        caput.e_ident[0] != 0x7f || caput.e_ident[1] != 'E'
        || caput.e_ident[2] != 'L' || caput.e_ident[3] != 'F'
    )
        culpa("'%s' signaturam ELF non habet", iter);
    if (caput.e_ident[EI_CLASS] != ELFCLASS64)
        culpa("tantum ELF64 sustinetur");
    if (caput.e_ident[EI_DATA] != ELFDATA2LSB)
        culpa("tantum endianismus parvus sustinetur");
    if (caput.e_machina != EM_X86_64)
        culpa("machina non est x86_64 (codex %u)", caput.e_machina);
    if (caput.e_type != ET_EXEC && caput.e_type != ET_DYN)
        culpa("tantum ET_EXEC vel ET_DYN sustinentur");

    /* Basis virtualis. Pro ET_EXEC, p_vaddr absolutus est. Pro ET_DYN
     * (PIE) summas ad basem 0x400000. */
    uint64_t offset_pie = (caput.e_type == ET_DYN) ? 0x400000ull : 0;

    /* Inveni minimum et maximum segmentorum ad spatium metiendum. */
    uint64_t va_min = UINT64_MAX, va_max = 0;
    for (int i = 0; i < caput.e_phnum; i++) {
        Elf64Segmentum s;
        memcpy(&s, tota + caput.e_phoff + (uint64_t)i * caput.e_phentsize, sizeof s);
        if (s.p_type != PT_LOAD)
            continue;
        uint64_t va = s.p_vaddr + offset_pie;
        if (va < va_min)
            va_min = va;
        if (va + s.p_memsz > va_max)
            va_max = va + s.p_memsz;
    }
    if (va_min == UINT64_MAX)
        culpa("nullum segmentum PT_LOAD inventum");

    /* Basis paginae rotundatae. */
    uint64_t basis = va_min & ~(uint64_t)(PAGINA - 1);

    /* Spatium totum allocamus: a basi usque ad BASI + SPATII_MAG. */
    void *regio = mmap(
        (void *)basis, SPATII_MAG,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
        -1, 0
    );
    if (regio == MAP_FAILED || (uint64_t)regio != basis) {
        /* Si MAP_FIXED reicitur (e.g. macOS basem infra minimum vetat),
         * accipimus locum ab OS electum. Basis virtualis hospitis manet
         * invariata; accessus fit per m->caro + (va - m->basis). */
        if (regio != MAP_FAILED)
            munmap(regio, SPATII_MAG);
        regio = mmap(
            NULL, SPATII_MAG, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0
        );
        if (regio == MAP_FAILED)
            culpa("spatium virtuale allocari non potuit: %s", strerror(errno));
        nuntio(
            "basis hospitis 0x%lx non adepta; utimur %p (VA guest manet)",
            (unsigned long)basis, regio
        );
    }

    m->caro = (uint8_t *)regio;
    m->basis = basis;
    m->limen_superum = basis + SPATII_MAG;
    m->entry = caput.e_entry + offset_pie;
    m->terminus = (va_max + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
    m->phdr_va = 0;
    m->phdr_num = caput.e_phnum;
    m->phdr_ent = caput.e_phentsize;

    /* Copia segmenta. */
    for (int i = 0; i < caput.e_phnum; i++) {
        Elf64Segmentum s;
        memcpy(&s, tota + caput.e_phoff + (uint64_t)i * caput.e_phentsize, sizeof s);
        if (s.p_type == PT_INTERP)
            culpa("binarium dynamice connexum; tantum staticum sustinetur");
        if (s.p_type == PT_DYNAMIC)
            culpa("segmentum PT_DYNAMIC praesens; tantum staticum sustinetur");
        if (s.p_type != PT_LOAD)
            continue;
        uint64_t va = s.p_vaddr + offset_pie;
        if (s.p_filesz > s.p_memsz)
            culpa("segmentum: filesz > memsz");
        scribe_mem(m, va, tota + s.p_offset, s.p_filesz);
        /* BSS: memoria iam zero ex mmap anonymo. */
        /* Phdr locus, si in hoc segmento. */
        if (
            caput.e_phoff >= s.p_offset
            && caput.e_phoff < s.p_offset + s.p_filesz
        ) {
            m->phdr_va = va + (caput.e_phoff - s.p_offset);
        }
        nuntio(
            "segmentum mapped: va=0x%lx mag=%lu flags=%s%s%s",
            (unsigned long)va, (unsigned long)s.p_memsz,
            (s.p_flags & PF_R) ? "R" : "-",
            (s.p_flags & PF_W) ? "W" : "-",
            (s.p_flags & PF_X) ? "X" : "-"
        );
    }

    free(tota);
}

/* ======================================================================== *
 * V.  Status processoris.
 * ======================================================================== */

enum { CRAX = 0, CRCX, CRDX, CRBX, CRSP, CRBP, CRSI, CRDI,
    CR8, CR9, CR10, CR11, CR12, CR13, CR14, CR15 };

/* Vexilla in RFLAGS (locus bitorum). */
#define VCF  (1u<<0)
#define VPF  (1u<<2)
#define VAF  (1u<<4)
#define VZF  (1u<<6)
#define VSF  (1u<<7)
#define VDF  (1u<<10)
#define VOF  (1u<<11)

typedef union {
    struct {
        uint64_t lo, hi;
    } q;
    uint8_t  b[16];
    uint16_t w[8];
    uint32_t d[4];
    uint64_t o[2];
} CapsaXmm;

typedef struct {
    uint64_t cap[16];         /* rax..r15. */
    uint64_t rip;
    uint32_t vexilla;         /* RFLAGS: tantum bita supra notata valent. */
    CapsaXmm xmm[16];
    uint64_t fs_base;
    uint64_t gs_base;
    uint32_t mxcsr;           /* Non usitatum, sed servatum. */
} Machina;

static Machina mm_init(void)
{
    Machina x;
    memset(&x, 0, sizeof x);
    x.vexilla = 0x2;  /* Bitum reservatum semper unum. */
    x.mxcsr   = 0x1f80;
    return x;
}

/* ======================================================================== *
 * VI.  Paritatis computatio (pro VPF).
 * ======================================================================== */

static uint32_t
paritas(uint64_t v)
{
    uint8_t b = (uint8_t)v;
    b ^= b >> 4;
    b ^= b >> 2;
    b ^= b >> 1;
    return (b & 1) ? 0 : VPF;   /* Paritas = unum si numerus bitorum par. */
}

/* ======================================================================== *
 * VII.  Accessores capsarum variis magnitudinibus.
 *
 * Nota: scribere ad cap32 zero-extendit totum 64-bit capsam; cap16/cap8
 * partes superiores conservant. Pro cap8, si REX praesens, 4..7 significat
 * SPL/BPL/SIL/DIL; sine REX, 4..7 significat AH/CH/DH/BH.
 * ======================================================================== */

static uint64_t
cap_lege64(const Machina *m, unsigned i)
{
    return m->cap[i & 0xf];
}

static uint32_t
cap_lege32(const Machina *m, unsigned i)
{
    return (uint32_t)m->cap[i & 0xf];
}

static uint16_t
cap_lege16(const Machina *m, unsigned i)
{
    return (uint16_t)m->cap[i & 0xf];
}

static uint8_t
cap_lege8(const Machina *m, unsigned i, int habet_rex)
{
    if (!habet_rex && i >= 4 && i < 8)
        return (uint8_t)(m->cap[i - 4] >> 8);   /* AH..BH */
    return (uint8_t)m->cap[i & 0xf];
}

static void
cap_scribe64(Machina *m, unsigned i, uint64_t v)
{
    m->cap[i & 0xf] = v;
}

static void
cap_scribe32(Machina *m, unsigned i, uint32_t v)
{
    /* Scribendo 32-bit, zero-extenditur. */
    m->cap[i & 0xf] = (uint64_t)v;
}

static void
cap_scribe16(Machina *m, unsigned i, uint16_t v)
{
    m->cap[i & 0xf] = (m->cap[i & 0xf] & ~(uint64_t)0xffffull) | v;
}

static void
cap_scribe8(Machina *m, unsigned i, uint8_t v, int habet_rex)
{
    if (!habet_rex && i >= 4 && i < 8) {
        unsigned j = i - 4;
        m->cap[j]  = (m->cap[j] & ~(uint64_t)0xff00ull) | ((uint64_t)v << 8);
    } else {
        m->cap[i & 0xf] = (m->cap[i & 0xf] & ~(uint64_t)0xffull) | v;
    }
}

/* ======================================================================== *
 * VIII.  Decoder praenuntiorum / ModR/M / SIB.
 *
 * Omnis iussum resolvitur ad struct 'Resolutio' quae continet:
 *   — praenuntia (0x66, 0x67, REP, REPNE, LOCK, SEG)
 *   — REX (W, R, X, B)
 *   — octetos opcode (unum aut duos)
 *   — ModR/M decompositum (modus, reg, rm)
 *   — SIB decompositum (scala, index, basis)
 *   — displacement (octo, 32 bit signatum)
 *   — immediatum (magnitudine variabili, signatum)
 *   — longitudo iussi totalis.
 * ======================================================================== */

typedef struct {
    /* Praenuntia. */
    uint8_t  p66;       /* operand-size override. */
    uint8_t  p67;       /* address-size override. */
    uint8_t  rep;       /* 0xF3 rep / repe. */
    uint8_t  repne;     /* 0xF2 repne. */
    uint8_t  lock;
    uint8_t  seg;       /* 0x2E 0x36 0x3E 0x26 0x64 0x65 — raro usus. */
    /* REX. */
    uint8_t  rex;       /* 0 si non praesens; alioqui integrum octetum. */
    uint8_t  rex_w, rex_r, rex_x, rex_b;
    /* Opcode. */
    uint8_t  op_0f;     /* 1 si 0F escape; 0 alioqui. */
    uint8_t  op_0f38;   /* 1 si 0F 38 escape. */
    uint8_t  op_0f3a;   /* 1 si 0F 3A escape. */
    uint8_t  opcode;    /* octetum opcode finale. */
    /* ModR/M + SIB. */
    uint8_t  habet_modrm;
    uint8_t  mod, reg, rm;
    uint8_t  habet_sib;
    uint8_t  scala, index, basis;
    /* Displacement / immediate. */
    int8_t   habet_disp;         /* 0, 1, 4 byte. */
    int64_t  disp;
    int8_t   habet_imm;
    int64_t  imm;
    int8_t   imm_mag;            /* magnitudo immediatae in octetis. */
    /* Longitudo. */
    uint8_t  longitudo;
} Resolutio;

/* Praenuntia legit et REX si adest. */
static void
resolve_praenuntia(Memoria *m, uint64_t *rip, Resolutio *r)
{
    for (;;) {
        uint8_t o = lege_u8(m, *rip);
        switch (o) {
        case 0x66:
            r->p66 = 1;
            (*rip)++;
            continue;
        case 0x67:
            r->p67 = 1;
            (*rip)++;
            continue;
        case 0xF0:
            r->lock = 1;
            (*rip)++;
            continue;
        case 0xF2:
            r->repne = 1;
            (*rip)++;
            continue;
        case 0xF3:
            r->rep = 1;
            (*rip)++;
            continue;
        case 0x2E: case 0x36: case 0x3E: case 0x26:
        case 0x64:
        case 0x65:
            r->seg = o;
            (*rip)++;
            continue;
        default:
            /* REX? (40..4F) */
            if (o >= 0x40 && o <= 0x4F) {
                r->rex   = o;
                r->rex_w = (o >> 3) & 1;
                r->rex_r = (o >> 2) & 1;
                r->rex_x = (o >> 1) & 1;
                r->rex_b = (o >> 0) & 1;
                (*rip)++;
                continue;
            }
            return;
        }
    }
}

/* ModR/M, SIB, displacement legit. */
static void
resolve_modrm(Memoria *m, uint64_t *rip, Resolutio *r)
{
    r->habet_modrm = 1;
    uint8_t mr     = lege_u8(m, *rip);
    (*rip)++;
    r->mod = (mr >> 6) & 3;
    r->reg = (mr >> 3) & 7;
    r->rm  = mr & 7;

    /* SIB (32-bit addressing: rm == 4, mod != 3). In 64-bit longus idem. */
    if (r->mod != 3 && r->rm == 4) {
        uint8_t sb = lege_u8(m, *rip);
        (*rip)++;
        r->habet_sib = 1;
        r->scala     = (sb >> 6) & 3;
        r->index     = (sb >> 3) & 7;
        r->basis     = sb & 7;
    }

    /* Displacement. */
    if (r->mod == 1) {
        int8_t d = (int8_t)lege_u8(m, *rip);
        (*rip)++;
        r->habet_disp = 1;
        r->disp       = d;
    } else if (r->mod == 2) {
        int32_t d = (int32_t)lege_u32(m, *rip);
        (*rip) += 4;
        r->habet_disp = 4;
        r->disp       = d;
    } else if (r->mod == 0) {
        /* rm == 5 (RIP-relative) aut SIB cum basis == 5 et mod == 0. */
        if (r->rm == 5 || (r->habet_sib && r->basis == 5)) {
            int32_t d = (int32_t)lege_u32(m, *rip);
            (*rip) += 4;
            r->habet_disp = 4;
            r->disp       = d;
        }
    }
}

/* ======================================================================== *
 * IX.  Effectivus locus (ME) pro ModR/M memoriam tangente.
 * ======================================================================== */

static uint64_t
locus_efficax(const Machina *mac, const Resolutio *r, uint64_t rip_post)
{
    if (r->mod == 3)
        culpa("locus_efficax vocatus cum mod==3");
    uint64_t la = 0;

    if (r->habet_sib) {
        /* basis */
        unsigned b = r->basis | (r->rex_b << 3);
        if (r->mod == 0 && r->basis == 5) {
            /* nulla basis — tantum displ + (scaled index). */
        } else {
            la += mac->cap[b];
        }
        /* index */
        unsigned idx = r->index | (r->rex_x << 3);
        if (idx != 4) {  /* rsp non potest esse index. */
            la += mac->cap[idx] << r->scala;
        }
        la += (int64_t)r->disp;
    } else {
        if (r->mod == 0 && r->rm == 5) {
            /* RIP-relative. */
            la = rip_post + (int64_t)r->disp;
        } else {
            unsigned b = r->rm | (r->rex_b << 3);
            la         = mac->cap[b] + (int64_t)r->disp;
        }
    }

    /* Segmenta FS/GS: addimus basim. */
    if (r->seg == 0x64)
        la += mac->fs_base;
    else if (r->seg == 0x65)
        la += mac->gs_base;
    return la;
}

/* ======================================================================== *
 * X.  Accessus ad R/M operandum (lege/scribe, magnitudinum variabilium).
 * ======================================================================== */

static uint64_t
lege_rm(Machina *mac, Memoria *mem, const Resolutio *r, unsigned mag)
{
    if (r->mod == 3) {
        unsigned idx = r->rm | (r->rex_b << 3);
        switch (mag) {
        case 1:
            return cap_lege8(mac, idx, r->rex != 0);
        case 2:
            return cap_lege16(mac, idx);
        case 4:
            return cap_lege32(mac, idx);
        case 8:
            return cap_lege64(mac, idx);
        default:
            culpa("lege_rm mag=%u", mag);
        }
    }
    uint64_t la = locus_efficax(mac, r, mac->rip);
    switch (mag) {
    case 1:
        return lege_u8 (mem, la);
    case 2:
        return lege_u16(mem, la);
    case 4:
        return lege_u32(mem, la);
    case 8:
        return lege_u64(mem, la);
    default:
        culpa("lege_rm mag=%u", mag);
        return 0; // impossibile
    }
}

static void
scribe_rm(Machina *mac, Memoria *mem, const Resolutio *r, unsigned mag, uint64_t v)
{
    if (r->mod == 3) {
        unsigned idx = r->rm | (r->rex_b << 3);
        switch (mag) {
        case 1:
            cap_scribe8 (mac, idx, (uint8_t)v, r->rex != 0);
            return;
        case 2:
            cap_scribe16(mac, idx, (uint16_t)v);
            return;
        case 4:
            cap_scribe32(mac, idx, (uint32_t)v);
            return;
        case 8:
            cap_scribe64(mac, idx, v);
            return;
        default:
            culpa("scribe_rm mag=%u", mag);
        }
    }
    uint64_t la = locus_efficax(mac, r, mac->rip);
    switch (mag) {
    case 1:
        scribe_u8 (mem, la, (uint8_t)v);
        return;
    case 2:
        scribe_u16(mem, la, (uint16_t)v);
        return;
    case 4:
        scribe_u32(mem, la, (uint32_t)v);
        return;
    case 8:
        scribe_u64(mem, la, v);
        return;
    default:
        culpa("scribe_rm mag=%u", mag);
    }
}

static uint64_t
lege_reg_mag(const Machina *mac, const Resolutio *r, unsigned mag)
{
    unsigned idx = r->reg | (r->rex_r << 3);
    switch (mag) {
    case 1:
        return cap_lege8 (mac, idx, r->rex != 0);
    case 2:
        return cap_lege16(mac, idx);
    case 4:
        return cap_lege32(mac, idx);
    case 8:
        return cap_lege64(mac, idx);
    default:
        culpa("lege_reg mag=%u", mag);
        return 0; // impossibile
    }
}

static void
scribe_reg_mag(Machina *mac, const Resolutio *r, unsigned mag, uint64_t v)
{
    unsigned idx = r->reg | (r->rex_r << 3);
    switch (mag) {
    case 1:
        cap_scribe8 (mac, idx, (uint8_t)v, r->rex != 0);
        return;
    case 2:
        cap_scribe16(mac, idx, (uint16_t)v);
        return;
    case 4:
        cap_scribe32(mac, idx, (uint32_t)v);
        return;
    case 8:
        cap_scribe64(mac, idx, v);
        return;
    default:
        culpa("scribe_reg mag=%u", mag);
    }
}

/* Magnitudo operandorum ex REX.W + p66. */
static unsigned
mag_operandi(const Resolutio *r)
{
    if (r->rex_w)
        return 8;
    if (r->p66)
        return 2;
    return 4;
}

/* Signum-extendit secundum magnitudinem ad 64 bit. */
static int64_t
signum_ext(uint64_t v, unsigned mag)
{
    switch (mag) {
    case 1:
        return (int8_t)v;
    case 2:
        return (int16_t)v;
    case 4:
        return (int32_t)v;
    default:
        return (int64_t)v;
    }
}

static uint64_t
masca(unsigned mag)
{
    if (mag >= 8)
        return UINT64_MAX;
    return ((uint64_t)1 << (mag * 8)) - 1;
}

/* ======================================================================== *
 * XI.  Computatio vexillorum pro operationibus arithmeticis et logicis.
 * ======================================================================== */

static void
vex_zsp(Machina *m, uint64_t res, unsigned mag)
{
    uint64_t msk = masca(mag);
    uint64_t r   = res & msk;
    m->vexilla &= ~(VZF | VSF | VPF);
    if (r == 0)
        m->vexilla |= VZF;
    if (r & ((uint64_t)1 << (mag * 8 - 1)))
        m->vexilla |= VSF;
    m->vexilla |= paritas(r);
}

static void
vex_add(Machina *m, uint64_t a, uint64_t b, uint64_t res, unsigned mag)
{
    uint64_t msk  = masca(mag);
    uint64_t sbit = (uint64_t)1 << (mag * 8 - 1);
    m->vexilla &= ~(VCF | VOF | VAF);
    if ((res & msk) < (a & msk))
        m->vexilla |= VCF;
    if ((((a ^ res) & (b ^ res)) & sbit))
        m->vexilla |= VOF;
    if (((a ^ b ^ res) & 0x10))
        m->vexilla |= VAF;
    vex_zsp(m, res, mag);
}

static void
vex_adc(Machina *m, uint64_t a, uint64_t b, uint64_t c, uint64_t res, unsigned mag)
{
    uint64_t msk  = masca(mag);
    uint64_t sbit = (uint64_t)1 << (mag * 8 - 1);
    m->vexilla &= ~(VCF | VOF | VAF);
    if (c ? ((res & msk) <= (a & msk)) : ((res & msk) < (a & msk)))
        m->vexilla |= VCF;
    if ((((a ^ res) & (b ^ res)) & sbit))
        m->vexilla |= VOF;
    if (((a ^ b ^ res) & 0x10))
        m->vexilla |= VAF;
    vex_zsp(m, res, mag);
}

static void
vex_sub(Machina *m, uint64_t a, uint64_t b, uint64_t res, unsigned mag)
{
    uint64_t msk  = masca(mag);
    uint64_t sbit = (uint64_t)1 << (mag * 8 - 1);
    m->vexilla &= ~(VCF | VOF | VAF);
    if ((a & msk) < (b & msk))
        m->vexilla |= VCF;
    if ((((a ^ b) & (a ^ res)) & sbit))
        m->vexilla |= VOF;
    if (((a ^ b ^ res) & 0x10))
        m->vexilla |= VAF;
    vex_zsp(m, res, mag);
}

static void
vex_sbb(Machina *m, uint64_t a, uint64_t b, uint64_t c, uint64_t res, unsigned mag)
{
    uint64_t msk  = masca(mag);
    uint64_t sbit = (uint64_t)1 << (mag * 8 - 1);
    m->vexilla &= ~(VCF | VOF | VAF);
    if (c ? ((a & msk) <= (b & msk)) : ((a & msk) < (b & msk)))
        m->vexilla |= VCF;
    if ((((a ^ b) & (a ^ res)) & sbit))
        m->vexilla |= VOF;
    if (((a ^ b ^ res) & 0x10))
        m->vexilla |= VAF;
    vex_zsp(m, res, mag);
}

static void
vex_logica(Machina *m, uint64_t res, unsigned mag)
{
    /* CF = 0, OF = 0, AF = indefinitum (ponimus 0). */
    m->vexilla &= ~(VCF | VOF | VAF);
    vex_zsp(m, res, mag);
}

/* ======================================================================== *
 * XII.  Conditiones (cc) pro Jcc, CMOVcc, SETcc.
 * ======================================================================== */

static int
conditio(const Machina *m, uint8_t cc)
{
    uint32_t f = m->vexilla;
    int zf     = (f & VZF) != 0;
    int cf     = (f & VCF) != 0;
    int sf     = (f & VSF) != 0;
    int of     = (f & VOF) != 0;
    int pf     = (f & VPF) != 0;
    switch (cc & 0xf) {
    case 0x0:
        return of;
        /* O   */
    case 0x1:
        return !of;
        /* NO  */
    case 0x2:
        return cf;
        /* B/C/NAE */
    case 0x3:
        return !cf;
        /* AE/NB */
    case 0x4:
        return zf;
        /* E/Z */
    case 0x5:
        return !zf;
        /* NE/NZ */
    case 0x6:
        return cf || zf;
        /* BE/NA */
    case 0x7:
        return !cf && !zf;
        /* A/NBE */
    case 0x8:
        return sf;
        /* S   */
    case 0x9:
        return !sf;
        /* NS  */
    case 0xa:
        return pf;
        /* P/PE */
    case 0xb:
        return !pf;
        /* NP/PO */
    case 0xc:
        return sf != of;
        /* L/NGE */
    case 0xd:
        return sf == of;
        /* GE/NL */
    case 0xe:
        return zf || (sf != of);
        /* LE/NG */
    case 0xf:
        return !zf && (sf == of);
        /* G/NLE */
    }
    return 0;
}

/* ======================================================================== *
 * XIII.  Pila (stack) auxilia.
 * ======================================================================== */

static void
trude(Machina *m, Memoria *mem, uint64_t v, unsigned mag)
{
    m->cap[CRSP] -= mag;
    switch (mag) {
    case 2:
        scribe_u16(mem, m->cap[CRSP], (uint16_t)v);
        break;
    case 8:
        scribe_u64(mem, m->cap[CRSP], v);
        break;
    default:
        culpa("trude mag=%u", mag);
    }
}

static uint64_t
protrude(Machina *m, Memoria *mem, unsigned mag)
{
    uint64_t v;
    switch (mag) {
    case 2:
        v = lege_u16(mem, m->cap[CRSP]);
        break;
    case 8:
        v = lege_u64(mem, m->cap[CRSP]);
        break;
    default:
        culpa("protrude mag=%u", mag);
        return 0; // impossibile
    }
    m->cap[CRSP] += mag;
    return v;
}

/* ======================================================================== *
 * XIV.  Vocationes systematis.
 *
 * Filtrum rigorosum: tantum enumeratae subsistunt. Omnia alia culpa terminant.
 * ======================================================================== */

/* Linux x86_64 vocatorum numeri (subset). */
#define V_read            0
#define V_write           1
#define V_open            2
#define V_close           3
#define V_stat            4
#define V_fstat           5
#define V_lstat           6
#define V_lseek           8
#define V_mmap            9
#define V_mprotect       10
#define V_munmap         11
#define V_brk            12
#define V_ioctl          16
#define V_readv          19
#define V_writev         20
#define V_access         21
#define V_getpid         39
#define V_exit           60
#define V_uname          63
#define V_fcntl          72
#define V_getcwd         79
#define V_readlink       89
#define V_gettimeofday   96
#define V_getuid        102
#define V_getgid        104
#define V_geteuid       107
#define V_getegid       108
#define V_arch_prctl    158
#define V_time          201
#define V_clock_gettime 228
#define V_exit_group    231
#define V_set_tid_address 218
#define V_set_robust_list 273
#define V_getrandom     318

/* Salubris vocatio: legit/scribit tantum per memoriam imitatoris. */
static long
inv_read(Machina *mac, Memoria *mem, int fd, uint64_t buf, size_t n)
{
    /* Tantum 0 (stdin) permittimus hic. Alii fd possunt esse ex open. */
    uint8_t tmp[4096];
    long lecti_totales = 0;
    while (n > 0) {
        size_t chunk = n > sizeof tmp ? sizeof tmp : n;
        ssize_t r    = read(fd, tmp, chunk);
        if (r < 0)
            return -errno;
        if (r == 0)
            break;
        scribe_mem(mem, buf + lecti_totales, tmp, (size_t)r);
        lecti_totales += r;
        n -= (size_t)r;
        if ((size_t)r < chunk)
            break;
    }
    (void)mac;
    return lecti_totales;
}

static long
inv_write(Machina *mac, Memoria *mem, int fd, uint64_t buf, size_t n)
{
    uint8_t tmp[4096];
    long scripti_totales = 0;
    while (n > 0) {
        size_t chunk = n > sizeof tmp ? sizeof tmp : n;
        lege_mem(mem, buf + scripti_totales, tmp, chunk);
        ssize_t w = write(fd, tmp, chunk);
        if (w < 0)
            return -errno;
        scripti_totales += w;
        if ((size_t)w < chunk)
            break;
        n -= (size_t)w;
    }
    (void)mac;
    return scripti_totales;
}

/* struct iovec imitati: puntatores sunt in spatio virtuali. */
static long
inv_writev(Machina *mac, Memoria *mem, int fd, uint64_t iov, int cnt)
{
    long total = 0;
    for (int i = 0; i < cnt; i++) {
        uint64_t ba = lege_u64(mem, iov + (uint64_t)i * 16);
        uint64_t ln = lege_u64(mem, iov + (uint64_t)i * 16 + 8);
        long r      = inv_write(mac, mem, fd, ba, (size_t)ln);
        if (r < 0)
            return r;
        total += r;
        if ((uint64_t)r < ln)
            break;
    }
    return total;
}

static long
inv_readv(Machina *mac, Memoria *mem, int fd, uint64_t iov, int cnt)
{
    long total = 0;
    for (int i = 0; i < cnt; i++) {
        uint64_t ba = lege_u64(mem, iov + (uint64_t)i * 16);
        uint64_t ln = lege_u64(mem, iov + (uint64_t)i * 16 + 8);
        long r      = inv_read(mac, mem, fd, ba, (size_t)ln);
        if (r < 0)
            return r;
        total += r;
        if ((uint64_t)r < ln)
            break;
    }
    return total;
}

/* brk: si arg 0, redde terminum actualem. Alioqui trahe terminum (infra
 * limen_superum) et redde terminum novum. */
static long
inv_brk(Memoria *mem, uint64_t ptr)
{
    if (ptr == 0)
        return (long)mem->terminus;
    if (ptr < mem->terminus) {
        /* Contractio silenter acceptabilis. */
        mem->terminus = (ptr + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
        return (long)mem->terminus;
    }
    uint64_t novus = (ptr + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
    if (novus > mem->limen_superum - PILA_MAG - PAGINA)
        return (long)mem->terminus;  /* fallit silenter: terminus veternosus. */
    mem->terminus = novus;
    return (long)mem->terminus;
}

/* mmap anonymum: allocamus pagina(s) intra nostrum spatium, supra terminum brk. */
static uint64_t mmap_cursor = 0;  /* initializatur in principali. */

static long
inv_mmap(
    Memoria *mem, uint64_t addr, uint64_t len, uint64_t prot,
    uint64_t flags, long fd, uint64_t off
) {
    (void)addr;
    (void)prot;
    if (fd != -1)
        return -EINVAL;  /* tantum anonymum */
    if (!(flags & MAP_ANONYMOUS))
        return -EINVAL;
    uint64_t n = (len + PAGINA - 1) & ~(uint64_t)(PAGINA - 1);
    if (mmap_cursor + n > mem->limen_superum - PILA_MAG - PAGINA)
        return -ENOMEM;
    uint64_t v = mmap_cursor;
    mmap_cursor += n;
    /* Memoria iam zero. */
    (void)off;
    return (long)v;
}

static long
inv_munmap(uint64_t addr, uint64_t len)
{
    (void)addr;
    (void)len;
    return 0;  /* nihil facimus: allocatorem retro non movemus. */
}

static long
inv_uname(Memoria *mem, uint64_t buf)
{
    /* struct utsname: 6 × 65 octeti. */
    char zero[6 * 65];
    memset(zero, 0, sizeof zero);
    strcpy(zero + 0*65,   "Linux");
    strcpy(zero + 1*65,   "xlxxxvi");
    strcpy(zero + 2*65,   "5.0.0-xlxxxvi");
    strcpy(zero + 3*65,   "#1 SMP xlxxxvi");
    strcpy(zero + 4*65,   "x86_64");
    strcpy(zero + 5*65,   "");
    scribe_mem(mem, buf, zero, sizeof zero);
    return 0;
}

static long
inv_clock_gettime(Memoria *mem, int id, uint64_t ts)
{
    struct timespec t;
    if (clock_gettime((clockid_t)id, &t) < 0)
        return -errno;
    scribe_u64(mem, ts,     (uint64_t)t.tv_sec);
    scribe_u64(mem, ts + 8, (uint64_t)t.tv_nsec);
    return 0;
}

static long
inv_gettimeofday(Memoria *mem, uint64_t tv, uint64_t tz)
{
    (void)tz;
    struct timespec t;
    if (clock_gettime(CLOCK_REALTIME, &t) < 0)
        return -errno;
    if (tv) {
        scribe_u64(mem, tv, (uint64_t)t.tv_sec);
        scribe_u64(mem, tv + 8, (uint64_t)(t.tv_nsec / 1000));
    }
    return 0;
}

static long
inv_time(Memoria *mem, uint64_t pt)
{
    time_t t = time(NULL);
    if (pt)
        scribe_u64(mem, pt, (uint64_t)t);
    return (long)t;
}

static long
inv_arch_prctl(Machina *mac, int code, uint64_t addr)
{
    /* 0x1001 = ARCH_SET_GS, 0x1002 = ARCH_SET_FS,
       0x1003 = ARCH_GET_FS, 0x1004 = ARCH_GET_GS */
    switch (code) {
    case 0x1001:
        mac->gs_base = addr;
        return 0;
    case 0x1002:
        mac->fs_base = addr;
        return 0;
    case 0x1003:
        return (long)mac->fs_base;
    case 0x1004:
        return (long)mac->gs_base;
    }
    return -EINVAL;
}

static long
inv_ioctl(Memoria *mem, int fd, unsigned long req, uint64_t arg)
{
    /* Solum TIOCGWINSZ et isatty-probes sustinemus, simulati: erroremus. */
    (void)mem;
    (void)fd;
    (void)req;
    (void)arg;
    return -ENOTTY;
}

static long
inv_fstat(Memoria *mem, int fd, uint64_t buf)
{
    struct stat s;
    if (fstat(fd, &s) < 0)
        return -errno;
    /* struct stat linuxianus: 144 octeti layout (approx). Scribemus
     * tantum campos necessarios. Pro simplicitate, memoriam zeramus
     * et paucos ponimus. */
    uint8_t z[144];
    memset(z, 0, sizeof z);
    /* st_dev @ 0, st_ino @ 8, st_nlink @ 16, st_mode @ 24 (u32),
     * st_uid @ 28, st_gid @ 32, st_rdev @ 40, st_size @ 48, st_blksize @ 56,
     * st_blocks @ 64, tempora @ 72..... */
    memcpy(z + 0,  &s.st_dev, 8);
    memcpy(z + 8,  &s.st_ino, 8);
    uint64_t nlk = s.st_nlink;
    memcpy(z + 16, &nlk, 8);
    uint32_t mod = s.st_mode;
    memcpy(z + 24, &mod, 4);
    uint32_t uid = s.st_uid;
    memcpy(z + 28, &uid, 4);
    uint32_t gid = s.st_gid;
    memcpy(z + 32, &gid, 4);
    memcpy(z + 40, &s.st_rdev, 8);
    int64_t sz = s.st_size;
    memcpy(z + 48, &sz, 8);
    int64_t bsz = s.st_blksize;
    memcpy(z + 56, &bsz, 8);
    int64_t bkc = s.st_blocks;
    memcpy(z + 64, &bkc, 8);
    scribe_mem(mem, buf, z, sizeof z);
    return 0;
}

static long
inv_open(Memoria *mem, uint64_t pathva, int flags, int mode)
{
    char path[1024];
    size_t i = 0;
    for (; i < sizeof path - 1; i++) {
        uint8_t c = lege_u8(mem, pathva + i);
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
inv_getcwd(Memoria *mem, uint64_t buf, uint64_t sz)
{
    char t[4096];
    if (sz > sizeof t)
        sz = sizeof t;
    if (!getcwd(t, (size_t)sz))
        return -errno;
    size_t n = strlen(t) + 1;
    scribe_mem(mem, buf, t, n);
    return (long)n;
}

static long
inv_getrandom(Memoria *mem, uint64_t buf, size_t len, unsigned flags)
{
    (void)flags;
    /* Fons pseudoaleatorius stabilis sed imprudens: /dev/urandom. */
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f)
        return -EIO;
    uint8_t tmp[4096];
    size_t scriptum = 0;
    while (scriptum < len) {
        size_t chunk = len - scriptum > sizeof tmp ? sizeof tmp : len - scriptum;
        size_t r     = fread(tmp, 1, chunk, f);
        if (r == 0)
            break;
        scribe_mem(mem, buf + scriptum, tmp, r);
        scriptum += r;
    }
    fclose(f);
    return (long)scriptum;
}

/* Nucleus dispatch. */
static void
dispatch_vocationem(Machina *mac, Memoria *mem)
{
    uint64_t n = mac->cap[CRAX];
    uint64_t a = mac->cap[CRDI];
    uint64_t b = mac->cap[CRSI];
    uint64_t c = mac->cap[CRDX];
    uint64_t d = mac->cap[CR10];
    uint64_t e = mac->cap[CR8];
    uint64_t f = mac->cap[CR9];
    long rv;
    nuntio(
        "vocatio #%lu (a=0x%lx b=0x%lx c=0x%lx)",
        (unsigned long)n, (unsigned long)a, (unsigned long)b, (unsigned long)c
    );
    switch (n) {
    case V_read:
        rv = inv_read  (mac, mem, (int)a, b, (size_t)c);
        break;
    case V_write:
        rv = inv_write (mac, mem, (int)a, b, (size_t)c);
        break;
    case V_writev:
        rv = inv_writev(mac, mem, (int)a, b, (int)c);
        break;
    case V_readv:
        rv = inv_readv (mac, mem, (int)a, b, (int)c);
        break;
    case V_open:
        rv = inv_open  (mem, a, (int)b, (int)c);
        break;
    case V_close:
        rv = (close((int)a) < 0) ? -errno : 0;
        break;
    case V_fstat:
        rv = inv_fstat (mem, (int)a, b);
        break;
    case V_lseek:
        rv = (long)lseek((int)a, (off_t)b, (int)c);
        if (rv<0)
            rv = -errno;
        break;
    case V_brk:
        rv = inv_brk(mem, a);
        break;
    case V_mmap:
        rv = inv_mmap(mem, a, b, c, d, (long)e, f);
        break;
    case V_munmap:
        rv = inv_munmap(a, b);
        break;
    case V_mprotect:
        rv = 0;
        break;
        /* imitamus: non custodimus. */
    case V_ioctl:
        rv = inv_ioctl(mem, (int)a, (unsigned long)b, c);
        break;
    case V_access:
        rv = -EACCES;
        break;
    case V_getpid:
        rv = (long)getpid();
        break;
    case V_getuid:
        rv = (long)getuid();
        break;
    case V_getgid:
        rv = (long)getgid();
        break;
    case V_geteuid:
        rv = (long)geteuid();
        break;
    case V_getegid:
        rv = (long)getegid();
        break;
    case V_uname:
        rv = inv_uname(mem, a);
        break;
    case V_gettimeofday:
        rv = inv_gettimeofday(mem, a, b);
        break;
    case V_time:
        rv = inv_time(mem, a);
        break;
    case V_clock_gettime:
        rv = inv_clock_gettime(mem, (int)a, b);
        break;
    case V_arch_prctl:
        rv = inv_arch_prctl(mac, (int)a, b);
        break;
    case V_readlink:
        rv = inv_readlink(mem, a, b, c);
        break;
    case V_getcwd:
        rv = inv_getcwd(mem, a, b);
        break;
    case V_getrandom:
        rv = inv_getrandom(mem, a, (size_t)b, (unsigned)c);
        break;
    case V_set_tid_address:
        rv = 1;
        break;
        /* numerum tidis simulatum reddimus. */
    case V_set_robust_list:
        rv = 0;
        break;
    case V_fcntl:
        rv = 0;
        break;
        /* laxum. */
    case V_exit:
    case V_exit_group:
        nuntio("exit %ld", (long)a);
        exit((int)a);
    default:
        culpa(
            "vocatio non permissa: #%lu (rip=0x%lx)",
            (unsigned long)n, (unsigned long)mac->rip
        );
        return; // impossibile
    }
    mac->cap[CRAX] = (uint64_t)rv;
}

/* ======================================================================== *
 * XV.  Exsecutio iussorum integralium.
 * ======================================================================== */

/* Gruppus arithmeticus 0..7 pro opcodes 0x00..0x3F et 0x80..0x83:
 * 0=ADD 1=OR 2=ADC 3=SBB 4=AND 5=SUB 6=XOR 7=CMP. */
static uint64_t
arith_op(Machina *m, unsigned op, uint64_t a, uint64_t b, unsigned mag)
{
    uint64_t msk = masca(mag);
    uint64_t r;
    switch (op) {
    case 0:
        r = (a + b) & msk;
        vex_add(m, a, b, r, mag);
        return r;
    case 1:
        r = (a | b) & msk;
        vex_logica(m, r, mag);
        return r;
    case 2: {
            uint64_t c = (m->vexilla & VCF) ? 1 : 0;
            r = (a + b + c) & msk;
            vex_adc(m, a, b, c, r, mag);
            return r;
        }
    case 3: {
            uint64_t c = (m->vexilla & VCF) ? 1 : 0;
            r = (a - b - c) & msk;
            vex_sbb(m, a, b, c, r, mag);
            return r;
        }
    case 4:
        r = (a & b) & msk;
        vex_logica(m, r, mag);
        return r;
    case 5:
        r = (a - b) & msk;
        vex_sub(m, a, b, r, mag);
        return r;
    case 6:
        r = (a ^ b) & msk;
        vex_logica(m, r, mag);
        return r;
    case 7:
        r = (a - b) & msk;
        vex_sub(m, a, b, r, mag);
        return a;
    }
    culpa("arith_op invalida: %u", op);
    return 0; // impossibile
}

/* Shift gruppus: /4 SHL, /5 SHR, /6 SHL (nota: SHL et SAL idem), /7 SAR,
 * /0 ROL, /1 ROR, /2 RCL, /3 RCR. */
static uint64_t
shift_op(Machina *m, unsigned sub, uint64_t a, unsigned cnt, unsigned mag)
{
    unsigned bits     = mag * 8;
    unsigned mask_cnt = (mag == 8) ? 63 : 31;
    cnt &= mask_cnt;
    if (cnt == 0)
        return a;
    uint64_t msk = masca(mag);
    uint64_t r   = a & msk;
    int cf       = 0, of_out = 0;
    switch (sub) {
    case 4: case 6: /* SHL */
        cf     = (r >> (bits - cnt)) & 1;
        r      = (r << cnt) & msk;
        of_out = ((r >> (bits - 1)) & 1) ^ cf;
        break;
    case 5: /* SHR */
        cf     = (r >> (cnt - 1)) & 1;
        of_out = (a >> (bits - 1)) & 1;
        r      = (r >> cnt) & msk;
        break;
    case 7: { /* SAR */
            int64_t sa = signum_ext(a, mag);
            cf = (uint64_t)(sa >> (cnt - 1)) & 1;
            sa = sa >> cnt;
            r = (uint64_t)sa & msk;
            of_out = 0;
            break;
        }
    case 0: { /* ROL */
            r  = ((r << cnt) | (r >> (bits - cnt))) & msk;
            cf = r & 1;
            if (cnt == 1)
                of_out = ((r >> (bits - 1)) & 1) ^ cf;
            break;
        }
    case 1: { /* ROR */
            r  = ((r >> cnt) | (r << (bits - cnt))) & msk;
            cf = (r >> (bits - 1)) & 1;
            if (cnt == 1)
                of_out = ((r >> (bits - 1)) & 1) ^ ((r >> (bits - 2)) & 1);
            break;
        }
    default:
        culpa("shift subopcode non permissus: /%u", sub);
    }
    m->vexilla = (m->vexilla & ~(VCF | VOF)) | (cf ? VCF : 0)
        | (cnt == 1 ? (of_out ? VOF : 0) : (m->vexilla & VOF));
    if (sub == 4 || sub == 5 || sub == 6 || sub == 7)
        vex_zsp(m, r, mag);
    return r;
}

/* ======================================================================== *
 * XVI.  Nucleus exsecutionis.
 *
 * Unum iussum e positione RIP resolvemus et exsequemur. Si opcode non
 * cognitus, culpa.
 * ======================================================================== */

static void step(Machina *mac, Memoria *mem);

static void
exsequi(Machina *mac, Memoria *mem)
{
    for (;;)
        step(mac, mem);
}

/* Subauxilium: exsequi operationem arithmeticam in R/M. */
static void
arith_rm_reg(
    Machina *mac, Memoria *mem, const Resolutio *r, unsigned op,
    unsigned mag, int dir_reg_ad_rm
) {
    uint64_t rm = lege_rm(mac, mem, r, mag);
    uint64_t rg = lege_reg_mag(mac, r, mag);
    uint64_t res;
    if (dir_reg_ad_rm) {
        res = arith_op(mac, op, rm, rg, mag);
        if (op != 7)
            scribe_rm(mac, mem, r, mag, res);
    } else {
        res = arith_op(mac, op, rg, rm, mag);
        if (op != 7)
            scribe_reg_mag(mac, r, mag, res);
    }
}

/* SSE addressing: lege/scribe 128 bit ex memoria vel XMM reg. */
static void
lege_xmm_rm(Machina *mac, Memoria *mem, const Resolutio *r, CapsaXmm *out)
{
    if (r->mod == 3) {
        *out = mac->xmm[r->rm | (r->rex_b << 3)];
    } else {
        uint64_t la = locus_efficax(mac, r, mac->rip);
        out->o[0]   = lege_u64(mem, la);
        out->o[1]   = lege_u64(mem, la + 8);
    }
}

static void
scribe_xmm_rm(Machina *mac, Memoria *mem, const Resolutio *r, const CapsaXmm *in)
{
    if (r->mod == 3) {
        mac->xmm[r->rm | (r->rex_b << 3)] = *in;
    } else {
        uint64_t la = locus_efficax(mac, r, mac->rip);
        scribe_u64(mem, la,     in->o[0]);
        scribe_u64(mem, la + 8, in->o[1]);
    }
}

/* Radix quadrata interna, ne -lm pendat. Newtonis methodus. */
static double
radix_quad(double x)
{
    if (x != x)
        return x;
    if (x < 0)
        return 0.0/0.0;
    if (x == 0)
        return 0;
    double g = x > 1 ? x / 2 : 1.0;
    for (int i = 0; i < 64; i++) {
        double ng = 0.5 * (g + x / g);
        if (ng == g)
            break;
        g = ng;
    }
    return g;
}

/* Compara duplices (UCOMISD/COMISD). Flags: ZF,PF,CF pro ordered compare. */
static void
compara_duplices(Machina *m, double a, double b)
{
    m->vexilla &= ~(VZF | VPF | VCF | VOF | VSF | VAF);
    if (a != a || b != b) {
        m->vexilla |= VZF | VPF | VCF;
    } else if (a > b) {
        /* sine VZF, VPF, VCF: 0 */
    } else if (a < b) {
        m->vexilla |= VCF;
    } else {
        m->vexilla |= VZF;
    }
}

static void
compara_simplices(Machina *m, float a, float b)
{
    compara_duplices(m, (double)a, (double)b);
}

/* Opcode 0F principalis (SSE, MOVZX, MOVSX, Jcc rel32, CMOVcc, SETcc, IMUL). */
static void
step_0f(Machina *mac, Memoria *mem, Resolutio *r, uint8_t op)
{
    unsigned mag = mag_operandi(r);
    switch (op) {

    /* CMOVcc r, r/m */
    case 0x40: case 0x41: case 0x42: case 0x43:
    case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4a: case 0x4b:
    case 0x4c: case 0x4d: case 0x4e: case 0x4f:
        resolve_modrm(mem, &mac->rip, r);
        {
            uint64_t v = lege_rm(mac, mem, r, mag);
            if (conditio(mac, op & 0xf))
                scribe_reg_mag(mac, r, mag, v);
        }
        return;

    /* Jcc rel32 */
    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8a: case 0x8b:
    case 0x8c: case 0x8d: case 0x8e: case 0x8f:
        {
            int32_t rel = (int32_t)lege_u32(mem, mac->rip);
            mac->rip += 4;
            if (conditio(mac, op & 0xf))
                mac->rip += (int64_t)rel;
        }
        return;

    /* SETcc r/m8 */
    case 0x90: case 0x91: case 0x92: case 0x93:
    case 0x94: case 0x95: case 0x96: case 0x97:
    case 0x98: case 0x99: case 0x9a: case 0x9b:
    case 0x9c: case 0x9d: case 0x9e: case 0x9f:
        resolve_modrm(mem, &mac->rip, r);
        scribe_rm(mac, mem, r, 1, conditio(mac, op & 0xf) ? 1 : 0);
        return;

    /* MOVZX r, r/m8 */
    case 0xb6:
        resolve_modrm(mem, &mac->rip, r);
        {
            uint64_t v = lege_rm(mac, mem, r, 1);
            scribe_reg_mag(mac, r, mag, v & 0xff);
        }
        return;
    /* MOVZX r, r/m16 */
    case 0xb7:
        resolve_modrm(mem, &mac->rip, r);
        {
            uint64_t v = lege_rm(mac, mem, r, 2);
            scribe_reg_mag(mac, r, mag, v & 0xffff);
        }
        return;
    /* MOVSX r, r/m8 */
    case 0xbe:
        resolve_modrm(mem, &mac->rip, r);
        {
            int64_t v = (int8_t)lege_rm(mac, mem, r, 1);
            scribe_reg_mag(mac, r, mag, (uint64_t)v);
        }
        return;
    /* MOVSX r, r/m16 */
    case 0xbf:
        resolve_modrm(mem, &mac->rip, r);
        {
            int64_t v = (int16_t)lege_rm(mac, mem, r, 2);
            scribe_reg_mag(mac, r, mag, (uint64_t)v);
        }
        return;

    /* IMUL r, r/m */
    case 0xaf:
        resolve_modrm(mem, &mac->rip, r);
        {
            int64_t a = signum_ext(lege_reg_mag(mac, r, mag), mag);
            int64_t b = signum_ext(lege_rm(mac, mem, r, mag), mag);
            /* detect overflow: signed */
            int64_t res  = a * b;
            uint64_t msk = masca(mag);
            int cf       = 0;
            if (mag == 4) {
                int64_t full = (int64_t)(int32_t)a * (int64_t)(int32_t)b;
                if (full != (int64_t)(int32_t)full)
                    cf = 1;
            } else if (mag == 8) {
                int64_t  fhi;
                uint64_t flo;
                mul64_128s(a, b, &fhi, &flo);
                if (!imul_capit_64(fhi, flo))
                    cf = 1;
            } else if (mag == 2) {
                int32_t full = (int32_t)(int16_t)a * (int32_t)(int16_t)b;
                if (full != (int32_t)(int16_t)full)
                    cf = 1;
            }
            scribe_reg_mag(mac, r, mag, (uint64_t)res & msk);
            mac->vexilla &= ~(VCF | VOF);
            if (cf)
                mac->vexilla |= (VCF | VOF);
            vex_zsp(mac, (uint64_t)res, mag);
        }
        return;

    /* BSWAP r (0x0f c8 + rd) */
    case 0xc8: case 0xc9: case 0xca: case 0xcb:
    case 0xcc: case 0xcd: case 0xce: case 0xcf:
        {
            unsigned idx = (op - 0xc8) | (r->rex_b << 3);
            uint64_t v   = mac->cap[idx & 0xf];
            if (mag == 8) {
                v = ((v & 0x00000000000000ffull) << 56)
                    | ((v & 0x000000000000ff00ull) << 40)
                    | ((v & 0x0000000000ff0000ull) << 24)
                    | ((v & 0x00000000ff000000ull) <<  8)
                    | ((v & 0x000000ff00000000ull) >>  8)
                    | ((v & 0x0000ff0000000000ull) >> 24)
                    | ((v & 0x00ff000000000000ull) >> 40)
                    | ((v & 0xff00000000000000ull) >> 56);
            } else {
                uint32_t w = (uint32_t)v;
                w = ((w & 0x000000ffu) << 24) | ((w & 0x0000ff00u) << 8)
                    | ((w & 0x00ff0000u) >> 8) | ((w & 0xff000000u) >> 24);
                v = w;  /* upper 32 = 0 */
            }
            cap_scribe64(mac, idx, v);
        }
        return;

    /* BSF / BSR */
    case 0xbc: case 0xbd:
        resolve_modrm(mem, &mac->rip, r);
        {
            uint64_t v = lege_rm(mac, mem, r, mag) & masca(mag);
            if (v == 0) {
                mac->vexilla |= VZF;
            } else {
                mac->vexilla &= ~VZF;
                unsigned p = 0;
                if (op == 0xbc) {
                    while (!(v & 1)) {
                        v >>= 1;
                        p++;
                    }
                } else {
                    p = mag * 8 - 1;
                    uint64_t b = (uint64_t)1 << p;
                    while (!(v & b)) {
                        b >>= 1;
                        p--;
                    }
                }
                scribe_reg_mag(mac, r, mag, p);
            }
        }
        return;

    /* MOVD/MOVQ xmm,r/m (66 0f 6e) — sumptio cum p66 */
    case 0x6e:
        resolve_modrm(mem, &mac->rip, r);
        if (r->p66) {
            uint64_t v = lege_rm(mac, mem, r, r->rex_w ? 8 : 4);
            unsigned idx = r->reg | (r->rex_r << 3);
            mac->xmm[idx & 0xf].o[0] = r->rex_w ? v : (uint32_t)v;
            mac->xmm[idx & 0xf].o[1] = 0;
            return;
        }
        culpa("0F 6E sine 66 non sustinetur (rip=0x%lx)", (unsigned long)mac->rip);

    /* MOVD/MOVQ r/m,xmm (66 0f 7e) aut MOVQ xmm,xmm/m64 (F3 0F 7E) */
    case 0x7e:
        resolve_modrm(mem, &mac->rip, r);
        if (r->p66) {
            unsigned idx = r->reg | (r->rex_r << 3);
            uint64_t v   = r->rex_w ? mac->xmm[idx & 0xf].o[0] : mac->xmm[idx & 0xf].d[0];
            scribe_rm(mac, mem, r, r->rex_w ? 8 : 4, v);
            return;
        }
        if (r->rep) {
            CapsaXmm c = {{0, 0}};
            if (r->mod == 3)
                c = mac->xmm[r->rm | (r->rex_b<<3)];
            else
                c.o[0] = lege_u64(mem, locus_efficax(mac, r, mac->rip));
            c.o[1] = 0;
            mac->xmm[r->reg | (r->rex_r<<3)] = c;
            return;
        }
        culpa("0F 7E forma non sustinetur");

    /* MOVDQA (66 0F 6F) / MOVDQU (F3 0F 6F) — lege xmm,xmm/m128 */
    case 0x6f:
        resolve_modrm(mem, &mac->rip, r);
        {
            CapsaXmm c;
            lege_xmm_rm(mac, mem, r, &c);
            mac->xmm[r->reg | (r->rex_r<<3)] = c;
        }
        return;
    case 0x7f:
        resolve_modrm(mem, &mac->rip, r);
        {
            CapsaXmm c = mac->xmm[r->reg | (r->rex_r<<3)];
            scribe_xmm_rm(mac, mem, r, &c);
        }
        return;

    /* MOVAPS/MOVAPD xmm, xmm/m128 (0F 28) + scribe (0F 29) */
    case 0x28:
        resolve_modrm(mem, &mac->rip, r);
        {
            CapsaXmm c;
            lege_xmm_rm(mac, mem, r, &c);
            mac->xmm[r->reg | (r->rex_r<<3)] = c;
        }
        return;
    case 0x29:
        resolve_modrm(mem, &mac->rip, r);
        {
            CapsaXmm c = mac->xmm[r->reg | (r->rex_r<<3)];
            scribe_xmm_rm(mac, mem, r, &c);
        }
        return;

    /* MOVSS / MOVSD (F3 / F2 0F 10/11) */
    case 0x10:
    case 0x11:
        resolve_modrm(mem, &mac->rip, r);
        if (r->rep || r->repne) {
            unsigned size = r->rep ? 4 : 8;
            if (op == 0x10) {
                /* dst = xmm reg, src = rm */
                uint64_t v;
                if (r->mod == 3) {
                    v = size == 4
                        ? (uint64_t)mac->xmm[r->rm|(r->rex_b<<3)].d[0]
                        : mac->xmm[r->rm|(r->rex_b<<3)].o[0];
                } else {
                    uint64_t la = locus_efficax(mac, r, mac->rip);
                    v = size == 4 ? lege_u32(mem, la) : lege_u64(mem, la);
                    /* cum ex memoria: upper zeroed pro reg */
                    CapsaXmm z = {{0, 0}};
                    if (size == 4)
                        z.d[0] = (uint32_t)v;
                    else
                        z.o[0] = v;
                    mac->xmm[r->reg|(r->rex_r<<3)] = z;
                    return;
                }
                /* reg-to-reg: tantum infimus conservatur, ceteri manent */
                if (size == 4)
                    mac->xmm[r->reg|(r->rex_r<<3)].d[0] = (uint32_t)v;
                else
                    mac->xmm[r->reg|(r->rex_r<<3)].o[0] = v;
            } else {
                /* 0x11: src = xmm reg, dst = rm */
                uint64_t v = size == 4
                    ? (uint64_t)mac->xmm[r->reg|(r->rex_r<<3)].d[0]
                    : mac->xmm[r->reg|(r->rex_r<<3)].o[0];
                if (r->mod == 3) {
                    if (size == 4)
                        mac->xmm[r->rm|(r->rex_b<<3)].d[0] = (uint32_t)v;
                    else
                        mac->xmm[r->rm|(r->rex_b<<3)].o[0] = v;
                } else {
                    uint64_t la = locus_efficax(mac, r, mac->rip);
                    if (size == 4)
                        scribe_u32(mem, la, (uint32_t)v);
                    else
                        scribe_u64(mem, la, v);
                }
            }
            return;
        }
        /* MOVUPS/MOVUPD (non-scalar) */
        if (op == 0x10) {
            CapsaXmm c;
            lege_xmm_rm(mac, mem, r, &c);
            mac->xmm[r->reg|(r->rex_r<<3)] = c;
        } else {
            CapsaXmm c = mac->xmm[r->reg|(r->rex_r<<3)];
            scribe_xmm_rm(mac, mem, r, &c);
        }
        return;

    /* Operationes SSE2 scalariae: ADD/SUB/MUL/DIV/SQRT/MIN/MAX + UCOMI/COMI. */
    case 0x51: case 0x58: case 0x59: case 0x5c: case 0x5d: case 0x5e:
    case 0x5f:
    case 0x2e: case 0x2f:
        resolve_modrm(mem, &mac->rip, r);
        {
            int duplex  = r->repne;     /* F2 = double, F3 = single */
            int simplex = r->rep;
            int scalar  = duplex || simplex;
            int ucomi   = (op == 0x2e || op == 0x2f);
            if (!scalar && !ucomi && !r->p66)
                culpa("SSE sine praenuntio (op=%02x) non sustinetur", op);
            if (ucomi) {
                /* COMISD/UCOMISD (66) vel COMISS/UCOMISS (nihil) */
                if (r->p66) {
                    /* double */
                    CapsaXmm b;
                    lege_xmm_rm(mac, mem, r, &b);
                    union {
                        uint64_t u;
                        double d;
                    } A, B;
                    A.u = mac->xmm[r->reg|(r->rex_r<<3)].o[0];
                    B.u = b.o[0];
                    compara_duplices(mac, A.d, B.d);
                } else {
                    CapsaXmm b;
                    lege_xmm_rm(mac, mem, r, &b);
                    union {
                        uint32_t u;
                        float f;
                    } A, B;
                    A.u = mac->xmm[r->reg|(r->rex_r<<3)].d[0];
                    B.u = b.d[0];
                    compara_simplices(mac, A.f, B.f);
                }
                return;
            }
            unsigned sz = duplex ? 8 : 4;
            if (!scalar)
                culpa("SSE packed (op=%02x) non sustinetur", op);
            /* lege operandum src */
            uint64_t rb = 0;
            if (r->mod == 3) {
                rb = sz == 4
                    ? (uint64_t)mac->xmm[r->rm|(r->rex_b<<3)].d[0]
                    : mac->xmm[r->rm|(r->rex_b<<3)].o[0];
            } else {
                uint64_t la = locus_efficax(mac, r, mac->rip);
                rb = sz == 4 ? lege_u32(mem, la) : lege_u64(mem, la);
            }
            uint64_t ra = sz == 4
                ? (uint64_t)mac->xmm[r->reg|(r->rex_r<<3)].d[0]
                : mac->xmm[r->reg|(r->rex_r<<3)].o[0];
            if (duplex) {
                union {
                    uint64_t u;
                    double d;
                } A, B, R;
                A.u = ra;
                B.u = rb;
                switch (op) {
                case 0x58:
                    R.d = A.d + B.d;
                    break;
                case 0x59:
                    R.d = A.d * B.d;
                    break;
                case 0x5c:
                    R.d = A.d - B.d;
                    break;
                case 0x5d:
                    R.d = (A.d < B.d) ? A.d : B.d;
                    break;
                case 0x5e:
                    R.d = A.d / B.d;
                    break;
                case 0x5f:
                    R.d = (A.d > B.d) ? A.d : B.d;
                    break;
                case 0x51:
                    R.d = radix_quad(B.d);
                    break;
                default:
                    culpa("SSE dbl op=%02x", op);
                }
                mac->xmm[r->reg|(r->rex_r<<3)].o[0] = R.u;
            } else {
                union {
                    uint32_t u;
                    float f;
                } A, B, R;
                A.u = (uint32_t)ra;
                B.u = (uint32_t)rb;
                switch (op) {
                case 0x58:
                    R.f = A.f + B.f;
                    break;
                case 0x59:
                    R.f = A.f * B.f;
                    break;
                case 0x5c:
                    R.f = A.f - B.f;
                    break;
                case 0x5d:
                    R.f = (A.f < B.f) ? A.f : B.f;
                    break;
                case 0x5e:
                    R.f = A.f / B.f;
                    break;
                case 0x5f:
                    R.f = (A.f > B.f) ? A.f : B.f;
                    break;
                case 0x51:
                    R.f = (float)radix_quad((double)B.f);
                    break;
                default:
                    culpa("SSE flt op=%02x", op);
                }
                mac->xmm[r->reg|(r->rex_r<<3)].d[0] = R.u;
            }
        }
        return;

    /* CVTSI2SD/SS (F2/F3 0F 2A) */
    case 0x2a:
        resolve_modrm(mem, &mac->rip, r);
        {
            int duplex  = r->repne;
            int simplex = r->rep;
            if (!duplex && !simplex)
                culpa("CVT non sustinetur");
            unsigned smag = r->rex_w ? 8 : 4;
            int64_t src   = signum_ext(lege_rm(mac, mem, r, smag), smag);
            if (duplex) {
                union {
                    uint64_t u;
                    double d;
                } R;
                R.d = (double)src;
                mac->xmm[r->reg|(r->rex_r<<3)].o[0] = R.u;
            } else {
                union {
                    uint32_t u;
                    float f;
                } R;
                R.f = (float)src;
                mac->xmm[r->reg|(r->rex_r<<3)].d[0] = R.u;
            }
        }
        return;

    /* CVTTSD2SI / CVTSD2SI  (F2/F3 0F 2C / 2D) */
    case 0x2c:
    case 0x2d:
        resolve_modrm(mem, &mac->rip, r);
        {
            int duplex  = r->repne;
            int simplex = r->rep;
            if (!duplex && !simplex)
                culpa("CVT redde non sustinetur");
            unsigned dmag = r->rex_w ? 8 : 4;
            int64_t iv;
            if (duplex) {
                union {
                    uint64_t u;
                    double d;
                } V;
                if (r->mod == 3)
                    V.u = mac->xmm[r->rm|(r->rex_b<<3)].o[0];
                else
                    V.u = lege_u64(mem, locus_efficax(mac, r, mac->rip));
                iv = (int64_t)V.d;
            } else {
                union {
                    uint32_t u;
                    float f;
                } V;
                if (r->mod == 3)
                    V.u = mac->xmm[r->rm|(r->rex_b<<3)].d[0];
                else
                    V.u = lege_u32(mem, locus_efficax(mac, r, mac->rip));
                iv = (int64_t)V.f;
            }
            scribe_reg_mag(mac, r, dmag, (uint64_t)iv);
        }
        return;

    /* CVTSS2SD (F3 0F 5A) / CVTSD2SS (F2 0F 5A) */
    case 0x5a:
        resolve_modrm(mem, &mac->rip, r);
        if (r->rep) {
            /* ss -> sd */
            uint32_t su;
            if (r->mod == 3)
                su = mac->xmm[r->rm|(r->rex_b<<3)].d[0];
            else
                su = lege_u32(mem, locus_efficax(mac, r, mac->rip));
            union {
                uint32_t u;
                float f;
            } S;
            S.u = su;
            union {
                uint64_t u;
                double d;
            } D;
            D.d = (double)S.f;
            mac->xmm[r->reg|(r->rex_r<<3)].o[0] = D.u;
        } else if (r->repne) {
            uint64_t du;
            if (r->mod == 3)
                du = mac->xmm[r->rm|(r->rex_b<<3)].o[0];
            else
                du = lege_u64(mem, locus_efficax(mac, r, mac->rip));
            union {
                uint64_t u;
                double d;
            } D;
            D.u = du;
            union {
                uint32_t u;
                float f;
            } S;
            S.f = (float)D.d;
            mac->xmm[r->reg|(r->rex_r<<3)].d[0] = S.u;
        } else
            culpa("0F 5A sine prefix");
        return;

    /* PXOR/XORPS/XORPD (0F EF / 0F 57) */
    case 0xef:
    case 0x57:
        resolve_modrm(mem, &mac->rip, r);
        {
            CapsaXmm c;
            lege_xmm_rm(mac, mem, r, &c);
            unsigned i = r->reg | (r->rex_r<<3);
            mac->xmm[i].o[0] ^= c.o[0];
            mac->xmm[i].o[1] ^= c.o[1];
        }
        return;

    /* ANDPS/ANDPD (0F 54) */
    case 0x54:
        resolve_modrm(mem, &mac->rip, r);
        {
            CapsaXmm c;
            lege_xmm_rm(mac, mem, r, &c);
            unsigned i = r->reg | (r->rex_r<<3);
            mac->xmm[i].o[0] &= c.o[0];
            mac->xmm[i].o[1] &= c.o[1];
        }
        return;

    /* ORPS/ORPD (0F 56) */
    case 0x56:
        resolve_modrm(mem, &mac->rip, r);
        {
            CapsaXmm c;
            lege_xmm_rm(mac, mem, r, &c);
            unsigned i = r->reg | (r->rex_r<<3);
            mac->xmm[i].o[0] |= c.o[0];
            mac->xmm[i].o[1] |= c.o[1];
        }
        return;

    /* SYSCALL (0F 05) */
    case 0x05:
        dispatch_vocationem(mac, mem);
        return;

    /* RDTSC (0F 31) — simulato cum clock. */
    case 0x31:
        {
            struct timespec t;
            clock_gettime(CLOCK_MONOTONIC, &t);
            uint64_t tc    = (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
            mac->cap[CRAX] = tc & 0xffffffffull;
            mac->cap[CRDX] = tc >> 32;
        }
        return;

    /* NOP multi-byte (0F 1F /0) et ENDBR64/32 (F3 0F 1E /0). Tractamus ut NOP. */
    case 0x1f:
    case 0x1e:
        resolve_modrm(mem, &mac->rip, r);
        return;

    /* BT / BTS / BTR / BTC (0F A3, AB, B3, BB) — r/m, r */
    case 0xa3: case 0xab: case 0xb3: case 0xbb:
        resolve_modrm(mem, &mac->rip, r);
        {
            uint64_t off = signum_ext(lege_reg_mag(mac, r, mag), mag);
            uint64_t a   = lege_rm(mac, mem, r, mag);
            uint64_t bit = (uint64_t)1 << (off & (mag * 8 - 1));
            int cf       = (a & bit) ? 1 : 0;
            mac->vexilla = (mac->vexilla & ~VCF) | (cf ? VCF : 0);
            if (op == 0xab)
                a |= bit;
            else if (op == 0xb3)
                a &= ~bit;
            else if (op == 0xbb)
                a ^= bit;
            if (op != 0xa3)
                scribe_rm(mac, mem, r, mag, a & masca(mag));
        }
        return;
    /* BT group (0F BA /4..7 imm8). Imm primum. */
    case 0xba:
        resolve_modrm(mem, &mac->rip, r);
        {
            uint64_t imm = lege_u8(mem, mac->rip);
            mac->rip++;
            uint64_t a   = lege_rm(mac, mem, r, mag);
            uint64_t bit = (uint64_t)1 << (imm & (mag * 8 - 1));
            int cf       = (a & bit) ? 1 : 0;
            mac->vexilla = (mac->vexilla & ~VCF) | (cf ? VCF : 0);
            switch (r->reg) {
            case 4:
                break;
                /* BT */
            case 5:
                scribe_rm(mac, mem, r, mag, (a |  bit) & masca(mag));
                break;
            case 6:
                scribe_rm(mac, mem, r, mag, (a & ~bit) & masca(mag));
                break;
            case 7:
                scribe_rm(mac, mem, r, mag, (a ^  bit) & masca(mag));
                break;
            default:
                culpa("0F BA /%u non valet", r->reg);
            }
        }
        return;

    /* MOVQ r/m64, xmm (66 0F D6) */
    case 0xd6:
        resolve_modrm(mem, &mac->rip, r);
        if (r->p66) {
            unsigned i = r->reg | (r->rex_r << 3);
            uint64_t v = mac->xmm[i & 0xf].o[0];
            if (r->mod == 3) {
                mac->xmm[r->rm | (r->rex_b << 3)].o[0] = v;
                mac->xmm[r->rm | (r->rex_b << 3)].o[1] = 0;
            } else {
                scribe_u64(mem, locus_efficax(mac, r, mac->rip), v);
            }
            return;
        }
        culpa("0F D6 sine 66 non sustinetur");

    /* UD2 */
    case 0x0b:
        culpa("UD2 invocatum ad rip=0x%lx", (unsigned long)mac->rip);

    default:
        culpa(
            "opcode 0F %02x non sustinetur (rip=0x%lx)",
            op, (unsigned long)mac->rip
        );
    }
}

static void
step(Machina *mac, Memoria *mem)
{
    Resolutio r = {0};
    uint64_t rip_init = mac->rip;
    resolve_praenuntia(mem, &mac->rip, &r);
    uint8_t op = lege_u8(mem, mac->rip);
    mac->rip++;

    if (verbositas >= 2)
        fprintf(
            stderr, "rip=0x%lx op=%02x rex=%02x\n",
            (unsigned long)rip_init, op, r.rex
        );

    /* 0F escape. */
    if (op == 0x0f) {
        uint8_t op2 = lege_u8(mem, mac->rip);
        mac->rip++;
        r.op_0f     = 1;
        step_0f(mac, mem, &r, op2);
        return;
    }

    unsigned mag = mag_operandi(&r);

    /* Arith gruppa directa: 00..3F, per pattern op = (gp<<3) | mode, mode 0..5. */
    if (op < 0x40) {
        unsigned gp = (op >> 3) & 7;
        unsigned md = op & 7;
        if (
            gp == 0 || gp == 1 || gp == 2 || gp == 3
            || gp == 4 || gp == 5 || gp == 6 || gp == 7
        ) {
            switch (md) {
            case 0: /* r/m8, r8 */
                resolve_modrm(mem, &mac->rip, &r);
                {
                    uint64_t a   = lege_rm(mac, mem, &r, 1);
                    uint64_t b   = lege_reg_mag(mac, &r, 1);
                    uint64_t res = arith_op(mac, gp, a, b, 1);
                    if (gp != 7)
                        scribe_rm(mac, mem, &r, 1, res);
                }
                return;
            case 1: /* r/m, r */
                resolve_modrm(mem, &mac->rip, &r);
                arith_rm_reg(mac, mem, &r, gp, mag, 1);
                return;
            case 2: /* r8, r/m8 */
                resolve_modrm(mem, &mac->rip, &r);
                {
                    uint64_t a   = lege_reg_mag(mac, &r, 1);
                    uint64_t b   = lege_rm(mac, mem, &r, 1);
                    uint64_t res = arith_op(mac, gp, a, b, 1);
                    if (gp != 7)
                        scribe_reg_mag(mac, &r, 1, res);
                }
                return;
            case 3: /* r, r/m */
                resolve_modrm(mem, &mac->rip, &r);
                arith_rm_reg(mac, mem, &r, gp, mag, 0);
                return;
            case 4: /* AL, imm8 */
                {
                    uint64_t a = cap_lege8(mac, CRAX, r.rex != 0);
                    uint64_t b = lege_u8(mem, mac->rip);
                    mac->rip++;
                    uint64_t res = arith_op(mac, gp, a, b, 1);
                    if (gp != 7)
                        cap_scribe8(mac, CRAX, (uint8_t)res, r.rex != 0);
                }
                return;
            case 5: /* rAX, imm (32-bit imm, sign-ext si 64) */
                {
                    uint64_t a = (mag == 8) ? mac->cap[CRAX]
                        : (mag == 4) ? (uint64_t)(uint32_t)mac->cap[CRAX]
                        : (mag == 2) ? (uint64_t)(uint16_t)mac->cap[CRAX]
                        : (uint64_t)(uint8_t)mac->cap[CRAX];
                    uint64_t b;
                    if (mag == 2) {
                        b = lege_u16(mem, mac->rip);
                        mac->rip += 2;
                    } else if (mag == 4 || mag == 8) {
                        int32_t t = (int32_t)lege_u32(mem, mac->rip);
                        mac->rip += 4;
                        b = (mag == 8) ? (uint64_t)(int64_t)t : (uint64_t)(uint32_t)t;
                    } else { b = 0; }
                    uint64_t res = arith_op(mac, gp, a, b, mag);
                    if (gp != 7) {
                        if (mag == 8)
                            mac->cap[CRAX] = res;
                        else if (mag == 4)
                            mac->cap[CRAX] = (uint32_t)res;
                        else if (mag == 2)
                            cap_scribe16(mac, CRAX, (uint16_t)res);
                    }
                }
                return;
            }
        }
    }

    switch (op) {
    /* PUSH r (50+rd) */
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56: case 0x57:
        trude(mac, mem, mac->cap[(op - 0x50) | (r.rex_b << 3)], 8);
        return;
    /* POP r (58+rd) */
    case 0x58: case 0x59: case 0x5a: case 0x5b:
    case 0x5c: case 0x5d: case 0x5e: case 0x5f:
        {
            unsigned i  = (op - 0x58) | (r.rex_b << 3);
            mac->cap[i] = protrude(mac, mem, 8);
        }
        return;

    /* MOVSXD r64, r/m32 (63 /r) */
    case 0x63:
        resolve_modrm(mem, &mac->rip, &r);
        {
            int64_t v = (int32_t)lege_rm(mac, mem, &r, 4);
            scribe_reg_mag(mac, &r, mag, (uint64_t)v);
        }
        return;

    /* PUSH imm32 (68) — sign-ext ad 64 */
    case 0x68: {
            int32_t v = (int32_t)lege_u32(mem, mac->rip);
            mac->rip += 4;
            trude(mac, mem, (uint64_t)(int64_t)v, 8);
        } return;
    /* PUSH imm8 (6A) */
    case 0x6a: {
            int8_t v = (int8_t)lege_u8(mem, mac->rip);
            mac->rip++;
            trude(mac, mem, (uint64_t)(int64_t)v, 8);
        } return;

    /* IMUL r, r/m, imm32 (69) / imm8 (6B). Imm primum. */
    case 0x69:
    case 0x6b:
        resolve_modrm(mem, &mac->rip, &r);
        {
            int64_t b;
            if (op == 0x69) {
                if (mag == 2) {
                    b = (int16_t)lege_u16(mem, mac->rip);
                    mac->rip += 2;
                } else {
                    b = (int32_t)lege_u32(mem, mac->rip);
                    mac->rip += 4;
                }
            } else {
                b = (int8_t)lege_u8(mem, mac->rip);
                mac->rip++;
            }
            (void)0;
            int64_t a   = signum_ext(lege_rm(mac, mem, &r, mag), mag);
            int64_t res = a * b;
            scribe_reg_mag(mac, &r, mag, (uint64_t)res & masca(mag));
            mac->vexilla &= ~(VCF | VOF);
            if (mag == 8) {
                int64_t  fhi;
                uint64_t flo;
                mul64_128s(a, b, &fhi, &flo);
                if (!imul_capit_64(fhi, flo))
                    mac->vexilla |= VCF | VOF;
            } else if (mag == 4) {
                int64_t full = (int64_t)(int32_t)a * (int64_t)(int32_t)b;
                if (full != (int32_t)full)
                    mac->vexilla |= VCF | VOF;
            } else if (mag == 2) {
                int32_t full = (int32_t)(int16_t)a * (int32_t)(int16_t)b;
                if (full != (int16_t)full)
                    mac->vexilla |= VCF | VOF;
            }
            vex_zsp(mac, (uint64_t)res, mag);
        }
        return;

    /* Jcc rel8 (70..7F) */
    case 0x70: case 0x71: case 0x72: case 0x73:
    case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b:
    case 0x7c: case 0x7d: case 0x7e: case 0x7f:
        {
            int8_t rel = (int8_t)lege_u8(mem, mac->rip);
            mac->rip++;
            if (conditio(mac, op & 0xf))
                mac->rip += (int64_t)rel;
        }
        return;

    /* Grp 1: ADD/OR/ADC/SBB/AND/SUB/XOR/CMP r/m, imm8/imm.
     * Imm PRIMUM legimus: sic RIP relativus ad finem iussi refertur. */
    case 0x80: case 0x81: case 0x82: case 0x83:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned gp = r.reg;
            unsigned om = (op == 0x80 || op == 0x82) ? 1 : mag;
            uint64_t b;
            if (op == 0x80 || op == 0x82) {
                b = lege_u8(mem, mac->rip);
                mac->rip++;
            } else if (op == 0x83) {
                b = (uint64_t)(int64_t)(int8_t)lege_u8(mem, mac->rip);
                mac->rip++;
                b &= masca(om);
            } else if (op == 0x81) {
                if (om == 2) {
                    b = lege_u16(mem, mac->rip);
                    mac->rip += 2;
                } else {
                    int32_t t = (int32_t)lege_u32(mem, mac->rip);
                    mac->rip += 4;
                    b = (om == 8) ? (uint64_t)(int64_t)t : (uint64_t)(uint32_t)t; }
            } else
                b = 0;
            uint64_t a   = lege_rm(mac, mem, &r, om);
            uint64_t res = arith_op(mac, gp, a, b, om);
            if (gp != 7)
                scribe_rm(mac, mem, &r, om, res);
        }
        return;

    /* TEST r/m, r (84/85) */
    case 0x84: case 0x85:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om = (op == 0x84) ? 1 : mag;
            uint64_t a  = lege_rm(mac, mem, &r, om);
            uint64_t b  = lege_reg_mag(mac, &r, om);
            vex_logica(mac, a & b, om);
        }
        return;

    /* XCHG r, r/m (86/87) */
    case 0x86: case 0x87:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om = (op == 0x86) ? 1 : mag;
            uint64_t a  = lege_rm(mac, mem, &r, om);
            uint64_t b  = lege_reg_mag(mac, &r, om);
            scribe_rm(mac, mem, &r, om, b);
            scribe_reg_mag(mac, &r, om, a);
        }
        return;

    /* MOV r/m, r (88/89) */
    case 0x88: case 0x89:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om = (op == 0x88) ? 1 : mag;
            uint64_t v  = lege_reg_mag(mac, &r, om);
            scribe_rm(mac, mem, &r, om, v);
        }
        return;

    /* MOV r, r/m (8A/8B) */
    case 0x8a: case 0x8b:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om = (op == 0x8a) ? 1 : mag;
            uint64_t v  = lege_rm(mac, mem, &r, om);
            scribe_reg_mag(mac, &r, om, v);
        }
        return;

    /* LEA r, m (8D) */
    case 0x8d:
        resolve_modrm(mem, &mac->rip, &r);
        if (r.mod == 3)
            culpa("LEA cum mod=3 invalidum");
        {
            uint64_t la = locus_efficax(mac, &r, mac->rip);
            scribe_reg_mag(mac, &r, mag, la & masca(mag));
        }
        return;

    /* NOP (90) / XCHG rAX, r (90+rd) */
    case 0x90: case 0x91: case 0x92: case 0x93:
    case 0x94: case 0x95: case 0x96: case 0x97:
        if (op == 0x90 && !r.rex_b)
            return;   /* NOP */
        {
            unsigned i = (op - 0x90) | (r.rex_b << 3);
            uint64_t v = mac->cap[i & 0xf];
            if (mag == 8) {
                mac->cap[i&0xf] = mac->cap[CRAX];
                mac->cap[CRAX]  = v;
            } else if (mag == 4) {
                uint64_t ax     = mac->cap[CRAX];
                mac->cap[i&0xf] = (uint32_t)ax;
                mac->cap[CRAX]  = (uint32_t)v;
            } else if (mag == 2) {
                uint16_t ax = (uint16_t)mac->cap[CRAX];
                uint16_t rv = (uint16_t)v;
                cap_scribe16(mac, i, ax);
                cap_scribe16(mac, CRAX, rv);
            }
        }
        return;

    /* CBW/CWDE/CDQE (98) */
    case 0x98:
        if (mag == 8)
            mac->cap[CRAX] = (uint64_t)(int64_t)(int32_t)mac->cap[CRAX];
        else if (mag == 4)
            mac->cap[CRAX] = (uint32_t)(int32_t)(int16_t)mac->cap[CRAX];
        else if (mag == 2)
            cap_scribe16(mac, CRAX, (uint16_t)(int16_t)(int8_t)mac->cap[CRAX]);
        return;
    /* CWD/CDQ/CQO (99) */
    case 0x99:
        if (mag == 8)
            mac->cap[CRDX] = ((int64_t)mac->cap[CRAX] < 0) ? UINT64_MAX : 0;
        else if (mag == 4)
            mac->cap[CRDX] = ((int32_t)mac->cap[CRAX] < 0) ? 0xffffffffull : 0;
        else if (mag == 2)
            cap_scribe16(mac, CRDX, (int16_t)mac->cap[CRAX] < 0 ? 0xffff : 0);
        return;

    /* MOVS (A4/A5), STOS (AA/AB), LODS (AC/AD), CMPS (A6/A7), SCAS (AE/AF) */
    case 0xa4: case 0xa5: case 0xaa: case 0xab:
    case 0xac: case 0xad: case 0xa6: case 0xa7:
    case 0xae: case 0xaf:
        {
            unsigned om = (op & 1) ? mag : 1;
            int dir     = (mac->vexilla & VDF) ? -1 : 1;
            int do_loop = r.rep || r.repne;
            int is_cmp  = (op == 0xa6 || op == 0xa7 || op == 0xae || op == 0xaf);
            for (;;) {
                if (do_loop) {
                    if (mac->cap[CRCX] == 0)
                        break;
                }
                switch (op) {
                case 0xa4: case 0xa5: {
                        uint64_t v = 0;
                        switch (om) {
                        case 1:
                            v = lege_u8 (mem, mac->cap[CRSI]);
                            break;
                        case 2:
                            v = lege_u16(mem, mac->cap[CRSI]);
                            break;
                        case 4:
                            v = lege_u32(mem, mac->cap[CRSI]);
                            break;
                        case 8:
                            v = lege_u64(mem, mac->cap[CRSI]);
                            break;
                        }
                        switch (om) {
                        case 1:
                            scribe_u8 (mem, mac->cap[CRDI], (uint8_t)v);
                            break;
                        case 2:
                            scribe_u16(mem, mac->cap[CRDI], (uint16_t)v);
                            break;
                        case 4:
                            scribe_u32(mem, mac->cap[CRDI], (uint32_t)v);
                            break;
                        case 8:
                            scribe_u64(mem, mac->cap[CRDI], v);
                            break;
                        }
                        mac->cap[CRSI] += (uint64_t)((int64_t)dir * om);
                        mac->cap[CRDI] += (uint64_t)((int64_t)dir * om);
                        break;
                    }
                case 0xaa: case 0xab: {
                        uint64_t v = mac->cap[CRAX] & masca(om);
                        switch (om) {
                        case 1:
                            scribe_u8 (mem, mac->cap[CRDI], (uint8_t)v);
                            break;
                        case 2:
                            scribe_u16(mem, mac->cap[CRDI], (uint16_t)v);
                            break;
                        case 4:
                            scribe_u32(mem, mac->cap[CRDI], (uint32_t)v);
                            break;
                        case 8:
                            scribe_u64(mem, mac->cap[CRDI], v);
                            break;
                        }
                        mac->cap[CRDI] += (uint64_t)((int64_t)dir * om);
                        break;
                    }
                case 0xac: case 0xad: {
                        uint64_t v = 0;
                        switch (om) {
                        case 1:
                            v = lege_u8 (mem, mac->cap[CRSI]);
                            break;
                        case 2:
                            v = lege_u16(mem, mac->cap[CRSI]);
                            break;
                        case 4:
                            v = lege_u32(mem, mac->cap[CRSI]);
                            break;
                        case 8:
                            v = lege_u64(mem, mac->cap[CRSI]);
                            break;
                        }
                        if (om == 8)
                            mac->cap[CRAX] = v;
                        else if (om == 4)
                            mac->cap[CRAX] = (uint32_t)v;
                        else if (om == 2)
                            cap_scribe16(mac, CRAX, (uint16_t)v);
                        else
                            cap_scribe8(mac, CRAX, (uint8_t)v, r.rex != 0);
                        mac->cap[CRSI] += (uint64_t)((int64_t)dir * om);
                        break;
                    }
                case 0xa6: case 0xa7: {
                        uint64_t a = 0, b = 0;
                        switch (om) {
                        case 1:
                            a = lege_u8 (mem, mac->cap[CRSI]);
                            b = lege_u8 (mem, mac->cap[CRDI]);
                            break;
                        case 2:
                            a = lege_u16(mem, mac->cap[CRSI]);
                            b = lege_u16(mem, mac->cap[CRDI]);
                            break;
                        case 4:
                            a = lege_u32(mem, mac->cap[CRSI]);
                            b = lege_u32(mem, mac->cap[CRDI]);
                            break;
                        case 8:
                            a = lege_u64(mem, mac->cap[CRSI]);
                            b = lege_u64(mem, mac->cap[CRDI]);
                            break;
                        }
                        uint64_t res = (a - b) & masca(om);
                        vex_sub(mac, a, b, res, om);
                        mac->cap[CRSI] += (uint64_t)((int64_t)dir * om);
                        mac->cap[CRDI] += (uint64_t)((int64_t)dir * om);
                        break;
                    }
                case 0xae: case 0xaf: {
                        uint64_t a = mac->cap[CRAX] & masca(om);
                        uint64_t b = 0;
                        switch (om) {
                        case 1:
                            b = lege_u8 (mem, mac->cap[CRDI]);
                            break;
                        case 2:
                            b = lege_u16(mem, mac->cap[CRDI]);
                            break;
                        case 4:
                            b = lege_u32(mem, mac->cap[CRDI]);
                            break;
                        case 8:
                            b = lege_u64(mem, mac->cap[CRDI]);
                            break;
                        }
                        uint64_t res = (a - b) & masca(om);
                        vex_sub(mac, a, b, res, om);
                        mac->cap[CRDI] += (uint64_t)((int64_t)dir * om);
                        break;
                    }
                }
                if (!do_loop)
                    break;
                mac->cap[CRCX]--;
                if (is_cmp) {
                    int zf = (mac->vexilla & VZF) != 0;
                    if (r.rep && !zf)
                        break;
                    if (r.repne && zf)
                        break;
                }
            }
        }
        return;

    /* TEST AL/rAX, imm (A8/A9) */
    case 0xa8: case 0xa9:
        {
            unsigned om = (op == 0xa8) ? 1 : mag;
            uint64_t a, b;
            if (om == 1) {
                a = cap_lege8(mac, CRAX, r.rex != 0);
                b = lege_u8(mem, mac->rip);
                mac->rip++;
            } else if (om == 2) {
                a = (uint16_t)mac->cap[CRAX];
                b = lege_u16(mem, mac->rip);
                mac->rip += 2;
            } else {
                a         = (om == 8) ? mac->cap[CRAX] : (uint32_t)mac->cap[CRAX];
                int32_t t = (int32_t)lege_u32(mem, mac->rip);
                mac->rip += 4;
                b = (om == 8) ? (uint64_t)(int64_t)t : (uint64_t)(uint32_t)t; }
            vex_logica(mac, a & b, om);
        }
        return;

    /* MOV r8, imm8 (B0+rd) */
    case 0xb0: case 0xb1: case 0xb2: case 0xb3:
    case 0xb4: case 0xb5: case 0xb6: case 0xb7:
        {
            unsigned i = (op - 0xb0) | (r.rex_b << 3);
            uint8_t v  = lege_u8(mem, mac->rip);
            mac->rip++;
            cap_scribe8(mac, i, v, r.rex != 0);
        }
        return;
    /* MOV r, imm (B8+rd) */
    case 0xb8: case 0xb9: case 0xba: case 0xbb:
    case 0xbc: case 0xbd: case 0xbe: case 0xbf:
        {
            unsigned i = (op - 0xb8) | (r.rex_b << 3);
            uint64_t v;
            if (mag == 2) {
                v = lege_u16(mem, mac->rip);
                mac->rip += 2;
            } else if (mag == 4) {
                v = lege_u32(mem, mac->rip);
                mac->rip += 4;
            } else {
                v = lege_u64(mem, mac->rip);
                mac->rip += 8;
            }
            if (mag == 8)
                mac->cap[i&0xf] = v;
            else if (mag == 4)
                mac->cap[i&0xf] = (uint32_t)v;
            else
                cap_scribe16(mac, i, (uint16_t)v);
        }
        return;

    /* SHIFT gruppa (C0/C1/D0/D1/D2/D3). Imm primum pro C0/C1. */
    case 0xc0: case 0xc1: case 0xd0: case 0xd1:
    case 0xd2: case 0xd3:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om = (op == 0xc0 || op == 0xd0 || op == 0xd2) ? 1 : mag;
            unsigned cnt;
            if (op == 0xc0 || op == 0xc1) {
                cnt = lege_u8(mem, mac->rip);
                mac->rip++;
            } else if (op == 0xd0 || op == 0xd1) {
                cnt = 1;
            } else {
                cnt = (unsigned)cap_lege8(mac, CRCX, r.rex != 0);
            }
            uint64_t v = lege_rm(mac, mem, &r, om);
            v = shift_op(mac, r.reg, v, cnt, om);
            scribe_rm(mac, mem, &r, om, v);
        }
        return;

    /* RET near (C3) / RET imm16 (C2) */
    case 0xc2: case 0xc3:
        {
            uint16_t imm = 0;
            if (op == 0xc2) {
                imm = lege_u16(mem, mac->rip);
                mac->rip += 2;
            }
            mac->rip = protrude(mac, mem, 8);
            mac->cap[CRSP] += imm;
        }
        return;

    /* MOV r/m, imm (C6/C7). Imm primum. */
    case 0xc6: case 0xc7:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om = (op == 0xc6) ? 1 : mag;
            uint64_t v;
            if (om == 1) {
                v = lege_u8(mem, mac->rip);
                mac->rip++;
            } else if (om == 2) {
                v = lege_u16(mem, mac->rip);
                mac->rip += 2;
            } else {
                int32_t t = (int32_t)lege_u32(mem, mac->rip);
                mac->rip += 4;
                v = (om == 8) ? (uint64_t)(int64_t)t : (uint64_t)(uint32_t)t; }
            scribe_rm(mac, mem, &r, om, v);
        }
        return;

    /* LEAVE (C9) */
    case 0xc9:
        mac->cap[CRSP] = mac->cap[CRBP];
        mac->cap[CRBP] = protrude(mac, mem, 8);
        return;

    /* INT 3 (CC) / INT imm (CD) */
    case 0xcc:
        culpa("INT3 invocata (rip=0x%lx)", (unsigned long)rip_init);
    case 0xcd:
        culpa("INT imm invocata, non sustinetur");

    /* JMP rel8 (EB) */
    case 0xeb:
        {
            int8_t rel = (int8_t)lege_u8(mem, mac->rip);
            mac->rip++;
            mac->rip += (int64_t)rel;
        }
        return;
    /* JMP rel32 (E9) */
    case 0xe9:
        {
            int32_t rel = (int32_t)lege_u32(mem, mac->rip);
            mac->rip += 4;
            mac->rip += (int64_t)rel;
        }
        return;
    /* CALL rel32 (E8) */
    case 0xe8:
        {
            int32_t rel = (int32_t)lege_u32(mem, mac->rip);
            mac->rip += 4;
            trude(mac, mem, mac->rip, 8);
            mac->rip += (int64_t)rel;
        }
        return;

    /* HLT (F4) */
    case 0xf4:
        culpa("HLT invocata");

    /* CMC/CLC/STC */
    case 0xf5:
        mac->vexilla ^= VCF;
        return;
    case 0xf8:
        mac->vexilla &= ~VCF;
        return;
    case 0xf9:
        mac->vexilla |=  VCF;
        return;
    /* CLD/STD */
    case 0xfc:
        mac->vexilla &= ~VDF;
        return;
    case 0xfd:
        mac->vexilla |=  VDF;
        return;

    /* Grp 3: F6/F7 — /0 TEST, /2 NOT, /3 NEG, /4 MUL, /5 IMUL, /6 DIV, /7 IDIV.
     * Pro TEST (/0 /1) imm primum legere debemus. */
    case 0xf6: case 0xf7:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om  = (op == 0xf6) ? 1 : mag;
            unsigned sub = r.reg;
            /* TEST imm: imm primum. */
            if (sub == 0 || sub == 1) {
                uint64_t b;
                if (om == 1) {
                    b = lege_u8(mem, mac->rip);
                    mac->rip++;
                } else if (om == 2) {
                    b = lege_u16(mem, mac->rip);
                    mac->rip += 2;
                } else {
                    int32_t t = (int32_t)lege_u32(mem, mac->rip);
                    mac->rip += 4;
                    b = (om == 8) ? (uint64_t)(int64_t)t : (uint64_t)(uint32_t)t; }
                uint64_t vv = lege_rm(mac, mem, &r, om);
                vex_logica(mac, vv & b, om);
                return;
            }
            uint64_t v = lege_rm(mac, mem, &r, om);
            switch (sub) {
            case 0: case 1: {
                /* non attingitur — iam tractatum supra. */
                    (void)v;
                } return;
            case 2:
                v = ~v & masca(om);
                scribe_rm(mac, mem, &r, om, v);
                return;
            case 3: {
                    uint64_t a   = 0;
                    uint64_t res = (a - v) & masca(om);
                    vex_sub(mac, a, v, res, om);
                    scribe_rm(mac, mem, &r, om, res);
                } return;
            case 4: { /* MUL */
                    if (om == 1) {
                        uint64_t a = cap_lege8(mac, CRAX, r.rex != 0);
                        uint64_t p = a * v;
                        cap_scribe16(mac, CRAX, (uint16_t)p);
                        mac->vexilla &= ~(VCF | VOF);
                        if (p >> 8)
                            mac->vexilla |= VCF | VOF;
                    } else if (om == 2) {
                        uint32_t p = (uint32_t)(uint16_t)mac->cap[CRAX] * (uint32_t)(uint16_t)v;
                        cap_scribe16(mac, CRAX, (uint16_t)p);
                        cap_scribe16(mac, CRDX, (uint16_t)(p >> 16));
                        mac->vexilla &= ~(VCF | VOF);
                        if (p >> 16)
                            mac->vexilla |= VCF | VOF;
                    } else if (om == 4) {
                        uint64_t p     = (uint64_t)(uint32_t)mac->cap[CRAX] * (uint64_t)(uint32_t)v;
                        mac->cap[CRAX] = (uint32_t)p;
                        mac->cap[CRDX] = (uint32_t)(p >> 32);
                        mac->vexilla &= ~(VCF | VOF);
                        if (p >> 32)
                            mac->vexilla |= VCF | VOF;
                    } else {
                        uint64_t plo, phi;
                        mul64_128u(mac->cap[CRAX], v, &phi, &plo);
                        mac->cap[CRAX] = plo;
                        mac->cap[CRDX] = phi;
                        mac->vexilla &= ~(VCF | VOF);
                        if (phi)
                            mac->vexilla |= VCF | VOF;
                    }
                } return;
            case 5: { /* IMUL single-op */
                    if (om == 8) {
                        int64_t  phi;
                        uint64_t plo;
                        mul64_128s((int64_t)mac->cap[CRAX], (int64_t)v, &phi, &plo);
                        mac->cap[CRAX] = plo;
                        mac->cap[CRDX] = (uint64_t)phi;
                        mac->vexilla &= ~(VCF | VOF);
                        if (!imul_capit_64(phi, plo))
                            mac->vexilla |= VCF | VOF;
                    } else if (om == 4) {
                        int64_t p      = (int64_t)(int32_t)mac->cap[CRAX] * (int64_t)(int32_t)v;
                        mac->cap[CRAX] = (uint32_t)p;
                        mac->cap[CRDX] = (uint32_t)(p >> 32);
                        mac->vexilla &= ~(VCF | VOF);
                        if ((int32_t)p != p)
                            mac->vexilla |= VCF | VOF;
                    } else if (om == 2) {
                        int32_t p = (int32_t)(int16_t)mac->cap[CRAX] * (int32_t)(int16_t)v;
                        cap_scribe16(mac, CRAX, (uint16_t)p);
                        cap_scribe16(mac, CRDX, (uint16_t)(p >> 16));
                        mac->vexilla &= ~(VCF | VOF);
                        if ((int16_t)p != p)
                            mac->vexilla |= VCF | VOF;
                    } else {
                        int16_t p = (int16_t)(int8_t)cap_lege8(mac, CRAX, r.rex != 0) * (int16_t)(int8_t)v;
                        cap_scribe16(mac, CRAX, (uint16_t)p);
                        mac->vexilla &= ~(VCF | VOF);
                        if ((int8_t)p != p)
                            mac->vexilla |= VCF | VOF;
                    }
                } return;
            case 6: { /* DIV */
                    if (v == 0)
                        culpa("divisio per zero");
                    if (om == 8) {
                        uint64_t q, rr;
                        if (!udiv_128_64(mac->cap[CRDX], mac->cap[CRAX], v, &q, &rr))
                            culpa("divisio superat 64 bit");
                        mac->cap[CRAX] = q;
                        mac->cap[CRDX] = rr;
                    } else if (om == 4) {
                        uint64_t n = ((uint64_t)(uint32_t)mac->cap[CRDX] << 32) | (uint32_t)mac->cap[CRAX];
                        uint64_t q = n / v, rr = n % v;
                        if (q > 0xffffffffull)
                            culpa("divisio superat 32 bit");
                        mac->cap[CRAX] = (uint32_t)q;
                        mac->cap[CRDX] = (uint32_t)rr;
                    } else if (om == 2) {
                        uint32_t n = ((uint32_t)(uint16_t)mac->cap[CRDX] << 16) | (uint16_t)mac->cap[CRAX];
                        uint32_t q = n / v, rr = n % v;
                        if (q > 0xffffu)
                            culpa("divisio superat 16 bit");
                        cap_scribe16(mac, CRAX, (uint16_t)q);
                        cap_scribe16(mac, CRDX, (uint16_t)rr);
                    } else {
                        uint16_t n = (uint16_t)mac->cap[CRAX];
                        uint32_t q = n / v, rr = n % v;
                        if (q > 0xffu)
                            culpa("divisio superat 8 bit");
                        cap_scribe8(mac, CRAX, (uint8_t)q, r.rex != 0);
                        cap_scribe8(mac, CRAX+4, (uint8_t)rr, r.rex != 0);
                    }
                } return;
            case 7: { /* IDIV */
                    if (v == 0)
                        culpa("divisio per zero");
                    if (om == 8) {
                        int64_t q, rr;
                        if (!sdiv_128_64((int64_t)mac->cap[CRDX], mac->cap[CRAX], (int64_t)v, &q, &rr))
                            culpa("divisio superat 64 bit");
                        mac->cap[CRAX] = (uint64_t)q;
                        mac->cap[CRDX] = (uint64_t)rr;
                    } else if (om == 4) {
                        int64_t n      = ((int64_t)(int32_t)mac->cap[CRDX] << 32) | (uint32_t)mac->cap[CRAX];
                        int64_t q      = n / (int32_t)v, rr = n % (int32_t)v;
                        mac->cap[CRAX] = (uint32_t)q;
                        mac->cap[CRDX] = (uint32_t)rr;
                    } else if (om == 2) {
                        int32_t n = ((int32_t)(int16_t)mac->cap[CRDX] << 16) | (uint16_t)mac->cap[CRAX];
                        int32_t q = n / (int16_t)v, rr = n % (int16_t)v;
                        cap_scribe16(mac, CRAX, (uint16_t)q);
                        cap_scribe16(mac, CRDX, (uint16_t)rr);
                    } else {
                        int16_t n = (int16_t)(uint16_t)mac->cap[CRAX];
                        int16_t q = n / (int8_t)v, rr = n % (int8_t)v;
                        cap_scribe8(mac, CRAX, (uint8_t)q, r.rex != 0);
                        cap_scribe8(mac, CRAX+4, (uint8_t)rr, r.rex != 0);
                    }
                } return;
            default:
                culpa("grp3 subopcode /%u non sustinetur", sub);
            }
        }

    /* Grp 4/5: FE/FF */
    case 0xfe: case 0xff:
        resolve_modrm(mem, &mac->rip, &r);
        {
            unsigned om  = (op == 0xfe) ? 1 : mag;
            unsigned sub = r.reg;
            switch (sub) {
            case 0: { /* INC */
                    uint64_t a   = lege_rm(mac, mem, &r, om);
                    uint64_t res = (a + 1) & masca(om);
                    uint32_t kcf = mac->vexilla & VCF;
                    vex_add(mac, a, 1, res, om);
                    mac->vexilla = (mac->vexilla & ~VCF) | kcf;
                    scribe_rm(mac, mem, &r, om, res);
                } return;
            case 1: { /* DEC */
                    uint64_t a   = lege_rm(mac, mem, &r, om);
                    uint64_t res = (a - 1) & masca(om);
                    uint32_t kcf = mac->vexilla & VCF;
                    vex_sub(mac, a, 1, res, om);
                    mac->vexilla = (mac->vexilla & ~VCF) | kcf;
                    scribe_rm(mac, mem, &r, om, res);
                } return;
            case 2: { /* CALL r/m */
                    uint64_t tgt = lege_rm(mac, mem, &r, 8);
                    trude(mac, mem, mac->rip, 8);
                    mac->rip = tgt;
                } return;
            case 4: { /* JMP r/m */
                    uint64_t tgt = lege_rm(mac, mem, &r, 8);
                    mac->rip     = tgt;
                } return;
            case 6: { /* PUSH r/m */
                    uint64_t v = lege_rm(mac, mem, &r, 8);
                    trude(mac, mem, v, 8);
                } return;
            default:
                culpa("grp5 /%u non sustinetur", sub);
            }
        }

    /* PUSHF / POPF */
    case 0x9c:
        trude(mac, mem, mac->vexilla, 8);
        return;
    case 0x9d:
        mac->vexilla = (uint32_t)protrude(mac, mem, 8);
        return;

    /* Hex 0x8f /0 = POP r/m */
    case 0x8f:
        resolve_modrm(mem, &mac->rip, &r);
        if (r.reg != 0)
            culpa("0x8f sub /%u non valet", r.reg);
        {
            uint64_t v = protrude(mac, mem, 8);
            scribe_rm(mac, mem, &r, 8, v);
        }
        return;

    default:
        culpa("opcode %02x non sustinetur (rip=0x%lx)", op, (unsigned long)rip_init);
    }
}

/* ======================================================================== *
 * XVII.  Compositio pilae initialis + aux vector.
 * ======================================================================== */

static uint64_t
construe_pilam(Memoria *mem, int argc, char **argv, char **envp)
{
    /* Pila in summo spatii. Margini PILA_MAG. */
    uint64_t sp_top = mem->limen_superum - PAGINA;  /* ne in ipso limen scribimus. */
    uint64_t sp     = sp_top;

    /* 1) Pono stringas argv et envp in summo, deorsum. */
    int envc = 0;
    while (envp[envc])
        envc++;
    uint64_t *argv_va = calloc((size_t)argc + 1, sizeof(uint64_t));
    uint64_t *envp_va = calloc((size_t)envc + 1, sizeof(uint64_t));

    for (int i = argc - 1; i >= 0; i--) {
        size_t n = strlen(argv[i]) + 1;
        sp -= n;
        scribe_mem(mem, sp, argv[i], n);
        argv_va[i] = sp;
    }
    for (int i = envc - 1; i >= 0; i--) {
        size_t n = strlen(envp[i]) + 1;
        sp -= n;
        scribe_mem(mem, sp, envp[i], n);
        envp_va[i] = sp;
    }

    /* 2) Pono 16 octetos aleatorios pro AT_RANDOM. */
    uint8_t alea[16];
    {
        FILE *f = fopen("/dev/urandom", "rb");
        if (f) {
            size_t rd = fread(alea, 1, 16, f);
            if (rd != 16)
                memset(alea, 0x5a, 16);
            fclose(f);
        } else {
            memset(alea, 0x5a, 16);
        }
    }
    sp -= 16;
    scribe_mem(mem, sp, alea, 16);
    uint64_t random_va = sp;

    /* 3) Aligna ad 16-octet, cum ratione: post omnia pila debet esse
     * 16-aligned (entry ad function boundary). Computemus magnitudinem
     * reliquorum et aligamus sp accordingly. */
    /* size = 8 (argc) + 8*(argc+1) + 8*(envc+1) + 8*2*aux_pairs */
    int aux_pairs = 10;
    size_t tota   = 8 /* argc */ + 8ull*(argc+1) + 8ull*(envc+1) + 16ull*aux_pairs;
    sp &= ~(uint64_t)0xf;
    if ((sp - tota) & 0xf)
        sp -= 8;

    /* 4) Aux vector. */
    sp -= 16;
    scribe_u64(mem, sp, AT_NULL);
    scribe_u64(mem, sp+8, 0);
    sp -= 16;
    scribe_u64(mem, sp, AT_SECURE);
    scribe_u64(mem, sp+8, 0);
    sp -= 16;
    scribe_u64(mem, sp, AT_RANDOM);
    scribe_u64(mem, sp+8, random_va);
    sp -= 16;
    scribe_u64(mem, sp, AT_EGID);
    scribe_u64(mem, sp+8, (uint64_t)getegid());
    sp -= 16;
    scribe_u64(mem, sp, AT_GID);
    scribe_u64(mem, sp+8, (uint64_t)getgid());
    sp -= 16;
    scribe_u64(mem, sp, AT_EUID);
    scribe_u64(mem, sp+8, (uint64_t)geteuid());
    sp -= 16;
    scribe_u64(mem, sp, AT_UID);
    scribe_u64(mem, sp+8, (uint64_t)getuid());
    sp -= 16;
    scribe_u64(mem, sp, AT_PAGESZ);
    scribe_u64(mem, sp+8, PAGINA);
    sp -= 16;
    scribe_u64(mem, sp, AT_ENTRY);
    scribe_u64(mem, sp+8, mem->entry);
    sp -= 16;
    scribe_u64(mem, sp, AT_PHDR);
    scribe_u64(mem, sp+8, mem->phdr_va);

    /* 5) envp (NULL-terminated). */
    sp -= 8;
    scribe_u64(mem, sp, 0);
    for (int i = envc - 1; i >= 0; i--) {
        sp -= 8;
        scribe_u64(mem, sp, envp_va[i]);
    }

    /* 6) argv (NULL-terminated). */
    sp -= 8;
    scribe_u64(mem, sp, 0);
    for (int i = argc - 1; i >= 0; i--) {
        sp -= 8;
        scribe_u64(mem, sp, argv_va[i]);
    }

    /* 7) argc. */
    sp -= 8;
    scribe_u64(mem, sp, (uint64_t)argc);

    free(argv_va);
    free(envp_va);

    /* mmap cursor initializatur intra pilam inferiorem. */
    mmap_cursor = ((sp - PILA_MAG) & ~(uint64_t)(PAGINA - 1)) - (4ull << 20);
    if (mmap_cursor < mem->terminus + PAGINA)
        mmap_cursor = mem->terminus + PAGINA;
    return sp;
}

/* ======================================================================== *
 * XVIII.  Principalis.
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
    onera_elf(argv[i], &mem);

    Machina mac = mm_init();
    mac.rip     = mem.entry;

    /* Pila. */
    mac.cap[CRSP] = construe_pilam(&mem, argc - i, argv + i, environ);
    nuntio("entry=0x%lx rsp=0x%lx", (unsigned long)mac.rip, (unsigned long)mac.cap[CRSP]);

    exsequi(&mac, &mem);
    return 0;
}
