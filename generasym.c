/*
 * generasym.c — generator symbolicus (emittit assembly textum cum imm)
 *
 * Forma per genera.c ductus, sed emittit textum cc -S compatibile
 * loco binarii. Delegat ad imm assemblatorem pro:
 *   - resolutione labelorum et sectionum
 *   - relocationibus (ADRP/ADD/LDR @PAGE/@PAGEOFF/@GOTPAGE/@GOTPAGEOFF)
 *   - ordinatione .data/.bss/.cstring
 *
 * Referentiae ISO C99: citantur ubi logica semantica tractatur.
 */

#include "utilia.h"
#include "parser.h"
#include "generasym.h"
#include "emittesym.h"
#include "emitte.h"    /* pro typis: globalis_t, chorda_lit_t, data_reloc_t; constantibus SP/FP/LR/XZR */
#include "fluat.h"
#include "gsymconst.h"
#include "generasym_intern.h"

#include <string.h>
#include <stdlib.h>

/* ================================================================
 * status localis — parallelus cum emitte.c, sed hic generasym.c
 * solus (nullus collīgātus cum emitte.o)
 * ================================================================ */

/* chordae litterales */
chorda_lit_t *gsym_chordae     = NULL;
int           gsym_num_chordarum = 0;

/* globales (data, bss, cstring labels) */
globalis_t   *gsym_globales    = NULL;
int           gsym_num_globalium = 0;

/* init_data — byti initializatorum ante emissionem textualem */
uint8_t      *gsym_init_data   = NULL;
int           gsym_init_data_lon = 0;

/* relocationes in init_data (adresses intra data) */
data_reloc_t *gsym_data_relocs = NULL;
int           gsym_num_data_relocs = 0;

/* labels — simplex alveus ad memorandum an definita sit */
static int *gsym_labels_def = NULL;     /* 0 = nondum, 1 = posita */
static int  gsym_num_labels = 0;

gsym_func_loc_t *gsym_func_loci  = NULL;
int              gsym_num_func_loc = 0;

/* ================================================================
 * alveorum helpers
 * ================================================================ */

int gsym_chorda_adde(const char *data, int lon)
{
    /* cc -S consolidat chordas identicas — imitamur per quaestionem */
    for (int i = 0; i < gsym_num_chordarum; i++) {
        if (
            gsym_chordae[i].longitudo == lon
            && memcmp(gsym_chordae[i].data, data, lon) == 0
        )
            return i;
    }
    if (gsym_num_chordarum >= MAX_CHORDAE_LIT)
        erratum("generasym: nimis multae chordae");
    int id = gsym_num_chordarum++;
    gsym_chordae[id].data = malloc(lon + 1);
    if (!gsym_chordae[id].data)
        erratum("generasym: memoria exhausta");
    memcpy(gsym_chordae[id].data, data, lon);
    gsym_chordae[id] .data[lon] = 0;
    gsym_chordae[id] .longitudo = lon;
    gsym_chordae[id] .offset = 0;  /* non adhibetur in modo symbolico */
    return id;
}

int gsym_globalis_adde(const char *nomen, typus_t *typus, int est_staticus, long valor)
{
    /* quaere ne duplicemus — crucial pro tentativas declarationes */
    for (int i = 0; i < gsym_num_globalium; i++)
        if (strcmp(gsym_globales[i].nomen, nomen) == 0)
            return i;
    if (gsym_num_globalium >= MAX_GLOBALES)
        erratum("generasym: nimis multae globales");
    int id = gsym_num_globalium++;
    strncpy(gsym_globales[id].nomen, nomen, 255);
    gsym_globales[id].nomen[255] = 0;
    gsym_globales[id].typus = typus;
    gsym_globales[id].magnitudo = typus ? typus_magnitudo(typus) : 0;
    int col = typus ? typus_colineatio(typus) : 1;
    if (col < 1)
        col = 1;
    gsym_globales[id] .colineatio = col;
    gsym_globales[id] .est_bss = 1;  /* defalta: in BSS nisi initiatur */
    gsym_globales[id] .bss_offset = 0;
    gsym_globales[id] .data_offset = 0;
    gsym_globales[id] .est_staticus = est_staticus;
    gsym_globales[id] .valor_initialis = valor;
    gsym_globales[id] .habet_valorem = 0;
    return id;
}

void gsym_data_reloc_adde(int idata_offset, int genus, int target)
{
    if (gsym_num_data_relocs >= MAX_DATA_RELOCS)
        erratum("generasym: nimis multae relocationes datorum");
    int id = gsym_num_data_relocs++;
    gsym_data_relocs[id].idata_offset = idata_offset;
    gsym_data_relocs[id].genus = genus;
    gsym_data_relocs[id].target = target;
}

/* est referentia ad externum (functio vel variabilis non definita)
 * — nomen cum _ praefix. */
