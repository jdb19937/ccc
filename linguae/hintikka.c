/*
 * hintikka.c — Logica Epistemica Multi-Agentium
 *
 * Extensio Kripkiana cum operatoribus epistemicis per quosque agentes:
 * K_a phi = "agens a scit phi", B_a phi = "agens a credit phi".
 * Unusquisque agens habet suam relationem accessibilitatis in eisdem
 * mundis; scientia est veritas in omnibus mundis accessibilibus
 * ex mundo actuali perspectiva huius agentis.
 *
 * Compilatorem vexat per: struct recursivum cum plurium arietum indicibus,
 * indicatores functionum operatorum in structuris, varargs pro
 * diagnostica, retrovocatio ex functione ad se ipsam.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define MAX_MUNDORUM   8
#define MAX_AGENTIUM   4
#define MAX_ATOMI      8
#define MAX_FORMULAE   512

/* ===== Structura Hintikkiana ===== */

typedef struct {
    int n_mundi;
    int n_agentium;
    const char *nomina_agentium[MAX_AGENTIUM];
    /* R[a][i][j] = 1 si agens a potest accedere ex mundo i ad mundum j */
    int accessus[MAX_AGENTIUM][MAX_MUNDORUM][MAX_MUNDORUM];
    int n_atomi;
    char nomina_atom[MAX_ATOMI];
    int V[MAX_MUNDORUM][MAX_ATOMI];
} Hintikka;

/* ===== Formula recursiva ===== */

typedef enum {
    FH_ATOM, FH_NON, FH_ET, FH_VEL, FH_IMPL,
    FH_SCIT,     /* K_a phi */
    FH_CREDIT,   /* B_a phi (eandem semantica hic) */
    FH_COMMUNE,  /* C phi — scientia communis (simplificata) */
} GenusFh;

typedef struct Fh {
    GenusFh genus;
    char atom;
    int  agens;  /* index agentis pro FH_SCIT/CREDIT */
    struct Fh *sin, *dex;
} Fh;

static Fh receptaculum[MAX_FORMULAE];
static int n_receptaculum = 0;

static Fh *
novus(GenusFh g)
{
    if (n_receptaculum >= MAX_FORMULAE) {
        fprintf(stderr, "Error: spatium formularum exhaustum\n");
        exit(1);
    }
    Fh *f = &receptaculum[n_receptaculum++];
    f->genus = g;
    f->atom = 0;
    f->agens = 0;
    f->sin = f->dex = NULL;
    return f;
}

/* ===== Parsor: K<n>phi, B<n>phi, ! & | > atom ===== */

static Fh *
parsa(const char **p)
{
    while (**p == ' ') (*p)++;
    char c = **p;
    if (!c) return NULL;
    (*p)++;
    switch (c) {
    case '!': { Fh *f = novus(FH_NON); f->sin = parsa(p); return f; }
    case '&': { Fh *f = novus(FH_ET); f->sin = parsa(p); f->dex = parsa(p); return f; }
    case '|': { Fh *f = novus(FH_VEL); f->sin = parsa(p); f->dex = parsa(p); return f; }
    case '>': { Fh *f = novus(FH_IMPL); f->sin = parsa(p); f->dex = parsa(p); return f; }
    case 'K': case 'B': {
        Fh *f = novus(c == 'K' ? FH_SCIT : FH_CREDIT);
        while (**p == ' ') (*p)++;
        if (isdigit((unsigned char)**p))
            f->agens = *(*p)++ - '0';
        f->sin = parsa(p);
        return f;
    }
    case 'C': { Fh *f = novus(FH_COMMUNE); f->sin = parsa(p); return f; }
    default:
        if (isalpha((unsigned char)c)) {
            Fh *f = novus(FH_ATOM);
            f->atom = c;
            return f;
        }
        return NULL;
    }
}

/* ===== Satisfactionis ===== */

static int
atom_idx(const Hintikka *h, char a)
{
    for (int i = 0; i < h->n_atomi; i++)
        if (h->nomina_atom[i] == a) return i;
    return -1;
}

static int satisfit(const Hintikka *h, int mundus, const Fh *f);

static int
scit_per(const Hintikka *h, int agens, int mundus, const Fh *phi)
{
    if (agens < 0 || agens >= h->n_agentium) return 0;
    for (int m = 0; m < h->n_mundi; m++)
        if (h->accessus[agens][mundus][m] && !satisfit(h, m, phi))
            return 0;
    return 1;
}

/*
 * Scientia communis (simplificata): in omnibus mundis ad quos
 * aliquis agens potest accedere, phi tenet.
 */
static int
scientia_communis(const Hintikka *h, int mundus, const Fh *phi)
{
    /* BFS super coniunctionem omnium relationum */
    int visitatum[MAX_MUNDORUM] = { 0 };
    int coda[MAX_MUNDORUM];
    int initium = 0, finis = 0;
    coda[finis++] = mundus;
    visitatum[mundus] = 1;
    while (initium < finis) {
        int w = coda[initium++];
        if (!satisfit(h, w, phi)) return 0;
        for (int a = 0; a < h->n_agentium; a++)
            for (int m = 0; m < h->n_mundi; m++)
                if (h->accessus[a][w][m] && !visitatum[m]) {
                    visitatum[m] = 1;
                    coda[finis++] = m;
                }
    }
    return 1;
}

