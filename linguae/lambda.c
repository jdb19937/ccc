/*
 * lambda.c — Reductor Calculi Lambdae
 *
 * Parsa et reducit expressiones in calculo lambda non typato:
 *   \x.corpus    abstractio
 *   (M N)        applicatio
 *   x            variabilis
 * Sustinet reductionem beta (ordine normali) cum alpha-renominatione
 * pro elusione captionis variabilium liberarum.  Ostendit gradus
 * reductionis usque ad formam normalem vel ad finem limitis.
 *
 * Exercet compilatorem per: typum recursivum cum unione discriminata,
 * reditum structurae grandis ex functione, catenam malloc/free pro
 * allocatione dynamica arborum, alveos chordarum magnos.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NOMEN       16
#define MAX_TERMINI     1024
#define MAX_CHORDA      4096
#define MAX_STEPS        50

/* ===== Terminus lambdae (arbor) ===== */

typedef enum { T_VAR, T_ABS, T_APP } GenusTermini;

typedef struct Term {
    GenusTermini genus;
    char nomen[MAX_NOMEN];    /* pro T_VAR aut T_ABS */
    struct Term *filiolus;    /* corpus pro ABS, functor pro APP */
    struct Term *argumentum;  /* argumentum pro APP */
} Term;

static Term receptaculum_terminorum[MAX_TERMINI];
static int n_termini = 0;
static int var_contator = 0;

static Term *
novus_terminus(GenusTermini g)
{
    if (n_termini >= MAX_TERMINI) {
        fprintf(stderr, "Error: nimis multi termini\n");
        exit(1);
    }
    Term *t = &receptaculum_terminorum[n_termini++];
    t->genus = g;
    t->nomen[0] = '\0';
    t->filiolus = t->argumentum = NULL;
    return t;
}

/* ===== Parsor ===== */

static void praetermitte_spatia(const char **p)
{
    while (**p == ' ' || **p == '\t')
        (*p)++;
}

static Term *parsa_termum(const char **p);

static Term *
parsa_atomum(const char **p)
{
    praetermitte_spatia(p);
    if (**p == '(') {
        (*p)++;
        Term *t = parsa_termum(p);
        praetermitte_spatia(p);
        if (**p == ')') (*p)++;
        return t;
    }
    if (**p == '\\' || **p == 'L') {
        /* \x.corpus vel Lx.corpus */
        (*p)++;
        praetermitte_spatia(p);
        Term *t = novus_terminus(T_ABS);
        int i = 0;
        while (isalnum((unsigned char)**p) && i < MAX_NOMEN - 1)
            t->nomen[i++] = *(*p)++;
        t->nomen[i] = '\0';
        praetermitte_spatia(p);
        if (**p == '.') (*p)++;
        t->filiolus = parsa_termum(p);
        return t;
    }
    if (isalpha((unsigned char)**p)) {
        Term *t = novus_terminus(T_VAR);
        int i = 0;
        while (isalnum((unsigned char)**p) && i < MAX_NOMEN - 1)
            t->nomen[i++] = *(*p)++;
        t->nomen[i] = '\0';
        return t;
    }
    return NULL;
}

/*
 * Applicatio est associativa a sinistra: a b c = (a b) c.
 * Parsa omnes atoma proxima et applica successive.
 */
static Term *
parsa_termum(const char **p)
{
    Term *t = parsa_atomum(p);
    if (!t) return NULL;
    while (1) {
        praetermitte_spatia(p);
        if (!**p || **p == ')' || **p == '.') break;
        Term *arg = parsa_atomum(p);
        if (!arg) break;
        Term *app = novus_terminus(T_APP);
        app->filiolus = t;
        app->argumentum = arg;
        t = app;
    }
    return t;
}

/* ===== Scriptor pro monstrando ===== */

static void
scribe_termum(const Term *t, char *buf, int *pos, int max)
{
    if (!t || *pos >= max - 1) return;
    switch (t->genus) {
    case T_VAR:
        *pos += snprintf(buf + *pos, max - *pos, "%s", t->nomen);
        break;
    case T_ABS:
        *pos += snprintf(buf + *pos, max - *pos, "(\\%s.", t->nomen);
        scribe_termum(t->filiolus, buf, pos, max);
        if (*pos < max - 1) buf[(*pos)++] = ')';
        break;
    case T_APP:
        if (*pos < max - 1) buf[(*pos)++] = '(';
        scribe_termum(t->filiolus, buf, pos, max);
        if (*pos < max - 1) buf[(*pos)++] = ' ';
        scribe_termum(t->argumentum, buf, pos, max);
        if (*pos < max - 1) buf[(*pos)++] = ')';
        break;
    }
    buf[*pos] = '\0';
}

/* ===== Copia profunda ===== */

static Term *
copia(const Term *t)
{
    if (!t) return NULL;
    Term *c = novus_terminus(t->genus);
    strncpy(c->nomen, t->nomen, MAX_NOMEN - 1);
    c->nomen[MAX_NOMEN - 1] = '\0';
    c->filiolus = copia(t->filiolus);
    c->argumentum = copia(t->argumentum);
    return c;
}

/* ===== Verbis liberae ===== */

