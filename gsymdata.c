/*
 * gsymdata.c — emissio sectionum datorum (__cstring, __data, __bss)
 * post omnes functiones generatas. Extractum ex generasym.c.
 */

#include "utilia.h"
#include "parser.h"
#include "emittesym.h"
#include "generasym_intern.h"
#include "gsymconst.h"

#include <string.h>

/* §6.6: emitte relocationem pro identificatore in initializatore
 * (sive functio, sive globale definitum, sive externum). */
static void emitte_ident_reloc(int off, nodus_t *id)
{
    int flabel = gsym_func_loc_quaere(id->nomen);
    if (flabel >= 0) {
        gsym_data_reloc_adde(off, DR_TEXT, flabel);
    } else if (
        id->sym && id->sym->globalis_index >= 0
        && !gsym_globales[id->sym->globalis_index].est_bss
    ) {
        gsym_data_reloc_adde(
            off, DR_IDATA,
            gsym_globales[id->sym->globalis_index].data_offset
        );
    } else {
        int ei = gsym_extern_adde(id->nomen);
        gsym_data_reloc_adde(off, DR_EXT_FUNC, ei);
    }
}

static void emitte_quad_reloc(data_reloc_t *dr)
{
    switch (dr->genus) {
    case DR_CSTRING:
        {
            /* dr->target = index chordae (sid) — conventiō nostra in
             * modō symbolicō; cfr. gsym_data_reloc_adde(..., DR_CSTRING, sid) */
            esym_quad_str(dr->target);
            return;
        }
    case DR_IDATA:
        {
            /* target = data_offset globalis. Quaere quem globalem. */
            for (int i = 0; i < gsym_num_globalium; i++) {
                if (!gsym_globales[i].est_bss && gsym_globales[i].data_offset == dr->target) {
                    esym_quad_sym(gsym_globales[i].nomen, 0);
                    return;
                }
            }
            erratum("DR_IDATA: target %d non invenitur in globalibus", dr->target);
        }
    case DR_TEXT:
        {
            /* target = label functionis. Quaere func_loc. */
            for (int i = 0; i < gsym_num_func_loc; i++)
                if (gsym_func_loci[i].label == dr->target) {
                esym_quad_sym(gsym_func_loci[i].nomen, 0);
                return;
            }
            erratum("DR_TEXT: label %d non invenitur in func_loci", dr->target);
        }
    case DR_EXT_FUNC:
        {
            /* target = index in gsym_externa */
            if (dr->target < 0 || dr->target >= gsym_num_externa)
                erratum("DR_EXT_FUNC: target invalidus: %d", dr->target);
            /* gsym_externa stores nomen sine _ praefix */
            esym_quad_sym(gsym_externa[dr->target], 0);
            return;
        }
    default:
        erratum("relocatio datorum genus ignotum: %d", dr->genus);
    }
}

/* scribe byti range in init_data, intermiscens .quad relocationes */
static void scribe_datum_regionem(int data_off, int mag)
{
    /* collige relocationes in regione ordine offset */
    int end    = data_off + mag;
    int cursor = data_off;
    while (cursor < end) {
        /* quaere proximam relocationem */
        int next_r   = -1;
        int next_off = end;
        for (int i = 0; i < gsym_num_data_relocs; i++) {
            int o = gsym_data_relocs[i].idata_offset;
            if (o >= cursor && o < end && o < next_off) {
                next_off = o;
                next_r   = i;
            }
        }
        /* scribe byti ante relocationem ut .byte */
        while (cursor < next_off) {
            esym_byte(gsym_init_data[cursor]);
            cursor++;
        }
        if (next_r < 0)
            break;
        /* emitte .quad ad symbolum */
        emitte_quad_reloc(&gsym_data_relocs[next_r]);
        cursor += 8;
    }
}

static int log2_floor(int n)
{
    int k = 0;
    while ((1 << k) < n)
        k++;
    return k;
}

