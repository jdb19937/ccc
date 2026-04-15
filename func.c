/*
 * func.c — tabula functionum localium
 */

#include "ccc.h"
#include "func.h"

func_loc_t func_loci[MAX_GLOBALES];
int num_func_loc = 0;

int func_loc_quaere(const char *nomen)
{
    for (int i = 0; i < num_func_loc; i++)
        if (strcmp(func_loci[i].nomen, nomen) == 0)
            return func_loci[i].label;
    return -1;
}

int func_loc_adde(const char *nomen, int est_staticus)
{
    int lab = label_novus();
    strncpy(func_loci[num_func_loc].nomen, nomen, 255);
    func_loci[num_func_loc].label        = lab;
    func_loci[num_func_loc].est_staticus = est_staticus;
    num_func_loc++;
    return lab;
}
