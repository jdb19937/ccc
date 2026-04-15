/*
 * biblio.c — tractatio vexillorum -I, -L, -l
 *
 * Resolvit vias inclusionis, quaerit bibliothecas,
 * legit archiva .a et extrahit objecta.
 */

#include "ccc.h"
#include "biblio.h"
#include "scribo.h"

#include <errno.h>

/* ================================================================
 * viae inclusionis (-I)
 * ================================================================ */

char **viae_includ       = NULL;
int    num_viarum_includ = 0;
static int cap_includ    = 0;

void includ_adde(const char *via)
{
    if (num_viarum_includ >= cap_includ) {
        cap_includ  = cap_includ ? cap_includ * 2 : 8;
        viae_includ = realloc(viae_includ, cap_includ * sizeof(char *));
        if (!viae_includ)
            erratum("memoria exhausta");
    }
    viae_includ[num_viarum_includ++] = strdup(via);
}

char *includ_quaere(
    const char *nomen, int *longitudo,
    char *via_inventa, int via_mag
) {
    for (int i = 0; i < num_viarum_includ; i++) {
        char via_plena[1024];
        int n = snprintf(
            via_plena, sizeof(via_plena), "%s/%s",
            viae_includ[i], nomen
        );
        if (n < 0 || n >= (int)sizeof(via_plena))
            continue;
        FILE *fp = fopen(via_plena, "rb");
        if (!fp)
            continue;
        fseek(fp, 0, SEEK_END);
        long mag = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        char *data = malloc(mag + 1);
        if (!data)
            erratum("memoria exhausta");
        fread(data, 1, mag, fp);
        data[mag] = '\0';
        fclose(fp);
        if (longitudo)
            *longitudo = (int)mag;
        if (via_inventa)
            snprintf(via_inventa, via_mag, "%s", via_plena);
        return data;
    }
    return NULL;
}

/* ================================================================
 * viae bibliothecarum (-L)
 * ================================================================ */

char **viae_biblio       = NULL;
int    num_viarum_biblio = 0;
static int cap_biblio    = 0;

void biblio_via_adde(const char *via)
{
    if (num_viarum_biblio >= cap_biblio) {
        cap_biblio  = cap_biblio ? cap_biblio * 2 : 8;
        viae_biblio = realloc(viae_biblio, cap_biblio * sizeof(char *));
        if (!viae_biblio)
            erratum("memoria exhausta");
    }
    viae_biblio[num_viarum_biblio++] = strdup(via);
}

/* ================================================================
 * resolutio bibliothecarum (-l)
 * ================================================================ */

biblio_res_t *biblio_res     = NULL;
int           num_biblio_res = 0;
static int    cap_res        = 0;

static int plica_exstat(const char *via)
{
    FILE *fp = fopen(via, "rb");
    if (fp) {
        fclose(fp);
        return 1;
    }
    return 0;
}

void res_adde(const char *via, int genus)
{
    if (num_biblio_res >= cap_res) {
        cap_res    = cap_res ? cap_res * 2 : 8;
        biblio_res = realloc(biblio_res, cap_res * sizeof(biblio_res_t));
        if (!biblio_res)
            erratum("memoria exhausta");
    }
    biblio_res[num_biblio_res].via   = strdup(via);
    biblio_res[num_biblio_res].genus = genus;
    num_biblio_res++;
}

void biblio_adde(const char *nomen)
{
    char via[1024];

    /* proba in viis -L */
    for (int i = 0; i < num_viarum_biblio; i++) {
        snprintf(via, sizeof(via), "%s/lib%s.a", viae_biblio[i], nomen);
        if (plica_exstat(via)) {
            res_adde(via, BIBLIO_A);
            return;
        }
        snprintf(via, sizeof(via), "%s/lib%s.dylib", viae_biblio[i], nomen);
        if (plica_exstat(via)) {
            res_adde(via, BIBLIO_DYLIB);
            return;
        }
    }

    /* proba vias systematis */
    snprintf(via, sizeof(via), "/usr/lib/lib%s.dylib", nomen);
    if (plica_exstat(via)) {
        res_adde(via, BIBLIO_DYLIB);
        return;
    }
    snprintf(via, sizeof(via), "/usr/local/lib/lib%s.a", nomen);
    if (plica_exstat(via)) {
        res_adde(via, BIBLIO_A);
        return;
    }
    snprintf(via, sizeof(via), "/usr/local/lib/lib%s.dylib", nomen);
    if (plica_exstat(via)) {
        res_adde(via, BIBLIO_DYLIB);
        return;
    }

    erratum("bibliotheca non inventa: -l%s", nomen);
}

void biblio_framework_adde(const char *nomen)
{
    char via[1024];

    /* proba /System/Library/Frameworks/Name.framework/Name */
    snprintf(via, sizeof(via),
             "/System/Library/Frameworks/%s.framework/%s", nomen, nomen);
    res_adde(via, BIBLIO_DYLIB);
}

