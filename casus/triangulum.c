#include <stdio.h>

int main(void)
   {
   int ordo[13] = {0};
   ordo[0] = 1;
   for (int n = 0; n < 13; n++)
      {
      for (int k = n; k > 0; k--)
         ordo[k] = ordo[k] + ordo[k-1];
      for (int k = 0; k <= n; k++)
         {
         if (k > 0) printf(" ");
         printf("%d", ordo[k]);
         }
      printf("\n");
      }
   return 0;
   }
