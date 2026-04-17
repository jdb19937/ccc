/*
 * neumanni.c — Ordinales per Constructionem Neumannianam
 *
 * Aedificat numeros naturales ut ordinales von Neumannianos: 0 = {},
 * 1 = {0} = {{}}, 2 = {0, 1} = {{}, {{}}}, ... n+1 = n ∪ {n}.
 * Monstrat structuras explicitis cum signis mathematicis et verificat
 * axiomata ZFC fundamentalia: vacuitas, par, unio, infinitas.
 *
 * Exerciet compilatorem per: structuras recursivas cum indicatoribus
 * ad se ipsas, dynamicam allocationem arborum, buffers chordarum
 * magnos, computationem membershipis per comparationem profundam,
 * et qsort cum comparatore recursivo.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ORD       32
#define MAX_CHORDA    16384

/* ===== Structura ordinis (arbor setorum) ===== */

typedef struct Set {
    struct Set **membra;
    int n_membra;
} Set;

/* ===== Constructores fundamentales ===== */

/* Crea novum set vacuum */
static Set *
setum_vacuum(void)
{
    Set *s = malloc(sizeof(Set));
    if (!s) { fprintf(stderr, "Error memoriae\n"); exit(1); }
    s->membra = NULL;
    s->n_membra = 0;
    return s;
}

/*
 * Relatio memberhipis profunda: a ∈ b si b habet setum structuraliter
 * aequalem ipsi a inter membra sua.
 */
static int aequantur(const Set *a, const Set *b);

static int
est_membrum(const Set *a, const Set *b)
{
    for (int i = 0; i < b->n_membra; i++)
        if (aequantur(a, b->membra[i]))
            return 1;
    return 0;
}

/*
 * Aequalitas extensionalitatis (axioma ZFC §II): duo seti aequantur
 * iff quodque membrum unius est membrum alterius.
 */
static int
aequantur(const Set *a, const Set *b)
{
    if (a->n_membra != b->n_membra) return 0;
    for (int i = 0; i < a->n_membra; i++)
        if (!est_membrum(a->membra[i], b))
            return 0;
    return 1;
}

/*
 * Operator successoris Neumanniani: s(n) = n ∪ {n}.
 * Novus set cum omnibus membris n plus n ipsum.
 */
static Set *
successor(Set *n)
{
    Set *s = setum_vacuum();
    s->membra = malloc((n->n_membra + 1) * sizeof(Set *));
    if (!s->membra) { fprintf(stderr, "Error memoriae\n"); exit(1); }
    for (int i = 0; i < n->n_membra; i++)
        s->membra[i] = n->membra[i];
    s->membra[n->n_membra] = n;
    s->n_membra = n->n_membra + 1;
    return s;
}

/* ===== Aedificator ordinum 0..n per iterationem ===== */

static Set *
ordinal(int n)
{
    Set *cur = setum_vacuum();
    for (int i = 0; i < n; i++)
        cur = successor(cur);
    return cur;
}

/* ===== Scriptor notationis setorum ===== */

/*
 * Scribit setum notatione mathematica: {} pro vacuo, {m1, m2, ...}
 * pro pleno.  Profunditas limitata ne recursio sine limite fiat.
 */
static void
scribe_setum(const Set *s, char *buf, int *pos, int max, int prof)
{
    if (*pos >= max - 8) return;
    if (prof > 6) {
        *pos += snprintf(buf + *pos, max - *pos, "...");
        return;
    }
    buf[(*pos)++] = '{';
    for (int i = 0; i < s->n_membra; i++) {
        if (i > 0) {
            buf[(*pos)++] = ',';
            buf[(*pos)++] = ' ';
        }
        scribe_setum(s->membra[i], buf, pos, max, prof + 1);
    }
    buf[(*pos)++] = '}';
    buf[*pos] = '\0';
}

/* ===== Computatio cardinalitatis ===== */

static int
cardinalitas_transitiva(const Set *s)
{
    /* rudimentum: numeremus membros primum gradu */
    return s->n_membra;
}

/* ===== Verificatio proprietatum ===== */

static int
est_transitivus(const Set *s)
{
    /* set transitivus: quisque member membri est etiam member */
    for (int i = 0; i < s->n_membra; i++) {
        const Set *m = s->membra[i];
        for (int j = 0; j < m->n_membra; j++)
            if (!est_membrum(m->membra[j], s))
                return 0;
    }
    return 1;
}

/* ===== Axiomata ZFC — verificatio exempli ===== */

static void
monstra_axiomata(void)
{
    printf("\nAxiomata ZFC fundamentalia (exempli gratia):\n");

    Set *vac1 = setum_vacuum();
    Set *vac2 = setum_vacuum();
    printf("  Extensionalitas: {} = {} ? %s\n",
           aequantur(vac1, vac2) ? "DA" : "NON");

    Set *un = successor(vac1);
    printf("  Par: {} in 1 = { {} } ? %s\n",
           est_membrum(vac2, un) ? "DA" : "NON");

    Set *duo = successor(un);
    printf("  Unio (1 ∪ {1} = 2, cardinalitas): %d\n",
           cardinalitas_transitiva(duo));

    printf("  Transitivitas ordinis 3: %s\n",
           est_transitivus(ordinal(3)) ? "DA" : "NON");

    printf("  Infinitas: ordinales usque ad omega constructibiles, ");
    printf("exemplum ordinale %d existit.\n", MAX_ORD);
}

/* ===== Proiectio textualis: ordinales 0..n ===== */

static void
monstra_ordinales(int n_max)
{
    printf("\nOrdinales Neumanniani 0..%d:\n", n_max);
    for (int i = 0; i <= n_max; i++) {
        Set *ord = ordinal(i);
        char buf[MAX_CHORDA];
        int pos = 0;
        scribe_setum(ord, buf, &pos, MAX_CHORDA, 0);
        printf("  %2d = %s\n", i, buf);
    }
}

/* ===== Arithmetica ordinalis simplex ===== */

/*
 * Additio ordinalis α + β: iteratio successoris β vicibus super α.
 * Pro ordinalibus finitis, coincidit cum additione numerorum naturalium.
 */
static Set *
additio_ordinalis(Set *a, int b)
{
    for (int i = 0; i < b; i++)
        a = successor(a);
    return a;
}

int
main(int argc, char *argv[])
{
    printf("Ordinales Neumanniani et Axiomata ZFC\n");
    printf("=====================================\n");

    int n_max = 5;
    if (argc > 1) {
        char *fin;
        long v = strtol(argv[1], &fin, 10);
        if (*fin || v < 0 || v > MAX_ORD) {
            fprintf(stderr, "Error: argumentum 0..%d\n", MAX_ORD);
            return 1;
        }
        n_max = (int)v;
    }

    monstra_ordinales(n_max);
    monstra_axiomata();

    /* Arithmetica ordinalis simplex: 2 + 3 */
    Set *a = ordinal(2);
    Set *b = additio_ordinalis(a, 3);
    char buf[MAX_CHORDA];
    int pos = 0;
    scribe_setum(b, buf, &pos, MAX_CHORDA, 0);
    printf("\n2 + 3 = %s (cardinalitas %d)\n", buf,
           cardinalitas_transitiva(b));

    return 0;
}