gsym_extern_t         *gsym_externa = NULL;
int  gsym_num_externa = 0;

int gsym_extern_adde(const char *nomen)
{
    /* nomen cum praefixo _; conservamus sic */
    for (int i = 0; i < gsym_num_externa; i++)
        if (strcmp(gsym_externa[i], nomen) == 0)
            return i;
    if (gsym_num_externa >= GSYM_MAX_EXT)
        erratum("generasym: nimis multa externa");
    int id = gsym_num_externa++;
    strncpy(gsym_externa[id], nomen, 255);
    gsym_externa[id][255] = 0;
    return id;
}

int gsym_label_novus(void)
{
    if (gsym_num_labels >= MAX_LABELS)
        erratum("generasym: nimis multa labels");
    int id = gsym_num_labels++;
    gsym_labels_def[id] = 0;
    return id;
}

int gsym_func_loc_quaere(const char *nomen)
{
    for (int i = 0; i < gsym_num_func_loc; i++)
        if (strcmp(gsym_func_loci[i].nomen, nomen) == 0)
            return gsym_func_loci[i].label;
    return -1;
}

static int gsym_func_loc_adde(const char *nomen, int est_staticus)
{
    int lab = gsym_label_novus();
    if (gsym_num_func_loc >= MAX_GLOBALES)
        erratum("generasym: nimis multae functiones locales");
    int i = gsym_num_func_loc++;
    strncpy(gsym_func_loci[i].nomen, nomen, 255);
    gsym_func_loci[i] .nomen[255] = 0;
    gsym_func_loci[i] .label = lab;
    gsym_func_loci[i] .est_staticus = est_staticus;
    return lab;
}


int cur_frame_mag;
typus_t *cur_func_typus;
int profunditas_vocationis = 0;

int reg_vertex = 0;

int reg_alloca(void)
{
    int r = reg_vertex++;
    if (r >= 15)
        erratum("registra exhausta");
    return r;
}

void reg_libera(int r)
{
    (void)r;
    if (reg_vertex > 0)
        reg_vertex--;
}

int reg_arm(int slot)
{
    if (slot < 8)
        return slot;
    return slot + 1;
}


/* ================================================================
 * API publica
 * ================================================================ */

void generasym_initia(void)
{
    gsym_chordae     = calloc(MAX_CHORDAE_LIT, sizeof(chorda_lit_t));
    gsym_globales    = calloc(MAX_GLOBALES, sizeof(globalis_t));
    gsym_init_data   = calloc(MAX_DATA, 1);
    gsym_data_relocs = calloc(MAX_DATA_RELOCS, sizeof(data_reloc_t));
    gsym_labels_def  = calloc(MAX_LABELS, sizeof(int));
    gsym_func_loci   = calloc(MAX_GLOBALES, sizeof(gsym_func_loc_t));
    gsym_externa     = calloc(GSYM_MAX_EXT, sizeof(gsym_extern_t));
    if (
        !gsym_chordae || !gsym_globales || !gsym_init_data
        || !gsym_data_relocs || !gsym_labels_def || !gsym_func_loci
        || !gsym_externa
    )
        erratum("generasym: memoria exhausta");
    gsym_num_chordarum = 0;
    gsym_num_globalium = 0;
    gsym_init_data_lon = 0;
    gsym_num_data_relocs = 0;
    gsym_num_labels = 0;
    gsym_num_func_loc = 0;
    gsym_num_externa = 0;
    reg_vertex             = 0;
    profunditas_vocationis = 0;
    gsymsent_initia();
}


void generasym_translatio(nodus_t *radix, FILE *out)
{
    esym_initia(out);

    /* prima passu: colligā globalēs */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_VAR_DECL && !n->est_externus) {
            symbolum_t *s = n->sym ? n->sym : ambitus_quaere_omnes(n->nomen);
            long val   = 0;
            if (n->sinister && n->sinister->genus == N_NUM)
                val = n->sinister->valor;
            int gid = gsym_globalis_adde(n->nomen, n->typus_decl, n->est_staticus, val);
            if (s)
                s->globalis_index = gid;
        }
    }

    /* secunda passu: registrā functionēs */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF)
            gsym_func_loc_adde(n->nomen, n->est_staticus);
    }

    /* tertia passu: generā functionēs */
    for (int i = 0; i < radix->num_membrorum; i++) {
        nodus_t *n = radix->membra[i];
        if (n->genus == N_FUNC_DEF)
            genera_functio(n);
    }

    /* quarta passu: aedificā init_data pro globālibus et staticīs locālibus */
    gsymdata_aedifica_initiationes(radix);

    /* finaliter: emitte sectionēs datōrum ad fīnem plicae */
    emitte_sectiones_datorum();
    esym_finit();
}
