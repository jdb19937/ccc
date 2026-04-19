/*
 * typus.c — utilia typorum
 */

#include "utilia.h"
#include "typus.h"
#include "fluat.h"

/* ================================================================
 * allocator typorum
 * ================================================================ */

static typus_t typi_area[MAX_TYPI];
static int typi_vertex = 0;

typus_t *typus_novus(int genus)
{
    if (typi_vertex >= MAX_TYPI)
        erratum("nimis multi typi");
    typus_t *t = &typi_area[typi_vertex++];
    memset(t, 0, sizeof(typus_t));
    t->genus = genus;
    t->num_elementorum = -1;
    return t;
}

/* ================================================================
 * typi praefiniti
 * ================================================================ */

typus_t *ty_void;
typus_t *ty_char;
typus_t *ty_uchar;
typus_t *ty_short;
typus_t *ty_ushort;
typus_t *ty_int;
typus_t *ty_uint;
typus_t *ty_long;
typus_t *ty_ulong;

void typus_initia(void)
{
    typi_vertex = 0;

    ty_void  = typus_novus(TY_VOID);
    ty_void->magnitudo = 0;
    ty_void->colineatio = 1;

    ty_char  = typus_novus(TY_CHAR);
    ty_char->magnitudo = 1;
    ty_char->colineatio = 1;

    ty_uchar = typus_novus(TY_UCHAR);
    ty_uchar->magnitudo = 1;
    ty_uchar->colineatio = 1;
    ty_uchar->est_sine_signo = 1;

    ty_short = typus_novus(TY_SHORT);
    ty_short->magnitudo = 2;
    ty_short->colineatio = 2;

    ty_ushort = typus_novus(TY_USHORT);
    ty_ushort->magnitudo = 2;
    ty_ushort->colineatio = 2;
    ty_ushort->est_sine_signo = 1;

    ty_int   = typus_novus(TY_INT);
    ty_int->magnitudo = 4;
    ty_int->colineatio = 4;

    ty_uint  = typus_novus(TY_UINT);
    ty_uint->magnitudo = 4;
    ty_uint->colineatio = 4;
    ty_uint->est_sine_signo = 1;

    ty_long  = typus_novus(TY_LONG);
    ty_long->magnitudo = 8;
    ty_long->colineatio = 8;

    ty_ulong = typus_novus(TY_ULONG);
    ty_ulong->magnitudo = 8;
    ty_ulong->colineatio = 8;
    ty_ulong->est_sine_signo = 1;
}

/* ================================================================
 * utilia typorum
 * ================================================================ */

typus_t *typus_indicem(typus_t *basis)
{
    typus_t *t    = typus_novus(TY_PTR);
    t->basis      = basis;
    t->magnitudo  = 8;
    t->colineatio = 8;
    return t;
}

typus_t *typus_tabulam(typus_t *basis, int num)
{
    if (!basis)
        erratum("typus_tabulam: basis nulla");
    typus_t *t = typus_novus(TY_ARRAY);
    t->basis = basis;
    t->num_elementorum = num;
    t->colineatio = basis->colineatio;
    t->magnitudo = basis->magnitudo * num;
    return t;
}

int typus_magnitudo(const typus_t *t)
{
    if (!t)
        return 0;
    return t->magnitudo;
}

int typus_colineatio(const typus_t *t)
{
    if (!t)
        return 1;
    return t->colineatio ? t->colineatio : 1;
}

/*
 * §6.2.5¶10: float et double sunt typi "real floating".
 * §6.2.5¶14: integer et real floating = typi arithmetici.
 */  
int typus_est_fluat(const typus_t *t)
{
    if (!t)
        return 0;
    return t->genus == TY_FLOAT || t->genus == TY_DOUBLE;
}


int typus_est_integer(const typus_t *t)
{
    if (!t)
        return 0;
    /* §6.2.5¶17: char, short, int, long, long long et sine-signō */
    return (t->genus >= TY_CHAR && t->genus <= TY_ULLONG);
}

