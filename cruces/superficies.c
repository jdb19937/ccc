/*
 * superficies.c — Classificatio Superficierum per Characteristicam Euleri
 *
 * Classificat superficies clausas per genus, orientabilitatem,
 * et characteristicam Euleri. Demonstrat algebram typorum C99.
 * Cruciatus compilatoris: catenae typedef complexae,
 * qualificationes const/volatile/restrict in combinationibus,
 * indicatores ad indicatores cum qualificationibus,
 * typedef indicatorum functionum qualificatorum.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== Catenae typedef ===== */

/* Gradus 1: typus fundamentalis */
typedef int Dimensio;
typedef int Characteristica;
typedef int Genus;

/* Gradus 2: typedef qualificatus */
typedef const Dimensio DimensioConstans;
typedef volatile int NumerusVolatilis;

/* Gradus 3: typedef indicatoris */
typedef Characteristica *IndicatorCharacteristicae;
typedef const Characteristica *IndicatorConstantisCharacteristicae;

/* Gradus 4: typedef indicatoris ad indicatorem cum qualificationibus */
typedef const Characteristica *const *IndicatorConstAdConstChar;

/* Gradus 5: typedef functionis indicatae */
typedef Characteristica (*FunctioCharacteristicae)(Genus);

/* Gradus 6: aries functionum indicatarum */
typedef FunctioCharacteristicae TabulaFunctionum[4];

/* ===== Structurae cum qualificationibus complexis ===== */

typedef struct {
    const char *const nomen;  /* indicatorem mutare non licet */
    DimensioConstans dimensio;
    Genus genus;
    int est_orientabilis;
    Characteristica chi;
    const char *descriptio;
} SuperficiesConstans;

/*
 * Structura cum membro volatile — compilator non debet
 * accessus ad hoc membrum optimizare.
 */
typedef struct {
    const char *nomen;
    NumerusVolatilis computationes_factae;
    Characteristica ultima_characteristica;
} CacheComputationis;

/* ===== Functiones cum qualificationibus parametrorum ===== */

/*
 * 'restrict' in duobus parametris: compilatori promittit
 * indicatores non aliasari — optimizatio possibilis.
 */
static void
computa_characteristicas(
    Characteristica * restrict eventus,
    const Genus * restrict genera,
    int numerositas,
    FunctioCharacteristicae functio
) {
    for (int i = 0; i < numerositas; i++)
        eventus[i] = functio(genera[i]);
}

/* Characteristica superficiei orientabilis: chi = 2 - 2g */
static Characteristica
chi_orientabilis(Genus g)
{
    return 2 - 2 * g;
}

/* Characteristica superficiei non-orientabilis: chi = 2 - k */
static Characteristica
chi_non_orientabilis(Genus k)
{
    return 2 - k;
}

/* ===== Tabula superficierum ===== */

/*
 * Designatores initializationis cum qualificationibus const.
 * Tota tabula est const — in sectione read-only memoriae.
 */
static const SuperficiesConstans superficies_orientabiles[] = {
    {
        .nomen = "Sphaera S^2",
        .dimensio = 2, .genus = 0, .est_orientabilis = 1,
        .chi = 2, .descriptio = "superficies simplicissima"
    },
    {
        .nomen = "Torus T^2",
        .dimensio = 2, .genus = 1, .est_orientabilis = 1,
        .chi = 0, .descriptio = "annulus toroidalis"
    },
    {
        .nomen = "Bitorus",
        .dimensio = 2, .genus = 2, .est_orientabilis = 1,
        .chi = -2, .descriptio = "summa connexa duorum tororum"
    },
    {
        .nomen = "Tritorus",
        .dimensio = 2, .genus = 3, .est_orientabilis = 1,
        .chi = -4, .descriptio = "summa connexa trium tororum"
    },
};

static const SuperficiesConstans superficies_non_orientabiles[] = {
    {
        .nomen = "Planum proiectivum RP^2",
        .dimensio = 2, .genus = 1, .est_orientabilis = 0,
        .chi = 1, .descriptio = "crosscap singularis"
    },
    {
        .nomen = "Amphora Kleinii",
        .dimensio = 2, .genus = 2, .est_orientabilis = 0,
        .chi = 0, .descriptio = "duo crosscaps"
    },
    {
        .nomen = "Dymond",
        .dimensio = 2, .genus = 3, .est_orientabilis = 0,
        .chi = -1, .descriptio = "tres crosscaps"
    },
};

#define NUMEROSITAS_ORIENTABILIUM \
    ((int)(sizeof(superficies_orientabiles) / sizeof(superficies_orientabiles[0])))
#define NUMEROSITAS_NON_ORIENTABILIUM \
    ((int)(sizeof(superficies_non_orientabiles) / sizeof(superficies_non_orientabiles[0])))

/* ===== Demonstratio typorum ===== */

/*
 * Functio cum indicatore ad indicatorem qualificatum.
 * const char *const *pp — indicatorem ad (const indicatorem ad const char).
 */
static void
scribe_nomina(const char *const *nomina, int numerositas)
{
    for (int i = 0; i < numerositas; i++)
        printf("  %d. %s\n", i + 1, nomina[i]);
}

