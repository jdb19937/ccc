/* approximo.c — approximatio pi */
#include <stdio.h>

int main(void)
{
    double summa_partialis_seriei = 0.0;
    int index_termini_currentis = 0;
    int numerus_terminorum_total = 1000000;
    int signum_termini = 1;

    do {
        double denominator_termini_currentis = 2.0 * index_termini_currentis + 1.0;
        summa_partialis_seriei += signum_termini / denominator_termini_currentis;
        signum_termini = -signum_termini;
        index_termini_currentis++;
    } while (index_termini_currentis < numerus_terminorum_total);

    double approximatio_valoris_pi = 4.0 * summa_partialis_seriei;

    printf("Leibniz (%d termini): %.10f\n",
           numerus_terminorum_total,
           approximatio_valoris_pi);

    double summa_seriei_Nilakantha = 3.0;
    int index_iterationis = 1;
    int signum_iterationis = 1;

    do {
        double basis_producti_consecutivorum = 2.0 * index_iterationis;
        double productum_trium_consecutivorum = basis_producti_consecutivorum
            * (basis_producti_consecutivorum + 1.0)
            * (basis_producti_consecutivorum + 2.0);
        summa_seriei_Nilakantha += signum_iterationis * 4.0
            / productum_trium_consecutivorum;
        signum_iterationis = -signum_iterationis;
        index_iterationis++;
    } while (index_iterationis <= 100);

    printf("Nilakantha (%d termini): %.10f\n",
           index_iterationis - 1,
           summa_seriei_Nilakantha);

    return 0;
}
