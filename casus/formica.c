/* formica.c — formica Langton in toro */
#include <stdio.h>
#include <string.h>

#define LAT 20
#define ALT 20
#define PASSUS 300

typedef struct
{
    int x;
    int y;
    int dir;
} formica_t;

int main(void)
{
    int tabula[ALT][LAT];
    memset(tabula, 0, sizeof(tabula));

    formica_t f = (formica_t)
    {
        .x = LAT / 2,
        .y = ALT / 2,
        .dir = 0
    };

    int delta[][2] =
    {
        [0] = { 0, -1},
        [1] = { 1,  0},
        [2] = { 0,  1},
        [3] = {-1,  0}
    };

    for (int t = 0; t < PASSUS; t++)
    {
        switch (tabula[f.y][f.x])
        {
            case 0:
                f.dir = (f.dir + 1) % 4;
                break;
            case 1:
                f.dir = (f.dir + 3) % 4;
                break;
        }
        tabula[f.y][f.x] = 1 - tabula[f.y][f.x];
        f.x = (f.x + delta[f.dir][0] + LAT) % LAT;
        f.y = (f.y + delta[f.dir][1] + ALT) % ALT;
    }

    int nigrae = 0;
    for (int y = 0; y < ALT; y++)
    {
        for (int x = 0; x < LAT; x++)
        {
            printf("%c", (x == f.x && y == f.y) ? '@' : tabula[y][x] ? '#' : '.');
            nigrae += tabula[y][x];
        }
        printf("\n");
    }
    printf("%d cellae nigrae post %d passus\n", nigrae, PASSUS);

    return 0;
}
