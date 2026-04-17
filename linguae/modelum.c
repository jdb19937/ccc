/*
 * modelum.c — Verificator Modelorum Logicae Primi Ordinis
 *
 * Definit modelum M = (D, I) ubi D est dominium (copia elementorum)
 * et I interpretatio relatiorum et functionum.  Parsa formulam
 * quantificatam primi ordinis et verificat satisfactionem M |= phi
 * per recursionem super structuram formulae et enumerationem
 * assignationum variabilium.
 *
 * Vexat compilatorem per: uniones discriminatas per genus formulae,
 * recursionem profundam super arborem structurarum, VLA pro contextu variabilium,
 * setjmp/longjmp pro erroribus parsae, tabulam indicatorum functionum
 * pro operatoribus logicis.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <setjmp.h>

#define MAX_DOM       8
#define MAX_VARS     16
#define MAX_RELATIO  16
#define MAX_NODI     512

/* ===== Modelum ===== */

typedef struct {
    int n_dom;                           /* |D| */
    int n_relatio;
    char nomina_relatio[MAX_RELATIO];    /* 'P', 'Q', 'R' etc. */
    int  aritas[MAX_RELATIO];            /* 1 = monadica, 2 = dyadica */
    /* interpretatio: I[r][a][b] = 1 si R(a, b) tenet (pro dyadicis).
     * Pro monadicis, I[r][a][0] utimur. */
    int interp[MAX_RELATIO][MAX_DOM][MAX_DOM];
} Modelum;

/* ===== Formula arborea ===== */

typedef enum {
    FM_ATOM, FM_NON, FM_ET, FM_VEL, FM_IMPL,
    FM_EXISTIT, FM_OMNIS,
} GenusFm;

typedef struct Fm {
    GenusFm genus;
    char relatio;                   /* pro FM_ATOM */
    char args[2];                   /* variabiles: 'x', 'y', ... vel '0'-'7' pro constantibus */
    char var;                       /* pro FM_EXISTIT, FM_OMNIS */
    struct Fm *sin, *dex;
} Fm;

static Fm receptaculum[MAX_NODI];
static int n_receptaculum = 0;
static jmp_buf contextus_erroris;

static Fm *
novus_fm(GenusFm g)
{
    if (n_receptaculum >= MAX_NODI) longjmp(contextus_erroris, 1);
    Fm *f = &receptaculum[n_receptaculum++];
    f->genus = g;
    f->relatio = 0;
    f->args[0] = f->args[1] = 0;
    f->var = 0;
    f->sin = f->dex = NULL;
    return f;
}

/* ===== Parsor prefix ===== */

/*
 * Syntaxis: atomica ut "Pxy" (maior littera = relatio, sequentes
 * minores = variabiles).  Connectiva: !, &, |, >.  Quantificatores:
 * Exphi (existit), Axphi (omnis), cum x variabilis.
 */
static Fm *
parsa_fm(const char **p)
{
    while (**p == ' ') (*p)++;
    char c = **p;
    if (!c) longjmp(contextus_erroris, 2);
    (*p)++;
    switch (c) {
    case '!': { Fm *f = novus_fm(FM_NON); f->sin = parsa_fm(p); return f; }
    case '&': { Fm *f = novus_fm(FM_ET); f->sin = parsa_fm(p); f->dex = parsa_fm(p); return f; }
    case '|': { Fm *f = novus_fm(FM_VEL); f->sin = parsa_fm(p); f->dex = parsa_fm(p); return f; }
    case '>': { Fm *f = novus_fm(FM_IMPL); f->sin = parsa_fm(p); f->dex = parsa_fm(p); return f; }
    case 'E': case 'A': {
        Fm *f = novus_fm(c == 'E' ? FM_EXISTIT : FM_OMNIS);
        while (**p == ' ') (*p)++;
        if (!islower((unsigned char)**p)) longjmp(contextus_erroris, 3);
        f->var = *(*p)++;
        f->sin = parsa_fm(p);
        return f;
    }
    default:
        if (isupper((unsigned char)c)) {
            Fm *f = novus_fm(FM_ATOM);
            f->relatio = c;
            f->args[0] = *(*p)++;
            /* dyadica possibilis */
            if (islower((unsigned char)**p) || isdigit((unsigned char)**p))
                f->args[1] = *(*p)++;
            return f;
        }
        longjmp(contextus_erroris, 4);
    }
}

/* ===== Assignatio variabilium ===== */

typedef struct {
    char vars[MAX_VARS];
    int vals[MAX_VARS];
    int n;
} Assignatio;

static int
valor_termi(char t, const Assignatio *a)
{
    if (isdigit((unsigned char)t))
        return t - '0';
    for (int i = 0; i < a->n; i++)
        if (a->vars[i] == t) return a->vals[i];
    return -1;
}

/* ===== Verificator ===== */

