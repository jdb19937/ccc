/*
 * confabulator.c — Generator Textus per Catenam Markovianam
 *
 * Generator statisticus ex corpore exemplari: computat probabilitates
 * transitionum ex n-grammatis verborum et producit novos textus
 * similes sed non identicos exemplo.  Semen seminis ex argumento
 * vel ex tempore.
 *
 * Accipit corpus per argumenta vel utitur exemplis defaltis polyglotis.
 * Ordo Markovianus (n) determinat continuitatem generationis.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_VERBI       64
#define MAX_VOCABULI    2048
#define MAX_TRANSIT     8192
#define MAX_SUCCESSORES 32

/* Nodus in catena transitionum: ex verbo (praevio) in sequentes */
typedef struct {
    char verbum[MAX_VERBI];
    int  freq;
} Successor;

typedef struct {
    char verbum[MAX_VERBI];
    Successor successores[MAX_SUCCESSORES];
    int  n_succ;
    int  total_freq;
} NodusCatenae;

static NodusCatenae catena[MAX_VOCABULI];
static int n_catenae = 0;

/* Inveni vel crea nodum pro verbo */
static NodusCatenae *
quaere_nodum(const char *verbum)
{
    for (int i = 0; i < n_catenae; i++)
        if (strcmp(catena[i].verbum, verbum) == 0)
            return &catena[i];
    if (n_catenae >= MAX_VOCABULI)
        return NULL;
    NodusCatenae *n = &catena[n_catenae++];
    strncpy(n->verbum, verbum, MAX_VERBI - 1);
    n->verbum[MAX_VERBI - 1] = '\0';
    n->n_succ = 0;
    n->total_freq = 0;
    return n;
}

/* Registra transitionem ex a in b */
static void
registra(const char *a, const char *b)
{
    NodusCatenae *n = quaere_nodum(a);
    if (!n)
        return;
    for (int i = 0; i < n->n_succ; i++) {
        if (strcmp(n->successores[i].verbum, b) == 0) {
            n->successores[i].freq++;
            n->total_freq++;
            return;
        }
    }
    if (n->n_succ >= MAX_SUCCESSORES)
        return;
    strncpy(n->successores[n->n_succ].verbum, b, MAX_VERBI - 1);
    n->successores[n->n_succ].verbum[MAX_VERBI - 1] = '\0';
    n->successores[n->n_succ].freq = 1;
    n->n_succ++;
    n->total_freq++;
}

/* Paratio corporis: scindit verba et registrat transitiones ordinis 1 */
static void
instruit(const char *corpus)
{
    char praev[MAX_VERBI] = { 0 };
    int i = 0;
    int lon = (int)strlen(corpus);
    while (i < lon) {
        while (i < lon && !isalpha((unsigned char)corpus[i]))
            i++;
        if (i >= lon) break;
        char verbum[MAX_VERBI];
        int j = 0;
        while (i < lon && (isalpha((unsigned char)corpus[i])
               || corpus[i] == '\'' || corpus[i] == '-')
               && j < MAX_VERBI - 1) {
            verbum[j++] = (char)tolower((unsigned char)corpus[i++]);
        }
        verbum[j] = '\0';
        if (praev[0])
            registra(praev, verbum);
        strcpy(praev, verbum);
    }
}

/* Elige successorem per distributionem */
static const char *
elige_successorem(const NodusCatenae *n)
{
    if (!n || n->n_succ == 0)
        return NULL;
    int r = rand() % n->total_freq;
    int accum = 0;
    for (int i = 0; i < n->n_succ; i++) {
        accum += n->successores[i].freq;
        if (r < accum)
            return n->successores[i].verbum;
    }
    return n->successores[n->n_succ - 1].verbum;
}

/* Generat n verba incipiens ab inicio dato */
static void
confabula(const char *inicium, int n_verba)
{
    char cur[MAX_VERBI];
    strncpy(cur, inicium, MAX_VERBI - 1);
    cur[MAX_VERBI - 1] = '\0';
    /* normalizza */
    for (int i = 0; cur[i]; i++)
        cur[i] = (char)tolower((unsigned char)cur[i]);

    fputs(cur, stdout);
    for (int i = 1; i < n_verba; i++) {
        NodusCatenae *n = quaere_nodum(cur);
        const char *proxi = elige_successorem(n);
        if (!proxi) {
            /* reincipe ex nodo aleatorio */
            if (n_catenae == 0) break;
            NodusCatenae *rn = &catena[rand() % n_catenae];
            proxi = rn->verbum;
        }
        fputc(' ', stdout);
        fputs(proxi, stdout);
        strncpy(cur, proxi, MAX_VERBI - 1);
        cur[MAX_VERBI - 1] = '\0';
    }
    fputc('\n', stdout);
}

/* Corpora defalta — polyglotta */

static const char *const corpus_latinum =
    "gallia est omnis divisa in partes tres quarum unam incolunt belgae "
    "aliam aquitani tertiam qui ipsorum lingua celtae nostra galli "
    "appellantur hi omnes lingua institutis legibus inter se differunt "
    "arma virumque cano troiae qui primus ab oris italiam fato profugus "
    "lavinaque venit litora multa ille terris iactatus et alto vi superum "
    "saevae memorem iunonis ob iram multa quoque et bello passus dum "
    "conderet urbem inferretque deos latio genus unde latinum albanique "
    "patres atque altae moenia romae";

static const char *const corpus_anglicum =
    "in the beginning there was the word and the word was with god "
    "and the word was god all things were made by him and without him "
    "was not anything made that was made in him was life and the life "
    "was the light of men and the light shineth in darkness and the "
    "darkness comprehended it not there was a man sent from god whose "
    "name was john the same came for a witness to bear witness of the "
    "light that all men through him might believe";

int
main(int argc, char *argv[])
{
    unsigned semen = 67;
    int n_verba = 40;
    const char *inicium = NULL;
    const char *corpus_usu = NULL;

    /* parsa argumenta */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            semen = (unsigned)atoi(argv[++i]);
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
            n_verba = atoi(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0 && i + 1 < argc)
            inicium = argv[++i];
        else if (strcmp(argv[i], "-c") == 0 && i + 1 < argc)
            corpus_usu = argv[++i];
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Error: vexillum ignotum '%s'\n", argv[i]);
            return 1;
        }
    }
    srand(semen);

    printf("Confabulator Markovianus\n");
    printf("========================\n");
    printf("(semen=%u, n=%d)\n\n", semen, n_verba);

    if (corpus_usu) {
        instruit(corpus_usu);
        printf("[ex corpore proprio]\n");
        confabula(inicium ? inicium : "the", n_verba);
    } else {
        /* monstra ex ambobus corporibus defaltis */
        instruit(corpus_latinum);
        printf("[Latina, corpus Caesaris et Vergilii]\n");
        confabula(inicium ? inicium : "gallia", n_verba);

        /* purga catenam pro corpore novo */
        n_catenae = 0;
        memset(catena, 0, sizeof(catena));

        instruit(corpus_anglicum);
        printf("\n[Anglica, corpus evangelii]\n");
        confabula(inicium ? inicium : "the", n_verba);
    }

    return 0;
}
