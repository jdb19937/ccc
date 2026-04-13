#include <stdio.h>
#include <string.h>

#define LAT 79
#define GENERA 40

int main(void)
{
    int cellae[LAT]; int novae[LAT]; int* fons = cellae; int* dest = novae;
    memset(fons, 0, sizeof(cellae)); fons[LAT / 2] = 1;
    for (int t = 0; t < GENERA; t++) { for (int x = 0; x < LAT; x++) printf("%c", fons[x] ? '#' : ' '); printf("\n"); for (int x = 0; x < LAT; x++) { int lae = x > 0 ? fons[x - 1] : 0; int med = fons[x]; int dex = x < LAT - 1 ? fons[x + 1] : 0; dest[x] = (30 >> (lae * 4 + med * 2 + dex)) & 1; } int* temp = fons; fons = dest; dest = temp; }
    return 0;
}
