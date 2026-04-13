#include <stdio.h>
// -- numeri felices: summa quadratorum digitorum tandem ad 1 pervenit --

int summa_qd(int n) // computa summam quadratorum digitorum
{ int s = 0; // coacervator
  while (n > 0) // per digitos
  { int d = n % 10; // extrahe digitum
    s += d * d; // quadra et adde
    n /= 10; } // proximus digitus
  return s; } // factum

int est_felix(int n) // proba si n numerus felix est
{ int lentus = n; // testudo
  int celer = n; // lepus
  do { lentus = summa_qd(lentus); // unus gradus
       celer = summa_qd(summa_qd(celer)); // duo gradus
  } while (lentus != celer); // donec cyclus
  return lentus == 1; } // felix si cyclus ad 1

int main(void) // initium
{ int inventi = 0; // numerus
  printf("numeri felices sub 500:");
  for (int i = 1; // ab uno
       i < 500; // limen
       i++) // incrementum
  { if (est_felix(i)) // proba
    { printf(" %d", i); // scribe
      inventi++; } } // numera
  printf("\n(%d totales)\n", // summa
         inventi); // numerus
  return 0; } // exitus
