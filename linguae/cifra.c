/*
 * cifra.c — Encriptor et Decriptor Chordarum
 *
 * Applicat varias methodos cryptographicas classicas ad textum:
 * translationem Caesaris (simplex), Vigenerii (cum clave verbali),
 * atbash (reflectio), et rot13.  Functionat pro quolibet charactare
 * alphabetico; non-alphabetica praetermittuntur intacta.
 *
 * Temptat compilatorem per tabulam indicatorum functionum variadicarum,
 * uniones discriminatas pro clavibus, reditus structurae ex functione,
 * et expansionem macrorum cum coniunctione signorum.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#define MAX_TEXTUS  4096

/* ===== Genera clavis ===== */

typedef enum {
    CL_NULLA,     /* pro rot13, atbash */
    CL_NUMERICA,  /* Caesar translatio */
    CL_VERBALIS,  /* Vigenerius */
} GenusClavis;

typedef struct {
    GenusClavis genus;
    union {
        int numerus;
        const char *verbum;
    } valor;
} Clavis;

/* ===== Auxiliarium: transforma unum characterem ===== */

/* rotat litteram per n positiones, conservat casum, alia intacta */
static char
rota_char(char c, int n)
{
    if (c >= 'a' && c <= 'z')
        return (char)('a' + (((c - 'a') + n) % 26 + 26) % 26);
    if (c >= 'A' && c <= 'Z')
        return (char)('A' + (((c - 'A') + n) % 26 + 26) % 26);
    return c;
}

/* ===== Algorithmi ===== */

typedef void (*FunctioCifrae)(const char *, char *, Clavis, int decripta);

static void
cifra_caesaris(const char *intrat, char *exit_, Clavis cl, int decripta)
{
    int n = (cl.genus == CL_NUMERICA) ? cl.valor.numerus : 3;
    if (decripta) n = -n;
    for (int i = 0; intrat[i]; i++)
        exit_[i] = rota_char(intrat[i], n);
    exit_[strlen(intrat)] = '\0';
}

static void
cifra_rot13(const char *intrat, char *exit_, Clavis cl, int decripta)
{
    (void)cl; (void)decripta;
    for (int i = 0; intrat[i]; i++)
        exit_[i] = rota_char(intrat[i], 13);
    exit_[strlen(intrat)] = '\0';
}

/* Atbash: a<->z, b<->y, etc. Sua inversa. */
static void
cifra_atbash(const char *intrat, char *exit_, Clavis cl, int decripta)
{
    (void)cl; (void)decripta;
    for (int i = 0; intrat[i]; i++) {
        char c = intrat[i];
        if (c >= 'a' && c <= 'z')
            exit_[i] = (char)('a' + ('z' - c));
        else if (c >= 'A' && c <= 'Z')
            exit_[i] = (char)('A' + ('Z' - c));
        else
            exit_[i] = c;
    }
    exit_[strlen(intrat)] = '\0';
}

/*
 * Vigenerius: clavis verbalis, unusquisque characterem clavis dat translatio.
 * Clavis ciclatur si brevior textu.
 */
static void
cifra_vigenerii(const char *intrat, char *exit_, Clavis cl, int decripta)
{
    const char *clavis = (cl.genus == CL_VERBALIS) ? cl.valor.verbum : "clavis";
    int lon_cl = (int)strlen(clavis);
    if (lon_cl == 0) {
        strcpy(exit_, intrat);
        return;
    }
    int j = 0;
    for (int i = 0; intrat[i]; i++) {
        char c = intrat[i];
        if (isalpha((unsigned char)c)) {
            char k = clavis[j % lon_cl];
            int translatio = tolower((unsigned char)k) - 'a';
            if (decripta) translatio = -translatio;
            exit_[i] = rota_char(c, translatio);
            j++;
        } else {
            exit_[i] = c;
        }
    }
    exit_[strlen(intrat)] = '\0';
}

/* ===== Tabula distributionis — indicatores functionum */

