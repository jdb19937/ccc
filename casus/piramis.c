/* piramis.c — triangulum Sierpinski */
#include <stdio.h>
int main(void) {
 int n = 32;
 for (int y = 0; y < n; y++) {
  for (int s = 0; s < n - y - 1; s++) {
   printf(" "); }
  for (int x = 0; x <= y; x++) {
   printf("%c ", (y & x) == x ? '#' : ' '); }
  printf("\n"); }
 return 0; }
