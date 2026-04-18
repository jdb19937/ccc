/*
 * biblio.h — tractatio vexillorum -L, -l
 */

#ifndef BIBLIO_H
#define BIBLIO_H

/* ================================================================
 * viae bibliothecarum (-L)
 * ================================================================ */

extern char **viae_biblio;
extern int    num_viarum_biblio;

void biblio_via_adde(const char *via);
void res_adde(const char *via, int genus);

/* ================================================================
 * bibliothecae (-l)
 * ================================================================ */

/* genus bibliothecae resolutae */
enum { BIBLIO_A = 0, BIBLIO_DYLIB = 1 };

typedef struct {
    char *via;      /* via plena resolutae bibliothecae */
    int   genus;    /* BIBLIO_A vel BIBLIO_DYLIB */
} biblio_res_t;

extern biblio_res_t *biblio_res;
extern int            num_biblio_res;

/* resolve -l nomen, adde ad biblio_res */
void biblio_adde(const char *nomen);

/* resolve -framework nomen, adde ad biblio_res */
void biblio_framework_adde(const char *nomen);

/*
 * biblio_extrahe_objecta — extrahit .o ex archivis .a,
 * scribit in plicas temporarias.
 * reddit tabulam viarum (malloc) et *numerum.
 * vocans liberet vias et tabulam.
 */
char **biblio_extrahe_objecta(int *numerum);

/* removet plicas temporarias creatas ab biblio_extrahe_objecta */
void biblio_purga_temporarias(char **viae, int numerus);

/* numerus bibliothecarum dynamicarum */
int biblio_num_dylib(void);

/* via n-esimae bibliothecae dynamicae (0-indexata) */
const char *biblio_dylib_via(int index);

#endif /* BIBLIO_H */
