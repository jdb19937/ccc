/*
 * transformer.c — Minimus Doctor Transformatoris Characterum
 *
 * Exempla paradigma transformatoris generativi ad charactera: cum
 * immersionibus parvis (d=8), uno strato attentionis singularis,
 * capitem linearem ad distributionem super characteres, et instructionem
 * per descensum gradientis stochasticum.  Discipulus ex textu parvo,
 * deinde generat textum novum similem.
 *
 * Parvissimum exemplum practicum: vocabularium = ASCII minusculum +
 * spatium + puncta; contextus = 8 characteres; dimensio immersionis = 8.
 * Omnia vero computantur per aritmeticam fluitantem.  Semen aleatorium
 * ex argumento vel tempore.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define VOCAB_MAG         30  /* 26 litterae + spatium + . + , + \n */
#define D_MODEL            8  /* dimensio immersionis */
#define CTX_LON            8  /* longitudo contextus */
#define MAX_CORPUS      4096
#define INSTRUCT_ITER    500
#define GRAD_ETA       0.05f  /* ratio doctrinae */

/* Tabula vocabularii: index ad characterem */
static const char vocab[VOCAB_MAG] = {
    'a','b','c','d','e','f','g','h','i','j','k','l','m',
    'n','o','p','q','r','s','t','u','v','w','x','y','z',
    ' ', '.', ',', '\n',
};

/* Redde indicem characteris in vocabulario (vel -1) */
static int
indice(int c)
{
    c = tolower(c);
    for (int i = 0; i < VOCAB_MAG; i++)
        if (vocab[i] == c) return i;
    return -1;
}

/* ===== Parametri transformatoris (discibiles) ===== */

static float E[VOCAB_MAG][D_MODEL];   /* immersiones */
static float WQ[D_MODEL][D_MODEL];    /* pondera interrogationis */
static float WK[D_MODEL][D_MODEL];    /* pondera clavis */
static float WV[D_MODEL][D_MODEL];    /* pondera valoris */
static float WO[D_MODEL][VOCAB_MAG];  /* capit proiectio ad vocabularium */

/* Numerus fluitans in [-limen, limen] */
static float
aleatorium_parvum(float limen)
{
    return ((float)rand() / (float)RAND_MAX) * 2.0f * limen - limen;
}

static void
inicializa_parametros(void)
{
    float s = 0.1f;
    for (int i = 0; i < VOCAB_MAG; i++)
        for (int j = 0; j < D_MODEL; j++)
            E[i][j] = aleatorium_parvum(s);
    for (int i = 0; i < D_MODEL; i++)
        for (int j = 0; j < D_MODEL; j++) {
            WQ[i][j] = aleatorium_parvum(s);
            WK[i][j] = aleatorium_parvum(s);
            WV[i][j] = aleatorium_parvum(s);
        }
    for (int i = 0; i < D_MODEL; i++)
        for (int j = 0; j < VOCAB_MAG; j++)
            WO[i][j] = aleatorium_parvum(s);
}

/* ===== Passus anterius ===== */

/*
 * Pro brevitate, implementamus modum simplicissimum:
 * 1. immerge unumquemque characterem contextus
 * 2. computa mediam immersionum (analogon attentionis uniformis)
 * 3. proice ad vocabularium per WO
 * 4. softmax pro distributione probabilitatum
 */

static void
softmax(float *x, int n)
{
    float mx = x[0];
    for (int i = 1; i < n; i++)
        if (x[i] > mx) mx = x[i];
    float sum = 0;
    for (int i = 0; i < n; i++) {
        x[i] = expf(x[i] - mx);
        sum += x[i];
    }
    for (int i = 0; i < n; i++) x[i] /= sum;
}

static void
prae_dice(const int *ctx, int ctx_lon, float *logiti)
{
    /* medium ponderatum simplex: quisque position tenet pondus aequale */
    float h[D_MODEL] = { 0 };
    for (int t = 0; t < ctx_lon; t++) {
        /* immersionem accipe */
        float emb[D_MODEL];
        for (int d = 0; d < D_MODEL; d++) emb[d] = E[ctx[t]][d];
        /* Q,K,V — sed pro simplicitate attendimus uniforme */
        float v[D_MODEL] = { 0 };
        for (int i = 0; i < D_MODEL; i++)
            for (int j = 0; j < D_MODEL; j++)
                v[i] += emb[j] * WV[j][i];
        for (int d = 0; d < D_MODEL; d++)
            h[d] += v[d] / (float)ctx_lon;
    }
    /* proiectio ad logitos */
    for (int k = 0; k < VOCAB_MAG; k++) {
        logiti[k] = 0;
        for (int d = 0; d < D_MODEL; d++)
            logiti[k] += h[d] * WO[d][k];
    }
}

/* ===== Instructio per descensum gradientis (simplex) ===== */

/*
 * Pro docendo sine plena computatione gradientis, utimur heuristica
 * simplificatam: incrementa logit vere correcti proporzionaliter ad
 * errorem per propagationem retrogradam per WO et mediae immersionum.
 */
