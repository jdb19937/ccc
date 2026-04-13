/* planum.c — distantiae in toro plano */
#include <stdio.h>

int main(void)
{
	int n = 8; int fons_x = 0; int fons_y = 0;
	printf("distantiae ab (%d,%d) in toro %dx%d:\n", fons_x, fons_y, n, n);
	for (int y = 0; y < n; y++) {
    for (int x = 0; x < n; x++) {
		int dx = x < n - x ? x : n - x; int dy = y < n - y ? y : n - y;
        printf("%2d ", dx + dy);
	}
    printf("\n");
	}
	printf("maxima distantia: %d\n", n / 2 + n / 2);
	return 0;
}
