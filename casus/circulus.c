/* circulus.c — problema Josephi */

    #include <stdio.h>

    #define IMAX 20
    #define IPASSUS 3

int main(void)
{
    int iNumero = IMAX;
    int iPassus = IPASSUS;
    int bVivus[iNumero];
    int iRestantes = iNumero;

        #if IPASSUS > 0
    for (int i = 0; i < iNumero; i++)
        bVivus[i] = 1;
        #endif

    int iPosicio = 0;
    int iOrdo = 0;

    printf("problema Josephi: %d homines, passus %d\n",
            iNumero, iPassus);

    while (iRestantes > 1) {
        int iComputatus = 0;
        while (iComputatus < iPassus) {
            if (bVivus[iPosicio])
                iComputatus++;
            if (iComputatus < iPassus)
                iPosicio = (iPosicio + 1) % iNumero;
        }
        bVivus[iPosicio] = 0;
        iRestantes--;
        iOrdo++;

            #ifdef IMAX
        printf("  %2d: homo %2d eliminatus\n", iOrdo, iPosicio + 1);
            #endif

        iPosicio = (iPosicio + 1) % iNumero;
    }

    for (int i = 0; i < iNumero; i++)
        if (bVivus[i])
            printf("superstes: homo %d\n", i + 1);

    return 0;
}