static int
relatio_idx(const Modelum *m, char r)
{
    for (int i = 0; i < m->n_relatio; i++)
        if (m->nomina_relatio[i] == r) return i;
    return -1;
}

static int
satisfit_fm(const Modelum *m, const Fm *f, Assignatio *a)
{
    switch (f->genus) {
    case FM_ATOM: {
        int ri = relatio_idx(m, f->relatio);
        if (ri < 0) return 0;
        int v0 = valor_termi(f->args[0], a);
        if (v0 < 0 || v0 >= m->n_dom) return 0;
        if (m->aritas[ri] == 1)
            return m->interp[ri][v0][0];
        int v1 = valor_termi(f->args[1], a);
        if (v1 < 0 || v1 >= m->n_dom) return 0;
        return m->interp[ri][v0][v1];
    }
    case FM_NON:  return !satisfit_fm(m, f->sin, a);
    case FM_ET:   return satisfit_fm(m, f->sin, a) && satisfit_fm(m, f->dex, a);
    case FM_VEL:  return satisfit_fm(m, f->sin, a) || satisfit_fm(m, f->dex, a);
    case FM_IMPL: return !satisfit_fm(m, f->sin, a) || satisfit_fm(m, f->dex, a);
    case FM_EXISTIT: {
        a->vars[a->n] = f->var;
        for (int v = 0; v < m->n_dom; v++) {
            a->vals[a->n] = v;
            a->n++;
            if (satisfit_fm(m, f->sin, a)) {
                a->n--;
                return 1;
            }
            a->n--;
        }
        return 0;
    }
    case FM_OMNIS: {
        a->vars[a->n] = f->var;
        for (int v = 0; v < m->n_dom; v++) {
            a->vals[a->n] = v;
            a->n++;
            if (!satisfit_fm(m, f->sin, a)) {
                a->n--;
                return 0;
            }
            a->n--;
        }
        return 1;
    }
    }
    return 0;
}

/* ===== Modelum exemplare ===== */

/*
 * Dominium D = {0, 1, 2, 3}.
 * P (monadica): "est par" — P(0), P(2)
 * L (dyadica): "minor quam" — L(i,j) si i < j
 * Q (dyadica): "aequalis" — Q(i,j) si i == j
 */
static void
inicializa_modelum(Modelum *m)
{
    m->n_dom = 4;
    m->n_relatio = 3;
    m->nomina_relatio[0] = 'P'; m->aritas[0] = 1;
    m->nomina_relatio[1] = 'L'; m->aritas[1] = 2;
    m->nomina_relatio[2] = 'Q'; m->aritas[2] = 2;
    memset(m->interp, 0, sizeof(m->interp));
    /* P: parum */
    m->interp[0][0][0] = 1;
    m->interp[0][2][0] = 1;
    /* L: minor */
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            if (i < j) m->interp[1][i][j] = 1;
    /* Q: aequalis */
    for (int i = 0; i < 4; i++)
        m->interp[2][i][i] = 1;
}

static const char *const exempla_formularum[] = {
    "P0",            /* 0 est par */
    "P1",            /* 1 est par */
    "L02",           /* 0 < 2 */
    "AxEyLxy",       /* omnis x existit y tali ut x<y */
    "ExAyLxy",       /* existit x tali ut omnis y, x<y */
    "AxQxx",         /* omnis x: x=x (reflexivitas) */
    "Ax>PxEyLxy",    /* omnis par habet maiorem */
    NULL,
};

int
main(int argc, char *argv[])
{
    Modelum m;
    inicializa_modelum(&m);

    printf("Verificator Modelorum FOL\n");
    printf("=========================\n");
    printf("Dominium: {0, 1, 2, 3}\n");
    printf("P(x) = 'x est par';  L(x,y) = 'x < y';  Q(x,y) = 'x = y'\n");

    if (setjmp(contextus_erroris)) {
        fprintf(stderr, "Error in parsa vel verificatione\n");
        return 1;
    }

    if (argc > 1) {
        const char *p = argv[1];
        n_receptaculum = 0;
        Fm *f = parsa_fm(&p);
        Assignatio a = { .n = 0 };
        int r = satisfit_fm(&m, f, &a);
        printf("\nFormula: %s\n", argv[1]);
        printf("Verificatio: %s\n", r ? "VERUM in M" : "FALSUM in M");
    } else {
        printf("\n");
        for (int i = 0; exempla_formularum[i]; i++) {
            const char *p = exempla_formularum[i];
            n_receptaculum = 0;
            Fm *f = parsa_fm(&p);
            Assignatio a = { .n = 0 };
            int r = satisfit_fm(&m, f, &a);
            printf("  %-15s %s\n", exempla_formularum[i],
                   r ? "VERUM" : "FALSUM");
        }
        printf("\nUsus: modelum 'formula'  (P, L, Q; ! & | >; Ex, Ax)\n");
    }
    return 0;
}
