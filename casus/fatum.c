/* fatum.c — algorithmus fati Conway */
#include <stdio.h>

const char *nomina[] =
  {
    "So" "lis",
    "Lu" "nae",
    "Mar" "tis",
    "Mercu" "rii",
    "Io" "vis",
    "Vene" "ris",
    "Satur" "ni"
  };

int fatum(int annus, int mensis, int dies)
  {
    int saeculum = annus / 100;
    int ancora = (2 + 5 * (saeculum % 4)) % 7;
    int a = annus % 100;
    int f = (a / 12 + a % 12 + (a % 12) / 4 + ancora) % 7;
    int bis = (annus % 4 == 0 && annus % 100 != 0)
              || annus % 400 == 0;
    int referens[] =
      {
        0, bis ? 4 : 3, bis ? 29 : 28,
        7, 4, 9, 6, 11, 8, 5, 10, 7, 12
      };
    return ((dies - referens[mensis]) % 7 + 7 + f) % 7;
  }

int main(void)
  {
    int datae[][3] =
      {
        {2026, 4, 4},
        {1776, 7, 4},
        {2000, 1, 1},
        {1969, 7, 20},
        {1945, 5, 8},
        {2012, 12, 21}
      };
    for (int i = 0; i < 6; i++)
      {
        int a = datae[i][0];
        int m = datae[i][1];
        int d = datae[i][2];
        int h = fatum(a, m, d);
        printf(
            "%04d" "-" "%02d" "-" "%02d"
            ": dies %s\n",
            a, m, d, nomina[h]
        );
      }
    return 0;
  }
