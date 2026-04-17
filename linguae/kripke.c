/*
 * kripke.c — Semantica Modalis per Structuras Kripkianas
 *
 * Evalua propositiones modales K = <W, R, V> ubi W est copia
 * mundorum possibilium, R relatio accessibilitatis, et V functio
 * valuationis propositionum atomicarum in mundis.  Operatores modales
 * sunt [] (necessarie — in omnibus mundis accessibilibus) et <>
 * (possibiliter — in aliquo mundo accessibili).
 *
 * Formulae dantur in notatione prefixa: 'p' atomica, '!' negatio,
 * '&' coniunctio, '|' disiunctio, '>' implicatio, '[' necessitas,
 * '<' possibilitas.  Monstrat satisfactionem per omnes mundos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_MUNDORUM   16
#define MAX_ATOMI      16
#define MAX_FORMULAE   256

/* ===== Structura Kripkiana ===== */

typedef struct {
    int n_mundi;
    int accessus[MAX_MUNDORUM][MAX_MUNDORUM]; /* R[i][j] = 1 si i -> j */
    char nomina_atom[MAX_ATOMI];
    int n_atom;
    int V[MAX_MUNDORUM][MAX_ATOMI]; /* V[mundus][atom] = 0 vel 1 */
} Kripke;

/* ===== Formula arborea ===== */

typedef enum {
    F_ATOM, F_NON, F_ET, F_VEL, F_IMPL, F_NECESSE, F_POSSIBILE
} GenusFormulae;

typedef struct Formula {
    GenusFormulae genus;
    char atom;                    /* pro F_ATOM */
    struct Formula *sin, *dex;    /* filii */
} Formula;

static Formula receptaculum_formularum[MAX_FORMULAE];
static int n_formulae = 0;

static Formula *
nova_formula(GenusFormulae g)
{
    if (n_formulae >= MAX_FORMULAE) {
        fprintf(stderr, "Error: nimis multae formulae\n");
        exit(1);
    }
    Formula *f = &receptaculum_formularum[n_formulae++];
    f->genus = g;
    f->atom = 0;
    f->sin = f->dex = NULL;
    return f;
}

/* ===== Parsor prefixae notationis ===== */

static Formula *parsa_rec(const char **p);

static Formula *
parsa_rec(const char **p)
{
    while (**p == ' ') (*p)++;
    char c = **p;
    if (!c) return NULL;
    (*p)++;
    switch (c) {
    case '!': {
        Formula *f = nova_formula(F_NON);
        f->sin = parsa_rec(p);
        return f;
    }
    case '&': {
        Formula *f = nova_formula(F_ET);
        f->sin = parsa_rec(p);
        f->dex = parsa_rec(p);
        return f;
    }
    case '|': {
        Formula *f = nova_formula(F_VEL);
        f->sin = parsa_rec(p);
        f->dex = parsa_rec(p);
        return f;
    }
    case '>': {
        Formula *f = nova_formula(F_IMPL);
        f->sin = parsa_rec(p);
        f->dex = parsa_rec(p);
        return f;
    }
    case '[': {
        Formula *f = nova_formula(F_NECESSE);
        f->sin = parsa_rec(p);
        return f;
    }
    case '<': {
        Formula *f = nova_formula(F_POSSIBILE);
        f->sin = parsa_rec(p);
        return f;
    }
    default:
        if (isalpha((unsigned char)c)) {
            Formula *f = nova_formula(F_ATOM);
            f->atom = c;
            return f;
        }
        fprintf(stderr, "Error: symbol ignotus '%c'\n", c);
        return NULL;
    }
}

/* ===== Satisfactionis relatio: M, w |= phi ===== */

static int atom_idx(const Kripke *k, char a);

static int
satisfit(const Kripke *k, int mundus, const Formula *f)
{
    if (!f) return 0;
    switch (f->genus) {
    case F_ATOM: {
        int idx = atom_idx(k, f->atom);
        if (idx < 0) return 0;
        return k->V[mundus][idx];
    }
    case F_NON:
        return !satisfit(k, mundus, f->sin);
    case F_ET:
        return satisfit(k, mundus, f->sin) && satisfit(k, mundus, f->dex);
    case F_VEL:
        return satisfit(k, mundus, f->sin) || satisfit(k, mundus, f->dex);
    case F_IMPL:
        return !satisfit(k, mundus, f->sin) || satisfit(k, mundus, f->dex);
    case F_NECESSE:
        /* in omnibus mundis accessibilibus */
        for (int m = 0; m < k->n_mundi; m++)
            if (k->accessus[mundus][m] && !satisfit(k, m, f->sin))
                return 0;
        return 1;
    case F_POSSIBILE:
        for (int m = 0; m < k->n_mundi; m++)
            if (k->accessus[mundus][m] && satisfit(k, m, f->sin))
                return 1;
        return 0;
    }
    return 0;
}