static int
apparet_libere(const char *nomen, const Term *t)
{
    if (!t) return 0;
    switch (t->genus) {
    case T_VAR:
        return strcmp(t->nomen, nomen) == 0;
    case T_ABS:
        if (strcmp(t->nomen, nomen) == 0) return 0;
        return apparet_libere(nomen, t->filiolus);
    case T_APP:
        return apparet_libere(nomen, t->filiolus)
            || apparet_libere(nomen, t->argumentum);
    }
    return 0;
}

/* ===== Substitutio cum alpha-renominatione ===== */

static void
substitute(Term *t, const char *vet, const Term *nov)
{
    if (!t) return;
    switch (t->genus) {
    case T_VAR:
        if (strcmp(t->nomen, vet) == 0) {
            Term *c = copia(nov);
            t->genus = c->genus;
            strncpy(t->nomen, c->nomen, MAX_NOMEN - 1);
            t->nomen[MAX_NOMEN - 1] = '\0';
            t->filiolus = c->filiolus;
            t->argumentum = c->argumentum;
        }
        break;
    case T_ABS:
        if (strcmp(t->nomen, vet) == 0) return; /* variabilis ligata */
        /* alpha-renomatio si conflictus */
        if (apparet_libere(t->nomen, nov)) {
            char novum[MAX_NOMEN];
            snprintf(novum, sizeof(novum), "%s%d", t->nomen, var_contator++);
            Term var_n = { T_VAR, { 0 }, NULL, NULL };
            strncpy(var_n.nomen, novum, MAX_NOMEN - 1);
            substitute(t->filiolus, t->nomen, &var_n);
            strncpy(t->nomen, novum, MAX_NOMEN - 1);
            t->nomen[MAX_NOMEN - 1] = '\0';
        }
        substitute(t->filiolus, vet, nov);
        break;
    case T_APP:
        substitute(t->filiolus, vet, nov);
        substitute(t->argumentum, vet, nov);
        break;
    }
}

/* ===== Reductio beta per ordinem normalem ===== */

/*
 * Ordo normalis: reduce redicem sinistriorem exteriorem.
 * Redex = (\x.M) N, reducit ad M[x := N].
 * Redde 1 si mutatum est aliquid, 0 aliter.
 */
static int
reduce_passim(Term *t)
{
    if (!t) return 0;
    if (t->genus == T_APP && t->filiolus && t->filiolus->genus == T_ABS) {
        /* beta-redex */
        Term *abs = t->filiolus;
        Term *arg = t->argumentum;
        Term *corpus = copia(abs->filiolus);
        substitute(corpus, abs->nomen, arg);
        /* muta t in corpus */
        t->genus = corpus->genus;
        strncpy(t->nomen, corpus->nomen, MAX_NOMEN - 1);
        t->nomen[MAX_NOMEN - 1] = '\0';
        t->filiolus = corpus->filiolus;
        t->argumentum = corpus->argumentum;
        return 1;
    }
    /* non est redex hic — descende */
    if (t->genus == T_APP) {
        if (reduce_passim(t->filiolus)) return 1;
        return reduce_passim(t->argumentum);
    }
    if (t->genus == T_ABS)
        return reduce_passim(t->filiolus);
    return 0;
}

/* ===== Exempla: numerales Church et combinatores ===== */

static const char *const exempla[] = {
    "(\\x.x) y",
    "(\\x.\\y.x) a b",
    "(\\x.\\y.y) a b",
    "(\\f.\\x.f (f x)) (\\n.n) y",     /* num 2 applicat ad identicam */
    "(\\x.x x) (\\y.y)",                /* applicatio ad se ipsam */
    "(\\x.\\y.x y) (\\z.z z) w",
    "(\\f.\\g.\\x.f (g x)) (\\y.y) (\\y.y) z",  /* compositio */
    /* numerales Church: 0 = \f.\x.x, 1 = \f.\x.f x */
    "(\\n.\\f.\\x.f (n f x)) (\\f.\\x.x)",     /* succ 0 */
    NULL,
};

int
main(int argc, char *argv[])
{
    printf("Reductor Calculi Lambdae\n");
    printf("========================\n");

    const char *intrantia[32];
    int n_in = 0;
    if (argc > 1) {
        for (int i = 1; i < argc && n_in < 32; i++)
            intrantia[n_in++] = argv[i];
    } else {
        for (int i = 0; exempla[i] && n_in < 32; i++)
            intrantia[n_in++] = exempla[i];
    }

    for (int i = 0; i < n_in; i++) {
        n_termini = 0;
        var_contator = 0;
        const char *p = intrantia[i];
        Term *t = parsa_termum(&p);
        if (!t) {
            fprintf(stderr, "Error parsa: %s\n", intrantia[i]);
            continue;
        }
        printf("\nIntrat: %s\n", intrantia[i]);
        char buf[MAX_CHORDA];
        int pos;
        for (int step = 0; step <= MAX_STEPS; step++) {
            pos = 0;
            scribe_termum(t, buf, &pos, MAX_CHORDA);
            printf("  [%2d] %s\n", step, buf);
            if (!reduce_passim(t)) {
                printf("  -> forma normalis\n");
                break;
            }
            if (step == MAX_STEPS)
                printf("  -> limit %d gradus\n", MAX_STEPS);
        }
    }

    if (argc == 1)
        printf("\nUsus: lambda 'expressio'  (\\x.corpus, applicatio spatio)\n");
    return 0;
}