int biblio_num_dylib(void)
{
    int n = 0;
    for (int i = 0; i < num_biblio_res; i++)
        if (biblio_res[i].genus == BIBLIO_DYLIB)
            n++;
    return n;
}

const char *biblio_dylib_via(int index)
{
    int n = 0;
    for (int i = 0; i < num_biblio_res; i++) {
        if (biblio_res[i].genus == BIBLIO_DYLIB) {
            if (n == index)
                return biblio_res[i].via;
            n++;
        }
    }
    return NULL;
}

/* ================================================================
 * extractio archivorum .a
 *
 * Formatum ar:
 *   8 octeti: "!<arch>\n"
 *   Iterantur membra:
 *     60 octeti caput:
 *       16: nomen (vel "#1/N" pro nomine extenso BSD)
 *       12: mtime, 6: uid, 6: gid, 8: modus
 *       10: magnitudo
 *        2: "`\n"
 *     data (impletio ad 2 octetos)
 * ================================================================ */

static int ar_numerus(const uint8_t *p, int lon)
{
    int val = 0;
    for (int i = 0; i < lon; i++) {
        if (p[i] >= '0' && p[i] <= '9')
            val = val * 10 + (p[i] - '0');
        else if (p[i] != ' ')
            break;
    }
    return val;
}

char **biblio_extrahe_objecta(int *numerum)
{
    char **viae = NULL;
    int    num  = 0;
    int    cap  = 0;

    for (int bi = 0; bi < num_biblio_res; bi++) {
        if (biblio_res[bi].genus != BIBLIO_A)
            continue;

        int ar_lon;
        uint8_t *ar = (uint8_t *)lege_plicam(biblio_res[bi].via, &ar_lon);

        if (ar_lon < 8 || memcmp(ar, "!<arch>\n", 8) != 0)
            erratum("'%s' non est archivum ar", biblio_res[bi].via);

        int pos = 8;
        int mi  = 0; /* index membri */
        while (pos + 60 <= ar_lon) {
            const uint8_t *caput = ar + pos;
            if (caput[58] != '`' || caput[59] != '\n')
                break;

            int mag      = ar_numerus(caput + 48, 10);
            int data_pos = pos + 60;

            /* lege nomen */
            char nomen[256];
            memset(nomen, 0, sizeof(nomen));
            int nomen_in_data = 0;

            if (caput[0] == '#' && caput[1] == '1' && caput[2] == '/') {
                /* nomen extensum BSD */
                nomen_in_data = ar_numerus(caput + 3, 13);
                int nlon      = nomen_in_data;
                if (nlon > 255)
                    nlon = 255;
                memcpy(nomen, ar + data_pos, nlon);
                /* remove NUL impletionem */
                while (nlon > 0 && nomen[nlon - 1] == '\0')
                    nlon--;
                nomen[nlon] = '\0';
            } else {
                int i;
                for (i = 0; i < 16 && caput[i] != ' ' && caput[i] != '/'; i++)
                    nomen[i] = caput[i];
                nomen[i] = '\0';
            }

            /* praetermitte tabulas symbolorum (__.SYMDEF etc.) */
            int nlon = (int)strlen(nomen);
            int est_obj = (
                nlon > 2 &&
                nomen[nlon - 2] == '.' && nomen[nlon - 1] == 'o'
            );

            int obj_pos = data_pos + nomen_in_data;
            int obj_mag = mag - nomen_in_data;

            if (est_obj && obj_pos + obj_mag <= ar_lon && obj_mag >= 4) {
                uint32_t magicum;
                memcpy(&magicum, ar + obj_pos, 4);

                if (magicum == MH_MAGIC_64) {
                    /* scribe in plicam temporariam */
                    char *via_tmp = malloc(256);
                    if (!via_tmp)
                        erratum("memoria exhausta");
                    snprintf(via_tmp, 256, "/tmp/ccc_ar_%d_%d.o", bi, mi);

                    FILE *fp = fopen(via_tmp, "wb");
                    if (!fp)
                        erratum("non possum scribere '%s'", via_tmp);
                    fwrite(ar + obj_pos, 1, obj_mag, fp);
                    fclose(fp);

                    /* adde ad tabulam */
                    if (num >= cap) {
                        cap  = cap ? cap * 2 : 8;
                        viae = realloc(viae, cap * sizeof(char *));
                        if (!viae)
                            erratum("memoria exhausta");
                    }
                    viae[num++] = via_tmp;
                }
            }

            pos = data_pos + mag;
            if (pos & 1)
                pos++;
            mi++;
        }

        free(ar);
    }

    *numerum = num;
    return viae;
}

void biblio_purga_temporarias(char **extractae, int numerus)
{
    for (int i = 0; i < numerus; i++) {
        remove(extractae[i]);
        free(extractae[i]);
    }
    free(extractae);
}