/*
 * Functio quae functionem indicatam redit —
 * typedef catena in usu.
 */
static FunctioCharacteristicae
selige_formulam(int est_orientabilis)
{
    return est_orientabilis ? chi_orientabilis : chi_non_orientabilis;
}

int
main(void)
{
    printf("Classificatio Superficierum Clausarum\n\n");

    /* ===== Informationes de typis ===== */
    printf("Magnitudines typorum (catenae typedef):\n");
    printf("  sizeof(Dimensio) = %zu\n", sizeof(Dimensio));
    printf("  sizeof(Characteristica) = %zu\n", sizeof(Characteristica));
    printf(
        "  sizeof(IndicatorCharacteristicae) = %zu\n",
        sizeof(IndicatorCharacteristicae)
    );
    printf(
        "  sizeof(IndicatorConstAdConstChar) = %zu\n",
        sizeof(IndicatorConstAdConstChar)
    );
    printf(
        "  sizeof(FunctioCharacteristicae) = %zu\n",
        sizeof(FunctioCharacteristicae)
    );
    printf(
        "  sizeof(TabulaFunctionum) = %zu\n",
        sizeof(TabulaFunctionum)
    );
    printf(
        "  sizeof(SuperficiesConstans) = %zu\n",
        sizeof(SuperficiesConstans)
    );
    printf(
        "  sizeof(CacheComputationis) = %zu\n\n",
        sizeof(CacheComputationis)
    );

    /* ===== Superficies orientabiles ===== */
    printf("Superficies Orientabiles (genus g, chi = 2 - 2g):\n");
    printf(
        "%-30s %5s %5s %5s  %s\n",
        "Nomen", "dim", "genus", "chi", "Descriptio"
    );
    printf(
        "%-30s %5s %5s %5s  %s\n",
        "------------------------------", "-----", "-----", "-----",
        "-------------------"
    );

    int errores = 0;
    for (int i = 0; i < NUMEROSITAS_ORIENTABILIUM; i++) {
        const SuperficiesConstans *s = &superficies_orientabiles[i];
        Characteristica chi_comp     = chi_orientabilis(s->genus);

        printf(
            "%-30s %5d %5d %5d  %s",
            s->nomen, s->dimensio, s->genus, s->chi,
            s->descriptio
        );

        if (chi_comp != s->chi) {
            printf("  FALLACIA!");
            errores++;
        }
        putchar('\n');
    }

    /* ===== Superficies non-orientabiles ===== */
    printf("\nSuperficies Non-orientabiles (crosscap k, chi = 2 - k):\n");
    for (int i = 0; i < NUMEROSITAS_NON_ORIENTABILIUM; i++) {
        const SuperficiesConstans *s = &superficies_non_orientabiles[i];
        Characteristica chi_comp     = chi_non_orientabilis(s->genus);

        printf(
            "  %-28s genus=%d chi=%d  %s",
            s->nomen, s->genus, s->chi, s->descriptio
        );

        if (chi_comp != s->chi) {
            printf("  FALLACIA!");
            errores++;
        }
        putchar('\n');
    }

    /* ===== Demonstratio catenae typedef ===== */
    printf("\nComputatio per catenam typedef:\n");

    /* Aries generum */
    Genus genera[] = {0, 1, 2, 3, 4, 5};
    int numerositas = (int)(sizeof(genera) / sizeof(genera[0]));
    Characteristica characteristicae[6];

    /* Functio indicata per typedef selecta */
    FunctioCharacteristicae formula = selige_formulam(1);
    computa_characteristicas(characteristicae, genera, numerositas, formula);

    printf("  Orientabiles: ");
    for (int i = 0; i < numerositas; i++)
        printf("g=%d->chi=%d  ", genera[i], characteristicae[i]);
    putchar('\n');

    formula = selige_formulam(0);
    computa_characteristicas(characteristicae, genera, numerositas, formula);

    printf("  Non-orient.:  ");
    for (int i = 0; i < numerositas; i++)
        printf("k=%d->chi=%d  ", genera[i], characteristicae[i]);
    putchar('\n');

    /* ===== Indicatores ad indicatores ===== */
    printf("\nNomina superficierum per const char *const *:\n");
    const char *const nomina_or[] = {
        "Sphaera", "Torus", "Bitorus", "Tritorus"
    };
    scribe_nomina(nomina_or, 4);

    /* ===== Cache cum volatile ===== */
    CacheComputationis cache;
    cache.nomen = "cache computationis";
    cache.computationes_factae = 0;
    cache.ultima_characteristica = 0;

    for (int g = 0; g <= 100; g++) {
        cache.ultima_characteristica = chi_orientabilis(g);
        cache.computationes_factae++;  /* volatile: non optimizatur */
    }

    printf(
        "\nCache (volatile): %d computationes, "
        "ultima chi = %d\n",
        (int)cache.computationes_factae,
        cache.ultima_characteristica
    );

    if (errores > 0) {
        fprintf(stderr, "Error: %d verificiones falluerunt\n", errores);
        return 1;
    }

    printf("\nClassificatio perfecta.\n");
    return 0;
}
