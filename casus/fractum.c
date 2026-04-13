/* fractum.c — Mandelbrot in arte ASCII */
#include <stdio.h>
int main(void)
{
  int h = 30, w = 70, m = 80;
  for (int j = 0; j < h; j++) {
    for (int i = 0; i < w; i++) {
      double a = (i - 47.0) / 23.0;
      double b = (j - 15.0) / 12.0;
      double x = 0, y = 0;
      int k = 0;
      while (k < m && x * x + y * y < 4.0) {
        double t = x * x - y * y + a;
        y = 2.0 * x * y + b;
        x = t;
        k++;
      }
      printf("%c", k >= m ? '#' : " .:-=+*%@"[k % 9]);
    }
    printf("\n");
  }
  return 0;
}