static int
atom_idx(const Kripke *k, char a)
{
    for (int i = 0; i < k->n_atom; i++)
        if (k->nomina_atom[i] == a) return i;
    return -1;
}

/* ===== Structura exempli ===== */

/*
 * Mundi: w0 (praesens), w1 (futurum possibile cum pluvia),
 *        w2 (futurum possibile sine pluvia).
 * Accessibilitas: w0 -> w1, w0 -> w2; w1 -> w1; w2 -> w2.
 * Atomi: p (pluit), s (sol lucet), u (umbracula aperta).
 */
static void
inicializa_exemplum(Kripke *k)
{
    k->n_mundi = 3;
    k->n_atom = 3;
    k->nomina_atom[0] = 'p';
    k->nomina_atom[1] = 's';
    k->nomina_atom[2] = 'u';
    memset(k->accessus, 0, sizeof(k->accessus));
    memset(k->V, 0, sizeof(k->V));
    /* w0 -> w1, w0 -> w2 */
    k->accessus[0][1] = 1;
    k->accessus[0][2] = 1;
    /* w1 -> w1 */
    k->accessus[1][1] = 1;
    /* w2 -> w2 */
    k->accessus[2][2] = 1;
    /* Valuationes: w1 habet p (pluit) et u (umbracula); w2 habet s (sol). */
    k->V[1][0] = 1;  /* p */
    k->V[1][2] = 1;  /* u */
    k->V[2][1] = 1;  /* s */
    /* w0 nihil habet */
}

static void
monstra_structuram(const Kripke *k)
{
    printf("Mundi: ");
    for (int i = 0; i < k->n_mundi; i++)
        printf("w%d ", i);
    putchar('\n');
    printf("Relatio accessus:\n");
    for (int i = 0; i < k->n_mundi; i++)
        for (int j = 0; j < k->n_mundi; j++)
            if (k->accessus[i][j])
                printf("  w%d -> w%d\n", i, j);
    printf("Valuationes atomicae:\n");
    for (int i = 0; i < k->n_mundi; i++) {
        printf("  w%d: ", i);
        int vacuum = 1;
        for (int a = 0; a < k->n_atom; a++)
            if (k->V[i][a]) {
                printf("%c ", k->nomina_atom[a]);
                vacuum = 0;
            }
        if (vacuum) printf("(nihil)");
        putchar('\n');
    }
}

/* ===== Formulae exemplares ===== */

static const char *const formulae_exemplares[] = {
    "p",            /* pluit */
    "[p",           /* necessarie pluit */
    "<p",           /* possibiliter pluit */
    "<s",           /* possibiliter sol lucet */
    "|ps",          /* pluit aut sol lucet */
    "<|ps",         /* possibiliter (pluit aut sol) */
    "[<p",          /* necessarie possibiliter pluit */
    ">pu",          /* pluit implicat umbracula */
    "[>pu",         /* necessarie (pluit implicat umbracula) */
    "&<p<s",        /* possibiliter pluit et possibiliter sol */
    NULL,
};

static const char *const descriptiones[] = {
    "pluit (p)",
    "necessarie pluit ([]p)",
    "possibiliter pluit (<>p)",
    "possibiliter sol lucet (<>s)",
    "pluit vel sol lucet (p v s)",
    "possibiliter (pluit vel sol) (<>(p v s))",
    "necessarie possibiliter pluit ([]<>p)",
    "pluit -> umbracula",
    "necessarie (pluit -> umbracula)",
    "possibiliter pluit et possibiliter sol lucet",
};

int
main(int argc, char *argv[])
{
    Kripke k;
    inicializa_exemplum(&k);

    printf("Evaluator Formularum Modalium (Semantica Kripkiana)\n");
    printf("===================================================\n");
    monstra_structuram(&k);
    putchar('\n');

    if (argc > 1) {
        const char *p = argv[1];
        n_formulae = 0;
        Formula *f = parsa_rec(&p);
        if (!f) return 1;
        printf("Formula: %s\n", argv[1]);
        for (int m = 0; m < k.n_mundi; m++)
            printf("  w%d: %s\n", m, satisfit(&k, m, f) ? "VERUM" : "FALSUM");
    } else {
        for (int i = 0; formulae_exemplares[i]; i++) {
            const char *p = formulae_exemplares[i];
            n_formulae = 0;
            Formula *f = parsa_rec(&p);
            if (!f) continue;
            printf("[%2d] %-30s ", i, descriptiones[i]);
            for (int m = 0; m < k.n_mundi; m++)
                printf(" w%d=%c", m,
                       satisfit(&k, m, f) ? 'T' : 'F');
            putchar('\n');
        }
        printf("\nUsus: kripke 'formula'  (prefixa: p q r, ! & | > [ <)\n");
    }
    return 0;
}
