/**
 ** probatio.c
 **
 ** valida numerum algorithmo Luhn
 **
 **/

#include <stdio.h>  /** lectura/scriptura **/
#include <string.h> /** pro strlen **/

/**
 ** duplica digitum, subtrahe 9 si > 9
 **/
int dupl(int d) /** d inter 0..9 **/
{
      int r = d * 2; /** duplicatum **/
      return r > 9 ? r - 9 : r; /** correctum **/
}

int main(void) /** initium **/
{
      /** numerus validandus **/
      const char *num = "79927398713";

      int summa = 0; /** summa currens **/
      int lon = (int)strlen(num); /** numerus digitorum **/
      int paritas = lon % 2; /** vexillum paritatis **/

      /** per digitos itera **/
      for (int i = 0; i < lon; i++) /** index **/
      {
            int d = num[i] - '0'; /** valor digiti **/

            if (i % 2 == paritas) /** positio par **/
                  d = dupl(d); /** duplica **/

            summa += d; /** accumula **/
      }

      /** proba exitum **/
      printf("%s: %s\n", /** forma **/
            num, /** numerus **/
            summa % 10 == 0 ? "validum" : "invalidum" /** iudicium **/
      ); /** finis printf **/

      return 0; /** successus **/
}