static void
doce_passu(const int *ctx, int ctx_lon, int bene)
{
    float logiti[VOCAB_MAG];
    prae_dice(ctx, ctx_lon, logiti);
    float probs[VOCAB_MAG];
    memcpy(probs, logiti, sizeof(probs));
    softmax(probs, VOCAB_MAG);

    /* gradiens logitorum: probs - vector unitarius */
    float dL[VOCAB_MAG];
    for (int k = 0; k < VOCAB_MAG; k++)
        dL[k] = probs[k] - (k == bene ? 1.0f : 0.0f);

    /* computa mediam immersionum iterum pro propagatione retrograda ad WO */
    float h[D_MODEL] = { 0 };
    for (int t = 0; t < ctx_lon; t++) {
        float emb[D_MODEL];
        for (int d = 0; d < D_MODEL; d++) emb[d] = E[ctx[t]][d];
        float v[D_MODEL] = { 0 };
        for (int i = 0; i < D_MODEL; i++)
            for (int j = 0; j < D_MODEL; j++)
                v[i] += emb[j] * WV[j][i];
        for (int d = 0; d < D_MODEL; d++)
            h[d] += v[d] / (float)ctx_lon;
    }

    /* gradiens WO: productum exterius h et dL */
    for (int d = 0; d < D_MODEL; d++)
        for (int k = 0; k < VOCAB_MAG; k++)
            WO[d][k] -= GRAD_ETA * h[d] * dL[k];

    /*
     * Pro simplicitate, proximum gradientem (ad WV et E) simpliciter
     * approximamus per signum: incrementa parum immersiones contextus
     * versus immersionem expectatam.
     */
    float h_grad[D_MODEL] = { 0 };
    for (int d = 0; d < D_MODEL; d++)
        for (int k = 0; k < VOCAB_MAG; k++)
            h_grad[d] += WO[d][k] * dL[k];
    for (int t = 0; t < ctx_lon; t++) {
        for (int d = 0; d < D_MODEL; d++)
            E[ctx[t]][d] -= GRAD_ETA * h_grad[d] / (float)ctx_lon;
    }
}

/* ===== Sumptio ex distributione ===== */

static int
preme_ex_distributione(const float *probs)
{
    float r = (float)rand() / (float)RAND_MAX;
    float accum = 0;
    for (int k = 0; k < VOCAB_MAG; k++) {
        accum += probs[k];
        if (r < accum) return k;
    }
    return VOCAB_MAG - 1;
}

/* ===== Parsor corporis ad indices ===== */

static int
parsor_ad_indices(const char *txt, int *out, int max)
{
    int n = 0;
    for (int i = 0; txt[i] && n < max; i++) {
        int idx = indice((unsigned char)txt[i]);
        if (idx >= 0) out[n++] = idx;
    }
    return n;
}

/* ===== Instruuntur modelem ===== */

static void
instrue(const int *corpus, int lon, int iter)
{
    for (int it = 0; it < iter; it++) {
        /* elige positionem aleatoriam */
        if (lon <= CTX_LON) break;
        int p = rand() % (lon - CTX_LON);
        doce_passu(corpus + p, CTX_LON, corpus[p + CTX_LON]);
    }
}

/* ===== Generat sequentiam ===== */

static void
genera(const int *inicium, int in_lon, int n_cara)
{
    int ctx[CTX_LON];
    int cur = in_lon > CTX_LON ? CTX_LON : in_lon;
    for (int i = 0; i < cur; i++)
        ctx[i] = inicium[in_lon - cur + i];

    for (int i = 0; i < in_lon; i++)
        fputc(vocab[inicium[i]], stderr);

    for (int k = 0; k < n_cara; k++) {
        float logiti[VOCAB_MAG];
        prae_dice(ctx, cur, logiti);
        softmax(logiti, VOCAB_MAG);
        int novus = preme_ex_distributione(logiti);
        fputc(vocab[novus], stderr);
        /* sicc contextus */
        if (cur < CTX_LON) {
            ctx[cur++] = novus;
        } else {
            for (int i = 0; i < CTX_LON - 1; i++) ctx[i] = ctx[i+1];
            ctx[CTX_LON - 1] = novus;
        }
    }
    fputc('\n', stderr);
}

/* ===== Corpus discibilis defaltus ===== */

static const char *const corpus_defaltus =
    "in principio erat verbum et verbum erat apud deum et deus erat "
    "verbum hoc erat in principio apud deum omnia per ipsum facta sunt "
    "et sine ipso factum est nihil quod factum est in ipso vita erat "
    "et vita erat lux hominum et lux in tenebris lucet et tenebrae eam "
    "non comprehenderunt fuit homo missus a deo cui nomen erat iohannes "
    "hic venit in testimonium ut testimonium perhiberet de lumine ut "
    "omnes crederent per illum";

int
main(int argc, char *argv[])
{
    unsigned semen = 57;
    int iter = INSTRUCT_ITER;
    int n_gen = 200;
    const char *inicium_str = "in principio";
    const char *corpus_str = corpus_defaltus;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s")      == 0 && i + 1 < argc)
            semen = (unsigned)atoi(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            iter = atoi(argv[++i]);
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            n_gen = atoi(argv[++i]);
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            inicium_str = argv[++i];
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
            corpus_str = argv[++i];
    }
    srand(semen);

    printf("Minimus Doctor Transformatoris\n");
    printf("==============================\n");
    printf("(d=%d, ctx=%d, vocab=%d, iter=%d, semen=%u)\n",
        D_MODEL, CTX_LON, VOCAB_MAG, iter, semen);

    inicializa_parametros();

    int corpus_idx[MAX_CORPUS];
    int corpus_lon = parsor_ad_indices(corpus_str, corpus_idx, MAX_CORPUS);
    printf("\nCorpus %d characteres habet, instruentes...\n", corpus_lon);

    instrue(corpus_idx, corpus_lon, iter);

    int inicium_idx[CTX_LON * 4];
    int inicium_lon = parsor_ad_indices(inicium_str, inicium_idx, CTX_LON * 4);
    printf("\nExitus generatuson ad stderr.\n");
    genera(inicium_idx, inicium_lon, n_gen);

    return 0;
}