void emitte_sectiones_datorum(void)
{
    /* ordo: __cstring primum, deinde __data, deinde __bss.
     * imm requirit ut offsetos chordarum nota sint ante
     * processandum .quad relocationes in __data. */

    /* __cstring */
    if (gsym_num_chordarum > 0) {
        esym_sectio_cstring();
        for (int i = 0; i < gsym_num_chordarum; i++) {
            esym_str_label_pone(i);
            esym_asciz(gsym_chordae[i].data, gsym_chordae[i].longitudo);
        }
    }

    /* __data */
    int habet_data = 0;
    for (int i = 0; i < gsym_num_globalium; i++) {
        globalis_t *g = &gsym_globales[i];
        if (g->est_bss)
            continue;
        if (!habet_data) {
            esym_sectio_data();
            habet_data = 1;
        }
        int col   = g->colineatio > 0 ? g->colineatio : 1;
        int log2a = log2_floor(col);
        if (!g->est_staticus)
            esym_globl(g->nomen);
        if (log2a > 0)
            esym_p2align(log2a);
        esym_glob_label_pone(g->nomen);
        scribe_datum_regionem(g->data_offset, g->magnitudo);
    }

    /* __bss — utamur .zerofill directivā */
    for (int i = 0; i < gsym_num_globalium; i++) {
        globalis_t *g = &gsym_globales[i];
        if (!g->est_bss)
            continue;
        int col   = g->colineatio > 0 ? g->colineatio : 1;
        int log2a = log2_floor(col);
        if (!g->est_staticus)
            esym_globl(g->nomen);
        esym_zerofill("__DATA", "__bss", g->nomen, g->magnitudo, log2a);
    }
}

