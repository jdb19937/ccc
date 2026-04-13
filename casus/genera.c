/* genera.c — classificatio numerorum */
#include <stdio.h>

int main(void)
{
    int numerusCurrens, estPrim, divisor, summaDiv;
    int estPerfectus, radixQuad, estQuadratus;
    const char *classis;

    numerusCurrens = 1;

iterum:
    if (numerusCurrens > 50) goto finis;

    estPrim = 1;
    if (numerusCurrens < 2) { estPrim = 0; goto classifica; }
    for (divisor = 2; divisor * divisor <= numerusCurrens; divisor++)
        if (numerusCurrens % divisor == 0) { estPrim = 0; goto classifica; }

classifica:
    summaDiv = 0;
    for (divisor = 1; divisor < numerusCurrens; divisor++)
        if (numerusCurrens % divisor == 0) summaDiv += divisor;
    estPerfectus = (summaDiv == numerusCurrens && numerusCurrens > 1);

    radixQuad = 0;
    while (radixQuad * radixQuad < numerusCurrens) radixQuad++;
    estQuadratus = (radixQuad * radixQuad == numerusCurrens);

    classis = estPerfectus ? "perfectus"
            : estPrim      ? "primus"
            : estQuadratus ? "quadratus"
            : "vulgaris";

    printf("%3d: %s\n", numerusCurrens, classis);
    numerusCurrens++;
    goto iterum;

finis:
    return 0;
}