int typus_est_index(const typus_t *t)
{
    if (!t)
        return 0;
    return t->genus == TY_PTR || t->genus == TY_ARRAY;
}

int typus_est_arithmeticus(const typus_t *t)
{
    /* §6.2.5¶18: integer et real floating = typi arithmetici */
    return typus_est_integer(t) || typus_est_fluat(t);
}

typus_t *typus_basis_indicis(typus_t *t)
{
    if (!t)
        return ty_void;
    if (t->genus == TY_PTR)
        return t->basis ? t->basis : ty_void;
    if (t->genus == TY_ARRAY)
        return t->basis ? t->basis : ty_void;
    return ty_void;
}

int est_unsigned(const typus_t *t)
{
    if (!t)
        return 0;
    return t->est_sine_signo ||
        t->genus == TY_UCHAR || t->genus == TY_USHORT ||
        t->genus == TY_UINT || t->genus == TY_ULONG ||
        t->genus == TY_ULLONG ||
        t->genus == TY_PTR || t->genus == TY_ARRAY;
}

int mag_typi(const typus_t *t)
{
    if (!t)
        return 8;
    if (t->genus == TY_PTR || t->genus == TY_ARRAY || t->genus == TY_FUNC)
        return 8;
    if (t->genus == TY_STRUCT)
        return t->magnitudo;
    return t->magnitudo ? t->magnitudo : 4;
}

/* magnitudo vera pro accessu tabulae — non reducit TY_ARRAY ad 8 */
int mag_typi_verus(const typus_t *t)
{
    if (!t)
        return 8;
    if (t->genus == TY_ARRAY)
        return t->magnitudo > 0 ? t->magnitudo : 8;
    return mag_typi(t);
}

/* §6.7.5.2: genera codicem quī magnitudinem totam typī (tabulae cum
 * VLA dimēnsiōnibus aut scālāris) in x_dest pōnit.  Redit 1 sī VLA
 * implicātur (dynamica), 0 sī tota magnitudo constans. */
int typus_habet_vla(const typus_t *t)
{
    while (t && t->genus == TY_ARRAY) {
        if (t->num_elementorum <= 0 && t->vla_dim)
            return 1;
        t = t->basis;
    }
    return 0;
}

/* §6.7.8: numera elementa plana quae in initializatore structurae
 * expectantur pro hoc typo. Recursivus pro sub-structuris.
 * char[N] cum chorda = 1, alii arietes = N scalaria, struct = summa. */
int numera_elementa_init(const typus_t *t)
{
    if (!t)
        return 1;
    if (
        t->genus == TY_ARRAY && t->basis
        && (t->basis->genus == TY_CHAR || t->basis->genus == TY_UCHAR)
    )
        return 1; /* char[N] = "..." — unum elementum */
    if (t->genus == TY_ARRAY && t->basis) {
        typus_t *f = t->basis;
        while (f->genus == TY_ARRAY && f->basis)
            f = f->basis;
        int f_mag = typus_magnitudo(f);
        if (f_mag < 1)
            erratum("numera_elementa_init: magnitudo folii invalida");
        return typus_magnitudo(t) / f_mag;
    }
    if (t->genus == TY_STRUCT && t->membra && t->num_membrorum > 0) {
        int summa = 0;
        for (int i = 0; i < t->num_membrorum; i++)
            summa += numera_elementa_init(t->membra[i].typus);
        return summa;
    }
    return 1; /* scalar */
}

/* §6.7.2.1: quaerit informātiōnem campī bitōrum prō membrō */
membrum_t *quaere_membrum(typus_t *st, const char *nomen)
{
    if (!st || !nomen || st->genus != TY_STRUCT)
        return NULL;
    for (int i = 0; i < st->num_membrorum; i++)
        if (strcmp(st->membra[i].nomen, nomen) == 0)
            return &st->membra[i];
    return NULL;
}
