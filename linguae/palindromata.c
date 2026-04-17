/*
 * palindromata.c — Detector et Generator Palindromorum
 *
 * Verificat an chorda sit palindromus per modos varios: strictus
 * (character per characterem), mollis (ignora spatia et punctuationem), et
 * verborum (verba in ordine inverso).  Polyglotta: functionat cum
 * quolibet charactare ASCII.
 *
 * Cruciatus compilatoris: tabula indicatorum functionum, campi bitorum
 * per modo, recursio, litteralia composita, varargs, reditus structurae.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

/* ===== Modi palindromi ===== */

typedef enum {
    MOD_STRICTUS,   /* character per characterem, casu sensibilis */
    MOD_MOLLIS,     /* ignora spatia, punctuationem, casum */
    MOD_VERBORUM,   /* verba in ordine inverso */
} ModusPalindromi;

/* vexilla processationis in campis bitorum */
typedef struct {
    unsigned int ignora_spatium    : 1;
    unsigned int ignora_casum      : 1;
    unsigned int ignora_punctum    : 1;
    unsigned int inversio_verborum : 1;
} VexillaProcessationis;

/* ===== Functio detectionis ===== */

typedef int (*Detector)(const char *, VexillaProcessationis);

/*
    * normaliza chordam per vexilla.  Redde longitudinem novam.
 */
static int
normaliza_chordam(const char *in, char *out, int max,
                  VexillaProcessationis vex)
{
    int j = 0;
    for (int i = 0; in[i] && j < max - 1; i++) {
        unsigned char c = (unsigned char)in[i];
        if (vex.ignora_spatium && isspace(c))
            continue;
        if (vex.ignora_punctum && ispunct(c))
            continue;
        out[j++] = vex.ignora_casum ? (char)tolower(c) : (char)c;
    }
    out[j] = '\0';
    return j;
}

/* litterale compositum pro signis in printf — demonstratio simplex */
static int
est_palindromus_basis(const char *s, int lon)
{
    for (int i = 0, j = lon - 1; i < j; i++, j--)
        if (s[i] != s[j])
            return 0;
    return 1;
}

static int
detecta_strictum(const char *s, VexillaProcessationis vex)
{
    (void)vex;
    int lon = (int)strlen(s);
    return est_palindromus_basis(s, lon);
}

static int
detecta_molle(const char *s, VexillaProcessationis vex)
{
    char buf[1024];
    int lon = normaliza_chordam(s, buf, sizeof(buf), vex);
    return est_palindromus_basis(buf, lon);
}

/*
 * Modus verborum: divide in verba, compara primum cum ultimo,
 * secundum cum paenultimo, etc.
 */
static int
detecta_verba(const char *s, VexillaProcessationis vex)
{
    (void)vex;
    const char *verba[256];
    int n = 0;
    int lon = (int)strlen(s);
    int i = 0;
    /* puncta verborum sunt mutabilia — copiamus ad buffer */
    static char buf[2048];
    int bl = 0;
    while (i < lon && n < 256) {
        while (i < lon && isspace((unsigned char)s[i])) i++;
        if (i >= lon) break;
        verba[n++] = buf + bl;
        while (i < lon && !isspace((unsigned char)s[i])
               && bl < (int)sizeof(buf) - 1) {
            buf[bl++] = (char)tolower((unsigned char)s[i++]);
        }
        if (bl < (int)sizeof(buf) - 1)
            buf[bl++] = '\0';
    }
    for (int a = 0, b = n - 1; a < b; a++, b--)
        if (strcmp(verba[a], verba[b]) != 0)
            return 0;
    return 1;
}

/* tabula distributio per modum */
static const struct {
    const char *nomen;
    Detector fn;
    VexillaProcessationis vex;
} modi[] = {
    { "strictus", detecta_strictum,
        { .ignora_spatium = 0, .ignora_casum = 0,
          .ignora_punctum = 0, .inversio_verborum = 0 } },
    { "mollis",   detecta_molle,
        { .ignora_spatium = 1, .ignora_casum = 1,
          .ignora_punctum = 1, .inversio_verborum = 0 } },
    { "verborum", detecta_verba,
        { .ignora_spatium = 0, .ignora_casum = 0,
          .ignora_punctum = 0, .inversio_verborum = 1 } },
};
#define NUMEROSITAS_MODORUM ((int)(sizeof(modi) / sizeof(modi[0])))

/* ===== Generator exemplorum per ordinem inversum — demonstratio ===== */

/*
 * Recursiva: scribe chordam in ordine inverso ad exitum.
    * recursio terminatur cum lon == 0.
 */
static void
scribe_inversam(const char *s, int lon, FILE *fp)
{
    if (lon <= 0)
        return;
    fputc(s[lon - 1], fp);
    scribe_inversam(s, lon - 1, fp);
}

/* Notator variadicus cum gradu praefixo — similitudo printf */
static void
nota(const char *lvl, const char *fmt, ...)
{
    va_list ap;
    printf("  [%s] ", lvl);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}

/* ===== Exempla polyglotta ===== */

static const char *const exempla[] = {
    /* Latina classicae */
    "in girum imus nocte et consumimur igni",
    "signa te signa temere me tangis et angis",
    "roma tibi subito motibus ibit amor",
    /* Anglica */
    "A man a plan a canal Panama",
    "Was it a car or a cat I saw",
    "No lemon no melon",
    /* Germanica */
    "reittier",
    "Otto",
    /* Verborum */
    "nice to see you to nice",
    "first ladies rule the state and state the rule ladies first",
    /* Negativa */
    "hoc non est palindromus",
    "venti vidi vinci",
    NULL,
};

int
main(int argc, char *argv[])
{
    printf("Detector Palindromorum Polyglottus\n");
    printf("===================================\n");

    if (argc > 1) {
        /* coniunge argumenta in unam chordam */
        char coniunctum[2048];
        coniunctum[0] = '\0';
        for (int i = 1; i < argc; i++) {
            if (i > 1)
                strncat(coniunctum, " ",
                        sizeof(coniunctum) - strlen(coniunctum) - 1);
            strncat(coniunctum, argv[i],
                    sizeof(coniunctum) - strlen(coniunctum) - 1);
        }
        printf("\nChorda: \"%s\"\n", coniunctum);
        for (int m = 0; m < NUMEROSITAS_MODORUM; m++) {
            int r = modi[m].fn(coniunctum, modi[m].vex);
            nota(modi[m].nomen, "%s",
                 r ? "palindromus est" : "non palindromus");
        }
        printf("  inversa: ");
        scribe_inversam(coniunctum, (int)strlen(coniunctum), stdout);
        putchar('\n');
    } else {
        for (int i = 0; exempla[i]; i++) {
            printf("\n\"%s\"\n", exempla[i]);
            for (int m = 0; m < NUMEROSITAS_MODORUM; m++) {
                int r = modi[m].fn(exempla[i], modi[m].vex);
                printf("  %-8s: %s\n", modi[m].nomen, r ? "DA" : "NON");
            }
        }
    }
    return 0;
}