void gsymdata_aedifica_initiationes(nodus_t *radix)
{
    /* quarta passu: initiā globālēs (§6.7.8) — aedificā init_data bytes
     * et relocationēs intrā gsym_init_data. */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus != N_VAR_DECL || n->est_externus)
            continue;
        symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
        if (!s || s->globalis_index < 0)
            continue;
        int gid = s->globalis_index;

        int habet_init_data = 0;
        if (n->num_membrorum > 0 && n->membra)
            habet_init_data = 1;
        if (
            n->sinister && n->sinister->genus == N_STR &&
            n->typus_decl && (
                n->typus_decl->genus == TY_PTR
                || (
                    n->typus_decl->genus == TY_ARRAY
                    && n->typus_decl->basis
                    && (
                        n->typus_decl->basis->genus == TY_CHAR
                        || n->typus_decl->basis->genus == TY_UCHAR
                    )
                )
            )
        )
            habet_init_data = 1;
        if (n->sinister && n->sinister->genus == N_IDENT && n->sinister->nomen)
            habet_init_data = 1;
        if (n->sinister && typus_est_integer(n->typus_decl))
            habet_init_data = 1;
        if (
            n->sinister
            && (
                n->sinister->genus == N_NUM_FLUAT
                || (
                    n->sinister->genus == N_UNOP
                    && n->sinister->op == T_MINUS
                    && n->sinister->sinister
                    && n->sinister->sinister->genus == N_NUM_FLUAT
                )
            )
        )
            habet_init_data = 1;
        /* staticae sine initiāle: zerō-initiā in init_data */
        if (!habet_init_data && n->est_staticus) {
            int mag0 = gsym_globales[gid].magnitudo;
            if (mag0 < 1)
                continue;
            int col0 = gsym_globales[gid].colineatio;
            if (col0 < 1)
                col0 = 1;
            gsym_init_data_lon = (gsym_init_data_lon + col0 - 1) & ~(col0 - 1);
            int doff0 = gsym_init_data_lon;
            if (gsym_init_data_lon + mag0 > MAX_DATA)
                erratum("gsym_init_data nimis magna");
            memset(gsym_init_data + doff0, 0, mag0);
            gsym_init_data_lon += mag0;
            gsym_globales[gid] .est_bss     = 0;
            gsym_globales[gid] .data_offset = doff0;
            continue;
        }
        if (!habet_init_data)
            continue;

        int mag = gsym_globales[gid].magnitudo;
        if (mag < 1)
            erratum("globalis '%s' magnitudo invalida: %d", n->nomen, mag);
        int col = gsym_globales[gid].colineatio;
        if (col < 1)
            erratum("globalis '%s' colineatio invalida: %d", n->nomen, col);
        gsym_init_data_lon = (gsym_init_data_lon + col - 1) & ~(col - 1);
        int data_off       = gsym_init_data_lon;
        if (gsym_init_data_lon + mag > MAX_DATA)
            erratum("gsym_init_data nimis magna");
        memset(gsym_init_data + data_off, 0, mag);
        gsym_init_data_lon += mag;

        gsym_globales[gid] .est_bss     = 0;
        gsym_globales[gid] .data_offset = data_off;

        if (n->num_membrorum > 0 && n->membra) {
            /* §6.7.8: tabula vel strūctūra */
            typus_t *elem_t;
            int singula_structura = 0;
            if (
                n->typus_decl && n->typus_decl->genus == TY_ARRAY
                && n->typus_decl->basis
            )
                elem_t = n->typus_decl->basis;
            else if (n->typus_decl && n->typus_decl->genus == TY_STRUCT) {
                elem_t = n->typus_decl;
                singula_structura = 1;
            } else
                elem_t = ty_int;
            int elem_mag = typus_magnitudo(elem_t);
            if (elem_mag < 1)
                erratum("elementum tabulae '%s' magnitudo invalida", n->nomen);

            typus_t *folium_t = elem_t;
            if (singula_structura && folium_t->genus == TY_STRUCT) {
                if (folium_t->membra && folium_t->num_membrorum > 0)
                    folium_t = folium_t->membra[0].typus;
            }
            while (folium_t->genus == TY_ARRAY && folium_t->basis)
                folium_t = folium_t->basis;
            int folium_mag = typus_magnitudo(folium_t);
            if (folium_mag < 1)
                erratum("magnitudo folii invalida");

            int est_struct = (
                elem_t->genus == TY_STRUCT &&
                elem_t->membra && elem_t->num_membrorum > 0
            );
            int num_camp = est_struct ? elem_t->num_membrorum : 1;
            for (int j = 0; j < n->num_membrorum; j++) {
                nodus_t *elem = n->membra[j];
                int elem_off;
                int store_mag;
                int est_char_array = 0;
                if (elem->init_offset >= 0 && elem->init_size > 0) {
                    elem_off  = data_off + elem->init_offset;
                    store_mag = elem->init_size;
                    if (elem->genus == N_STR && elem->init_size > 8)
                        est_char_array = 1;
                } else if (singula_structura) {
                    elem_off  = data_off + j * folium_mag;
                    store_mag = folium_mag;
                } else if (est_struct) {
                    int idx_struct = j / num_camp;
                    int idx_camp   = j % num_camp;
                    typus_t        *camp_t = elem_t->membra[idx_camp].typus;
                    typus_t        *camp_fol = camp_t;
                    while (
                        camp_fol && camp_fol->genus == TY_ARRAY
                        && camp_fol->basis
                    )
                        camp_fol = camp_fol->basis;
                    int camp_fol_mag = typus_magnitudo(camp_fol);
                    if (camp_fol_mag < 1)
                        erratum("magnitudo folii campi invalida");
                    if (camp_t->genus == TY_ARRAY) {
                        int scal_per_struct = 0;
                        for (int mi = 0; mi < num_camp; mi++)
                            scal_per_struct += numera_elementa_init(elem_t->membra[mi].typus);
                        if (scal_per_struct < 1)
                            erratum("numerus scalarium per structuram invalidus");
                        int si      = j / scal_per_struct;
                        int sj      = j % scal_per_struct;
                        int run     = 0;
                        int mem_off = 0;
                        int sub_idx = sj;
                        for (int mi = 0; mi < num_camp; mi++) {
                            typus_t *mt = elem_t->membra[mi].typus;
                            int cnt = numera_elementa_init(mt);
                            int is_ch_arr = (
                                mt->genus == TY_ARRAY && mt->basis
                                && (
                                    mt->basis->genus == TY_CHAR
                                    || mt->basis->genus == TY_UCHAR
                                )
                            );
                            if (sj < run + cnt) {
                                sub_idx = sj - run;
                                mem_off = elem_t->membra[mi].offset;
                                if (is_ch_arr) {
                                    camp_fol_mag   = typus_magnitudo(mt);
                                    est_char_array = 1;
                                } else if (mt->genus == TY_STRUCT) {
                                    typus_t *sf = mt;
                                    while (
                                        sf->membra && sf->num_membrorum > 0
                                        && sf->genus == TY_STRUCT
                                    )
                                        sf = sf->membra[0].typus;
                                    while (sf->genus == TY_ARRAY && sf->basis)
                                        sf = sf->basis;
                                    camp_fol_mag = typus_magnitudo(sf);
                                    if (camp_fol_mag < 1)
                                        erratum("magnitudo folii sub-strūctūrae invalida");
                                } else {
                                    camp_fol_mag = (mt->genus == TY_ARRAY)
                                        ? typus_magnitudo(camp_fol) : mag_typi(mt);
                                }
                                break;
                            }
                            run += cnt;
                        }
                        elem_off = data_off + si * elem_mag
                            + mem_off + sub_idx * camp_fol_mag;
                        store_mag = camp_fol_mag;
                    } else {
                        elem_off = data_off + idx_struct * elem_mag
                            + elem_t->membra[idx_camp].offset;
                        store_mag = mag_typi(camp_t);
                        if (store_mag < 1)
                            erratum("campus structūrae magnitudo invalida");
                    }
                } else if (elem_t->genus == TY_ARRAY) {
                    elem_off  = data_off + j * folium_mag;
                    store_mag = folium_mag;
                } else {
                    elem_off  = data_off + j * elem_mag;
                    store_mag = elem_mag;
                }
                if (elem->genus == N_STR) {
                    if (est_struct && est_char_array) {
                        int lon = elem->lon_chordae < store_mag
                            ? elem->lon_chordae : store_mag;
                        memcpy(gsym_init_data + elem_off, elem->chorda, lon);
                        if (lon < store_mag)
                            memset(gsym_init_data + elem_off + lon, 0, store_mag - lon);
                    } else {
                        int sid = gsym_chorda_adde(elem->chorda, elem->lon_chordae);
                        gsym_data_reloc_adde(elem_off, DR_CSTRING, sid);
                    }
                } else if (elem->genus == N_NUM) {
                    long v = elem->valor;
                    if (elem->init_bitwidth > 0) {
                        unsigned int mask = elem->init_bitwidth >= 32
                            ? 0xffffffffu : ((1u << elem->init_bitwidth) - 1u);
                        unsigned int u;
                        memcpy(&u, gsym_init_data + elem_off, 4);
                        u &= ~(mask << elem->init_bitpos);
                        u |= ((unsigned int)v & mask) << elem->init_bitpos;
                        memcpy(gsym_init_data + elem_off, &u, 4);
                    } else {
                        memcpy(gsym_init_data + elem_off, &v, store_mag);
                    }
                } else if (
                    elem->genus == N_UNOP && elem->op == T_MINUS
                    && elem->sinister && elem->sinister->genus == N_NUM
                ) {
                    long v = -elem->sinister->valor;
                    memcpy(gsym_init_data + elem_off, &v, store_mag);
                } else if (
                    elem->genus == N_UNOP && elem->op == T_MINUS
                    && elem->sinister && elem->sinister->genus == N_NUM_FLUAT
                ) {
                    if (store_mag == 4) {
                        float fv = (float)-elem->sinister->valor_f;
                        memcpy(gsym_init_data + elem_off, &fv, 4);
                    } else {
                        double dv = -elem->sinister->valor_f;
                        memcpy(gsym_init_data + elem_off, &dv, store_mag);
                    }
                } else if (elem->genus == N_NUM_FLUAT) {
                    if (store_mag == 4) {
                        float fv = (float)elem->valor_f;
                        memcpy(gsym_init_data + elem_off, &fv, 4);
                    } else {
                        double dv = elem->valor_f;
                        memcpy(gsym_init_data + elem_off, &dv, store_mag);
                    }
                } else if (elem->genus == N_IDENT && elem->nomen) {
                    emitte_ident_reloc(elem_off, elem);
                } else if (
                    elem->genus == N_ADDR && elem->sinister
                    && elem->sinister->genus == N_IDENT
                    && elem->sinister->nomen
                ) {
                    emitte_ident_reloc(elem_off, elem->sinister);
                } else if (elem->typus && typus_est_integer(elem->typus)) {
                    long v = gsymconst_evalua_integer(elem);
                    if (elem->init_bitwidth > 0) {
                        unsigned int mask = elem->init_bitwidth >= 32
                            ? 0xffffffffu : ((1u << elem->init_bitwidth) - 1u);
                        unsigned int u;
                        memcpy(&u, gsym_init_data + elem_off, 4);
                        u &= ~(mask << elem->init_bitpos);
                        u |= ((unsigned int)v & mask) << elem->init_bitpos;
                        memcpy(gsym_init_data + elem_off, &u, 4);
                    } else {
                        memcpy(gsym_init_data + elem_off, &v, store_mag);
                    }
                } else if (
                    elem->typus && (
                        elem->typus->genus == TY_PTR
                        || typus_est_integer(elem->typus)
                    )
                ) {
                    long v = gsymconst_evalua_integer(elem);
                    memcpy(gsym_init_data + elem_off, &v, store_mag);
                } else if (
                    (elem->typus && typus_est_fluat(elem->typus))
                    || gsymconst_continet_fluat(elem)
                ) {
                    double dv = gsymconst_evalua_fluat(elem);
                    if (store_mag == 4) {
                        float fv = (float)dv;
                        memcpy(gsym_init_data + elem_off, &fv, 4);
                    } else {
                        memcpy(gsym_init_data + elem_off, &dv, store_mag);
                    }
                } else {
                    erratum_ad(
                        elem->linea,
                        "initializator aggregatus: genus nodi %d non tractatum",
                        elem->genus
                    );
                }
            }
        } else if (
            n->sinister && n->sinister->genus == N_STR
            && n->typus_decl && n->typus_decl->genus == TY_ARRAY
            && n->typus_decl->basis
            && (
                n->typus_decl->basis->genus == TY_CHAR
                || n->typus_decl->basis->genus == TY_UCHAR
            )
        ) {
            int lon    = n->sinister->lon_chordae;
            int cap    = gsym_globales[gid].magnitudo;
            int n_copy = lon < cap ? lon : cap;
            memcpy(gsym_init_data + data_off, n->sinister->chorda, n_copy);
            if (n_copy < cap)
                memset(gsym_init_data + data_off + n_copy, 0, cap - n_copy);
        } else if (n->sinister && n->sinister->genus == N_STR) {
            int sid = gsym_chorda_adde(n->sinister->chorda, n->sinister->lon_chordae);
            gsym_data_reloc_adde(data_off, DR_CSTRING, sid);
        } else if (
            n->sinister && n->sinister->genus == N_IDENT
            && n->sinister->nomen
        ) {
            emitte_ident_reloc(data_off, n->sinister);
        } else if (
            n->sinister && n->sinister->genus == N_ADDR
            && n->sinister->sinister
            && n->sinister->sinister->genus == N_IDENT
            && n->sinister->sinister->nomen
        ) {
            emitte_ident_reloc(data_off, n->sinister->sinister);
        } else if (n->sinister && n->sinister->genus == N_NUM_FLUAT) {
            int store_mag = gsym_globales[gid].magnitudo;
            if (store_mag == 4) {
                float fv = (float)n->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &fv, 4);
            } else {
                double dv = n->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &dv, store_mag > 8 ? 8 : store_mag);
            }
        } else if (
            n->sinister && n->sinister->genus == N_UNOP
            && n->sinister->op == T_MINUS && n->sinister->sinister
            && n->sinister->sinister->genus == N_NUM_FLUAT
        ) {
            int store_mag = gsym_globales[gid].magnitudo;
            if (store_mag == 4) {
                float fv = (float)-n->sinister->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &fv, 4);
            } else {
                double dv = -n->sinister->sinister->valor_f;
                memcpy(gsym_init_data + data_off, &dv, store_mag > 8 ? 8 : store_mag);
            }
        } else if (n->sinister && typus_est_integer(n->typus_decl)) {
            long v        = gsymconst_evalua_integer(n->sinister);
            int store_mag = gsym_globales[gid].magnitudo;
            if (store_mag > 8)
                store_mag = 8;
            memcpy(gsym_init_data + data_off, &v, store_mag);
        } else {
            erratum("initiālizātor globālis nōn tractātus prō '%s'", n->nomen);
        }
    }

    /* staticae locālēs cum initiālizātōribus */
    for (int si = 0; si < num_staticarum_localium; si++) {
        nodus_t    *n    = staticae_locales[si];
        symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
        if (!s || s->globalis_index < 0)
            continue;
        int gid = s->globalis_index;

        if (!n->num_membrorum || !n->membra) {
            int mag0 = gsym_globales[gid].magnitudo;
            if (mag0 < 1)
                continue;
            int col0 = gsym_globales[gid].colineatio;
            if (col0 < 1)
                col0 = 1;
            gsym_init_data_lon = (gsym_init_data_lon + col0 - 1) & ~(col0 - 1);
            int doff0 = gsym_init_data_lon;
            if (gsym_init_data_lon + mag0 > MAX_DATA)
                erratum("gsym_init_data nimis magna (static local BSS)");
            memset(gsym_init_data + doff0, 0, mag0);
            /* scalaris initialis: scribe valorem */
            if (n->sinister) {
                if (n->sinister->genus == N_STR) {
                    int sid = gsym_chorda_adde(
                        n->sinister->chorda, n->sinister->lon_chordae
                    );
                    gsym_data_reloc_adde(doff0, DR_CSTRING, sid);
                } else if (n->sinister->genus == N_IDENT && n->sinister->nomen) {
                    int flabel = gsym_func_loc_quaere(n->sinister->nomen);
                    if (flabel >= 0)
                        gsym_data_reloc_adde(doff0, DR_TEXT, flabel);
                    else
                        erratum_ad(
                            n->linea,
                            "staticae localis initiatio per identificatorem "
                            "non functionem non sustentatur: '%s'",
                            n->sinister->nomen
                        );
                } else if (
                    typus_est_integer(n->typus_decl)
                    || (n->typus_decl && n->typus_decl->genus == TY_PTR)
                ) {
                    long v = evalua_constans(n->sinister);
                    memcpy(gsym_init_data + doff0, &v, mag0 > 8 ? 8 : mag0);
                } else if (typus_est_fluat(n->typus_decl)) {
                    double dv = gsymconst_evalua_fluat(n->sinister);
                    if (mag0 == 4) {
                        float fv = (float)dv;
                        memcpy(gsym_init_data + doff0, &fv, 4);
                    } else {
                        memcpy(gsym_init_data + doff0, &dv, 8);
                    }
                } else {
                    erratum_ad(
                        n->linea,
                        "initializator staticae localis non tractatur"
                    );
                }
            }
            gsym_init_data_lon += mag0;
            gsym_globales[gid] .est_bss     = 0;
            gsym_globales[gid] .data_offset = doff0;
            continue;
        }

        int mag = gsym_globales[gid].magnitudo;
        if (mag < 1)
            continue;
        int col = gsym_globales[gid].colineatio;
        if (col < 1)
            col = 1;
        gsym_init_data_lon = (gsym_init_data_lon + col - 1) & ~(col - 1);
        int data_off       = gsym_init_data_lon;
        if (gsym_init_data_lon + mag > MAX_DATA)
            erratum("gsym_init_data nimis magna (static local)");
        memset(gsym_init_data + data_off, 0, mag);
        gsym_init_data_lon += mag;

        gsym_globales[gid] .est_bss     = 0;
        gsym_globales[gid] .data_offset = data_off;

        typus_t *sl_t   = n->typus_decl;
        typus_t *sl_fol = sl_t;
        while (sl_fol && (sl_fol->genus == TY_ARRAY || sl_fol->genus == TY_STRUCT)) {
            if (sl_fol->genus == TY_ARRAY && sl_fol->basis)
                sl_fol = sl_fol->basis;
            else if (
                sl_fol->genus == TY_STRUCT && sl_fol->membra
                && sl_fol->num_membrorum > 0
            )
                sl_fol = sl_fol->membra[0].typus;
            else
                break;
        }
        int sl_mag = typus_magnitudo(sl_fol);
        if (sl_mag < 1)
            erratum("magnitudo folii staticae localis invalida");

        for (int j = 0; j < n->num_membrorum; j++) {
            nodus_t      *elem = n->membra[j];
            int elem_off;
            int store_mag;
            if (elem->init_offset >= 0) {
                elem_off  = data_off + elem->init_offset;
                store_mag = elem->init_size > 0 ? elem->init_size : sl_mag;
            } else {
                elem_off  = data_off + j * sl_mag;
                store_mag = sl_mag;
            }
            if (elem_off + store_mag > data_off + mag)
                break;
            if (elem->genus == N_STR) {
                int sid = gsym_chorda_adde(elem->chorda, elem->lon_chordae);
                gsym_data_reloc_adde(elem_off, DR_CSTRING, sid);
            } else if (elem->genus == N_NUM) {
                long v = elem->valor;
                memcpy(gsym_init_data + elem_off, &v, store_mag);
            } else if (
                elem->genus == N_UNOP && elem->op == T_MINUS
                && elem->sinister && elem->sinister->genus == N_NUM
            ) {
                long v = -elem->sinister->valor;
                memcpy(gsym_init_data + elem_off, &v, store_mag);
            } else if (elem->genus == N_IDENT && elem->nomen) {
                int flabel = gsym_func_loc_quaere(elem->nomen);
                if (flabel >= 0)
                    gsym_data_reloc_adde(elem_off, DR_TEXT, flabel);
            }
        }
    }
}
