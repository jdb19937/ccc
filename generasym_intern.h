/*
 * generasym_intern.h — status et helperes communes inter
 * generasym.c et gsymexpr.c. NON est pars API publicae.
 */
#ifndef GENERASYM_INTERN_H
#define GENERASYM_INTERN_H

#include "parser.h"
#include "typus.h"
#include "emitte.h"

/* status registorum */
extern int reg_vertex;
int  reg_alloca(void);
void reg_libera(int r);
int  reg_arm(int slot);

/* status functionis currentis */
extern int      cur_frame_mag;
extern typus_t *cur_func_typus;
extern int      profunditas_vocationis;

/* alveorum helpers */
int gsym_chorda_adde(const char *data, int lon);
int gsym_globalis_adde(const char *nomen, typus_t *typus, int est_staticus, long valor);
int gsym_extern_adde(const char *nomen);
int gsym_label_novus(void);
int gsym_func_loc_quaere(const char *nomen);

/* inter-functiōnēs */
void genera_lval(nodus_t *n, int dest);
void genera_expr(nodus_t *n, int dest);
void genera_magnitudo_typi(typus_t *t, int dest_reg);
void genera_lectio_campi_bitorum(int r, membrum_t *mb);

/* status alveorum — exponitur ad gsymdata.c */
typedef struct {
    char nomen[256];
    int  label;
    int  est_staticus;
} gsym_func_loc_t;

#define GSYM_MAX_EXT 1024

typedef char gsym_extern_t[256];

extern chorda_lit_t     *gsym_chordae;
extern int               gsym_num_chordarum;
extern globalis_t       *gsym_globales;
extern int               gsym_num_globalium;
extern uint8_t          *gsym_init_data;
extern int               gsym_init_data_lon;
extern data_reloc_t     *gsym_data_relocs;
extern int               gsym_num_data_relocs;
extern gsym_func_loc_t  *gsym_func_loci;
extern int               gsym_num_func_loc;
extern gsym_extern_t    *gsym_externa;
extern int               gsym_num_externa;

void gsym_data_reloc_adde(int idata_offset, int genus, int target);
void gsymdata_aedifica_initiationes(nodus_t *radix);
void emitte_sectiones_datorum(void);

void gsymsent_initia(void);
void genera_functio(nodus_t *n);

extern nodus_t *staticae_locales[1024];
extern int      num_staticarum_localium;

#endif
