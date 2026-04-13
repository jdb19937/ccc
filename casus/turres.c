/*+--------------------------------------+*/
/*| turres.c — turres Hanoi              |*/
/*+--------------------------------------+*/
#include <stdio.h>

int /* numerus motuum */ tot = 0;

/*+--------------------------------------+*/
/*| move discum de columna in columnam   |*/
/*+--------------------------------------+*/
void /* nihil */ move(
        int /* disci movendi */ n,
        char /* fons */ a,
        char /* destinatio */ b,
        char /* auxilium */ c)
{
        if /* basis */ (n == 0)
                return /* nihil */;
        move(n - 1, a, c, b);
        tot /* incrementum */++;
        printf("  %d: %c -> %c\n",
                tot /* numerus */,
                a /* unde */,
                b /* quo */);
        move(n - 1, c, b, a);
}

/*+--------------------------------------+*/
/*| principale                           |*/
/*+--------------------------------------+*/
int /* exitus */ main(void /* nullae */)
{
        int /* disci */ n = 4;
        printf("turres Hanoi, %d disci:\n",
                n /* numerus discorum */);
        move(n, 'A', 'C', 'B');
        printf("%d motus\n",
                tot /* motus totales */);
        return /* successus */ 0;
}
