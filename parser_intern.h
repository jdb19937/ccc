/*
 * parser_intern.h — declarationes communes inter parser.c et
 * fasciculos parser_*.c. NON est pars API publicae.
 */
#ifndef PARSER_INTERN_H
#define PARSER_INTERN_H

#include "parser.h"

/* auxiliaria parsoris — definitiones in parser.c */
void expecta(int genus);
int  congruet(int genus);
int  est_specifier_typi(void);

/* ambitus currens — mutabilis directe a parser_sent.c (parse_declaratio) */
extern ambitus_t *cur_ambitus;

/* typi — in parser_typi.c */
typus_t *parse_specifiers(int *est_staticus, int *est_externus, int *est_typedef);
typus_t *parse_struct_vel_union(void);
typus_t *parse_enum(void);
typus_t *parse_declarator(typus_t *basis, char *nomen, int max_nomen);
typus_t *parse_parametros(typus_t *reditus, symbolum_t ***param_sym, int *num_param);

/* §6.7.5.2: ultima VLA expressio magnitudinis, posita per parse_declarator,
 * consumpta per parse_declaratio. */
extern nodus_t *ultima_vla_expr;

/* expressiones — in parser_expr.c */
nodus_t *parse_expr(void);
nodus_t *parse_expr_assign(void);
nodus_t *parse_expr_conditio(void);
int      constans_est(nodus_t *n);

/* sententiae et declarationes — in parser_sent.c */
nodus_t *parse_sententia(void);
nodus_t *parse_blocum(void);
nodus_t *parse_declaratio(int est_globalis);
int      parse_init_elementa(
    nodus_t **elems, int *nelem, int max,
    typus_t *t, int base_off
);

#endif