static const struct {
    const char *nomen;
    FunctioCifrae fn;
    GenusClavis requirit;
} algorithmi[] = {
    { "caesar",   cifra_caesaris,  CL_NUMERICA },
    { "rot13",    cifra_rot13,     CL_NULLA },
    { "atbash",   cifra_atbash,    CL_NULLA },
    { "vigenere", cifra_vigenerii, CL_VERBALIS },
};
#define NUMEROSITAS_ALG \
    ((int)(sizeof(algorithmi) / sizeof(algorithmi[0])))

/* ===== Variadica: demonstratio per textus varios */

static void
demonstra(const char *nomen_algorithmi, FunctioCifrae fn, Clavis cl, ...)
{
    va_list ap;
    va_start(ap, cl);
    printf("\n=== %s ===\n", nomen_algorithmi);
    const char *textus;
    int idx = 1;
    while ((textus = va_arg(ap, const char *)) != NULL) {
        char enc[MAX_TEXTUS], dec[MAX_TEXTUS];
        fn(textus, enc, cl, 0);
        fn(enc, dec, cl, 1);
        printf("  [%d] intrat:  %s\n", idx, textus);
        printf("      enc: %s\n", enc);
        printf("      dec:  %s\n", dec);
        idx++;
    }
    va_end(ap);
}

/* ===== Exempla polyglotta */

int
main(int argc, char *argv[])
{
    printf("Cifrator Chordarum\n");
    printf("==================\n");

    if (argc >= 3) {
        /* usus: cifra <algorithm> <clavis> <textus> [...] */
        const char *nomen = argv[1];
        int idx_algorithmi = -1;
        for (int i = 0; i < NUMEROSITAS_ALG; i++) {
            if (strcmp(algorithmi[i].nomen, nomen) == 0) {
                idx_algorithmi = i;
                break;
            }
        }
        if (idx_algorithmi < 0) {
            fprintf(stderr, "Error: algorithmus ignotus '%s'\n", nomen);
            return 1;
        }
        Clavis cl = { CL_NULLA, { .numerus = 0 } };
        int ta = 2;
        if (algorithmi[idx_algorithmi].requirit == CL_NUMERICA) {
            char *finis;
            long n = strtol(argv[2], &finis, 10);
            if (*finis != '\0') {
                fprintf(stderr, "Error: clavis numerica invalida\n");
                return 1;
            }
            cl = (Clavis){ CL_NUMERICA, { .numerus = (int)n } };
            ta = 3;
        } else if (algorithmi[idx_algorithmi].requirit == CL_VERBALIS) {
            cl = (Clavis){ CL_VERBALIS, { .verbum = argv[2] } };
            ta = 3;
        }
        if (ta >= argc) {
            fprintf(stderr, "Error: textus deest\n");
            return 1;
        }
        for (int i = ta; i < argc; i++) {
            char enc[MAX_TEXTUS];
            algorithmi[idx_algorithmi].fn(argv[i], enc, cl, 0);
            printf("%s\n", enc);
        }
    } else {
        demonstra("caesar_3", cifra_caesaris,
            (Clavis){ CL_NUMERICA, { .numerus = 3 } },
            "venti vidi vinci",
            "Roma caput mundi",
            "hello world",
            "guten tag freunde",
            (const char *)NULL);

        demonstra("rot13", cifra_rot13,
            (Clavis){ CL_NULLA, { .numerus = 0 } },
            "ars longa vita brevis",
            "The quick brown fox",
            "Wenn ist das Nunstruck",
            (const char *)NULL);

        demonstra("atbash", cifra_atbash,
            (Clavis){ CL_NULLA, { .numerus = 0 } },
            "alphabeta reversum",
            "Caesar Augustus",
            (const char *)NULL);

        demonstra("vigenerius clavis='lemon'", cifra_vigenerii,
            (Clavis){ CL_VERBALIS, { .verbum = "lemon" } },
            "ATTACKATDAWN",
            "venti vidi vinci populis",
            "schoene tag in berlin",
            (const char *)NULL);

        printf("\nUsus: cifra <algorithm> [clavis] <textus>\n");
        printf("  algorithmi: caesar, rot13, atbash, vigenere\n");
    }
    return 0;
}
