/*
 * utilia.c — functiōnēs commūnēs
 */

#include "utilia.h"

#include <errno.h>

const char *nomen_programmi = "ccc";
const char *plica_currentis = NULL;
const char *plica_exitus_gl = NULL;
const char *plica_i_tmp_gl  = NULL;

static void cleanup_exitus(void)
{
    if (plica_exitus_gl)
        remove(plica_exitus_gl);
    if (plica_i_tmp_gl)
        remove(plica_i_tmp_gl);
}

void erratum(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "%s: erratum: ", nomen_programmi);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    cleanup_exitus();
    exit(1);
}

void erratum_ad(int linea, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    const char *loc = plica_currentis ? plica_currentis : nomen_programmi;
    fprintf(stderr, "%s:%d: erratum: ", loc, linea);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    cleanup_exitus();
    exit(1);
}

char *lege_plicam(const char *via, int *longitudo)
{
    FILE *fp = fopen(via, "rb");
    if (!fp)
        erratum("non possum aperire '%s': %s", via, strerror(errno));

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
    return data;
}

int utf8_valida(const char *s, int n)
{
    const unsigned char *p   = (const unsigned char *)s;
    const unsigned char *fin = p + n;
    while (p < fin) {
        unsigned c = *p++;
        if (c < 0x80)
            continue;
        int extra;
        unsigned minimum;
        if ((c & 0xE0) == 0xC0) {
            if (c < 0xC2)         /* sequentia 2-octeta sur-longa */
                return 0;
            extra   = 1;
            minimum = 0x80;
        } else if ((c & 0xF0) == 0xE0) {
            extra   = 2;
            minimum = 0x800;
        } else if ((c & 0xF8) == 0xF0) {
            if (c > 0xF4)
                return 0;
            extra   = 3;
            minimum = 0x10000;
        } else {
            return 0;
        }
        if (p + extra > fin)
            return 0;
        unsigned punctum = c & (0x7Fu >> extra);
        for (int i = 0; i < extra; i++) {
            unsigned cc = *p++;
            if ((cc & 0xC0) != 0x80)
                return 0;
            punctum = (punctum << 6) | (cc & 0x3F);
        }
        if (punctum < minimum)
            return 0;
        if (punctum >= 0xD800 && punctum <= 0xDFFF)
            return 0;
        if (punctum > 0x10FFFF)
            return 0;
    }
    return 1;
}