static int
satisfit(const Hintikka *h, int mundus, const Fh *f)
{
    if (!f) return 0;
    switch (f->genus) {
    case FH_ATOM: {
        int i = atom_idx(h, f->atom);
        return i >= 0 ? h->V[mundus][i] : 0;
    }
    case FH_NON:     return !satisfit(h, mundus, f->sin);
    case FH_ET:      return  satisfit(h, mundus, f->sin) && satisfit(h, mundus, f->dex);
    case FH_VEL:     return  satisfit(h, mundus, f->sin) || satisfit(h, mundus, f->dex);
    case FH_IMPL:    return !satisfit(h, mundus, f->sin) || satisfit(h, mundus, f->dex);
    case FH_SCIT:
    case FH_CREDIT:  return  scit_per(h, f->agens, mundus, f->sin);
    case FH_COMMUNE: return  scientia_communis(h,  mundus, f->sin);
    }
    return 0;
}

/* ===== Diagnostica per variadic ===== */

static void
dic(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

/* ===== Exemplum: scenarium trium filosophorum ===== */

/*
 * Tres filosophi (0, 1, 2) in conventu.  Atom 'p' = "filosophus 0
 * habet cartam albam in fronte."  Structura: in omnibus mundis
 * quisque filosophus videt fontes aliorum sed non suum.
 */
static void
inicializa_exemplum(Hintikka *h)
{
    h->n_mundi = 2;
    h->n_agentium = 3;
    h->nomina_agentium[0] = "Socrates";
    h->nomina_agentium[1] = "Plato";
    h->nomina_agentium[2] = "Aristoteles";
    h->n_atomi = 1;
    h->nomina_atom[0] = 'p';
    memset(h->accessus, 0, sizeof(h->accessus));
    memset(h->V, 0, sizeof(h->V));
    /*
     * Mundus 0: Socrates habet cartam albam (p est verum).
     * Mundus 1: Socrates non habet cartam albam (p est falsum).
     * Socrates non potest distinguere inter mundos (non videt se ipsum).
     * Plato et Aristoteles possunt distinguere (vident Socraten).
     */
    h->V[0][0] = 1; /* p verum in w0 */
    h->V[1][0] = 0; /* p falsum in w1 */

    /* Socrates: w0 -- w1 (non distinguit) */
    h->accessus[0][0][0] = 1;
    h->accessus[0][0][1] = 1;
    h->accessus[0][1][0] = 1;
    h->accessus[0][1][1] = 1;

    /* Plato: solum ad se — distinguit */
    h->accessus[1][0][0] = 1;
    h->accessus[1][1][1] = 1;

    /* Aristoteles: similis Platoni */
    h->accessus[2][0][0] = 1;
    h->accessus[2][1][1] = 1;
}

static void
monstra_structuram(const Hintikka *h)
{
    dic("Mundi: %d, agentes: %d", h->n_mundi, h->n_agentium);
    for (int a = 0; a < h->n_agentium; a++)
        dic("  [%d] %s", a, h->nomina_agentium[a]);
    dic("Valuationes:");
    for (int m = 0; m < h->n_mundi; m++) {
        printf("  w%d: ", m);
        for (int i = 0; i < h->n_atomi; i++)
            printf("%c=%d ", h->nomina_atom[i], h->V[m][i]);
        putchar('\n');
    }
    dic("Relatio accessibilitatis per agentem:");
    for (int a = 0; a < h->n_agentium; a++) {
        printf("  %s: ", h->nomina_agentium[a]);
        for (int i = 0; i < h->n_mundi; i++)
            for (int j = 0; j < h->n_mundi; j++)
                if (h->accessus[a][i][j])
                    printf("w%d->w%d ", i, j);
        putchar('\n');
    }
}

static const char *const exempla[] = {
    "p",         /* p est verum */
    "K0p",       /* Socrates scit p */
    "K1p",       /* Plato scit p */
    "K2p",       /* Aristoteles scit p */
    "!K0p",      /* Socrates non scit p */
    "K1K0p",     /* Plato scit quod Socrates scit p */
    "K1!K0p",    /* Plato scit quod Socrates non scit p */
    "&K1pK2p",   /* Plato et Aristoteles ambo sciunt p */
    "Cp",        /* scientia communis p */
    NULL,
};

int
main(int argc, char *argv[])
{
    Hintikka h;
    inicializa_exemplum(&h);

    printf("Logica Epistemica Hintikkiana\n");
    printf("=============================\n");
    monstra_structuram(&h);
    putchar('\n');

    if (argc > 1) {
        const char *p = argv[1];
        n_receptaculum = 0;
        Fh *f = parsa(&p);
        if (!f) { fprintf(stderr, "Error parsa\n"); return 1; }
        for (int m = 0; m < h.n_mundi; m++)
            printf("  w%d: %s\n", m, satisfit(&h, m, f) ? "VERUM" : "FALSUM");
    } else {
        for (int i = 0; exempla[i]; i++) {
            const char *p = exempla[i];
            n_receptaculum = 0;
            Fh *f = parsa(&p);
            printf("  %-10s ", exempla[i]);
            for (int m = 0; m < h.n_mundi; m++)
                printf(" w%d=%c", m, satisfit(&h, m, f) ? 'T' : 'F');
            putchar('\n');
        }
        printf("\nUsus: hintikka 'formula'  (K0..K3, B0..B3, C, ! & | > atom)\n");
    }
    return 0;
}
