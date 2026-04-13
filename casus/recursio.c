/* recursio.c — functio Ackermann */
#include <stdio.h>

int ackermann(int m, int n)
{
    if (m == 0) return n + 1;
    if (n == 0) return ackermann(m - 1, 1);
    return ackermann(m - 1, ackermann(m, n - 1));
}

void computa(int m, int n, int * exitus)
{
    * exitus = ackermann(m, n);
}

int main(void)
{
    int limes_m  = 4;
    int limes_n  = 7;
    int latitudo = 6;
    int valor    = 0;
    int * cursor = &valor;

    for (int m = 0; m < limes_m; m++) {
        for (int n = 0; n < limes_n; n++) {
            computa(m, n, cursor);
            printf("%*d", latitudo, * cursor);
        }
        printf("\n");
    }

    return 0;
}
