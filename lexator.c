/*
 * lexator.c — lexator C99 purus
 *
 * Operatur in inputo iam praeprocessato ab ic. Directivae praeprocessoris
 * (#include, #define, #if, etc.) non tractantur hic — solum directiva
 * lineae "# LINEA \"PLICA\" [vexillum]" agnoscitur ut positio in
 * diagnosticis recte monstretur.
 */

#include "utilia.h"
#include "lexator.h"

/* ================================================================
 * vocabula clavis
 * ================================================================ */

static struct {
    const char *nomen;
    int genus;
} vocabula[] = {
    {"auto",      T_AUTO},     {"break",    T_BREAK},
    {"case",      T_CASE},     {"char",     T_CHAR},
    {"const",     T_CONST},    {"continue", T_CONTINUE},
    {"default",   T_DEFAULT},  {"do",       T_DO},
    {"double",    T_DOUBLE},   {"else",     T_ELSE},
    {"enum",      T_ENUM},     {"extern",   T_EXTERN},
    {"float",     T_FLOAT},    {"for",      T_FOR},
    {"goto",      T_GOTO},     {"if",       T_IF},
    {"int",       T_INT},      {"long",     T_LONG},
    {"register",  T_REGISTER}, {"return",   T_RETURN},
    {"short",     T_SHORT},    {"signed",   T_SIGNED},
    {"sizeof",    T_SIZEOF},   {"static",   T_STATIC},
    {"struct",    T_STRUCT},   {"switch",   T_SWITCH},
    {"typedef",   T_TYPEDEF},  {"union",    T_UNION},
    {"unsigned",  T_UNSIGNED}, {"void",     T_VOID},
    {"volatile",  T_VOLATILE}, {"while",    T_WHILE},
    /* §6.7.4: inline — indicium optimizationis */
    {"inline",    T_INLINE},
    /* §6.2.5: _Bool — typus Booleanus */
    {"_Bool",     T_BOOL},
    /* §6.7.3.1: restrict — qualificator indicis */
    {"restrict",  T_RESTRICT},
    {NULL, 0}
};

/* ================================================================
 * typedef nomina (pro disambiguatione in parsore)
 * ================================================================ */

static char typedef_nomina[4096][256];
static int  num_typedef_nominum = 0;

int lex_est_typus(const char *nomen)
{
    for (int i = 0; i < num_typedef_nominum; i++)
        if (strcmp(typedef_nomina[i], nomen) == 0)
            return 1;
    return 0;
}

void lex_registra_typedef(const char *nomen)
{
    if (num_typedef_nominum >= 4096)
        erratum("nimis multa typedef nomina");
    strncpy(typedef_nomina[num_typedef_nominum++], nomen, 255);
}

/* ================================================================
 * contextus fontis
 * ================================================================ */

static char       *cur_nomen;
static const char *cur_fons;
static int         cur_longitudo;
static int         cur_positus;
static int         cur_linea;

/* ================================================================
 * signum currens (exportatum)
 * ================================================================ */

signum_t sig;
int sig_linea;

/* ================================================================
 * characteres
 * ================================================================ */

static int lege_c(void)
{
    if (cur_positus < cur_longitudo) {
        char c = cur_fons[cur_positus++];
        if (c == '\n')
            cur_linea++;
        return (unsigned char)c;
    }
    return -1;
}

static void repone_c(void)
{
    if (cur_positus > 0) {
        cur_positus--;
        if (cur_positus < cur_longitudo && cur_fons[cur_positus] == '\n')
            cur_linea--;
    }
}

/* ================================================================
 * praetermissio spatiorum et commentariorum
 * ================================================================ */

static void praetermitte_spatia(void)
{
    for (;;) {
        int c = lege_c();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v')
            continue;
        if (c == '/') {
            int c2 = lege_c();
            if (c2 == '/') {
                while ((c = lege_c()) != -1 && c != '\n') {}
                continue;
            } else if (c2 == '*') {
                int prec = 0;
                while ((c = lege_c()) != -1) {
                    if (prec == '*' && c == '/')
                        break;
                    prec = c;
                }
                continue;
            }
            repone_c();
        }
        if (c != -1)
            repone_c();
        return;
    }
}

/* ================================================================
 * directiva lineae — "# LINEA \"PLICA\" [vexillum]"
 *
 * Emissa ab ic. Actualizat cur_linea et cur_nomen.
 * ================================================================ */

static void tracta_directivam_lineae(void)
{
    /* iam post '#' — praetermitte spatia */
    int c;
    while ((c = lege_c()) == ' ' || c == '\t') {}
    /* lege numerum lineae */
    if (c < '0' || c > '9')
        erratum_ad(
            cur_linea,
            "directiva '#' malformata in plica praeprocessata "
            "(expectabatur '# N \"plica\"')"
        );
    int linea_nova = 0;
    while (c >= '0' && c <= '9') {
        linea_nova = linea_nova * 10 + (c - '0');
        c = lege_c();
    }
    /* praetermitte spatia ante nomen plicae */
    while (c == ' ' || c == '\t')
        c = lege_c();
    /* nomen plicae in virgulis — obligatorium */
    if (c != '"')
        erratum_ad(
            cur_linea,
            "directiva '#' malformata: expectabatur '\"plica\"' post numerum"
        );
    char nomen_novum[1024];
    int i = 0;
    while ((c = lege_c()) != -1 && c != '"' && c != '\n') {
        if (c == '\\') {
            int e = lege_c();
            if (e == -1)
                break;
            c = e;
        }
        if (i >= (int)sizeof(nomen_novum) - 1)
            erratum_ad(cur_linea, "nomen plicae in directiva nimis longum");
        nomen_novum[i++] = c;
    }
    if (c != '"')
        erratum_ad(cur_linea, "nomen plicae in directiva non terminatum");
    nomen_novum[i] = '\0';
    free(cur_nomen);
    cur_nomen = strdup(nomen_novum);
    /* praetermitte residuum lineae (vexilla optionalia) */
    while (c != -1 && c != '\n')
        c = lege_c();
    /* linea nova inita — proxima linea ad legendum habebit numerum 'linea_nova' */
    cur_linea = linea_nova;
}

/* ================================================================
 * character effugiens in chordis
 * ================================================================ */

static int lege_effugium(void)
{
    int c = lege_c();
    switch (c) {
    case 'n':  return '\n';
    case 't':  return '\t';
    case 'r':  return '\r';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"':  return '"';
    case 'a':  return '\a';
    case 'b':  return '\b';
    case 'f':  return '\f';
    case 'v':  return '\v';
    case 'x': {
            int val = 0;
            for (int i = 0; i < 2; i++) {
                c = lege_c();
                if (c >= '0' && c <= '9')
                    val = val * 16 + (c - '0');
                else if (c >= 'a' && c <= 'f')
                    val = val * 16 + (c - 'a' + 10);
                else if (c >= 'A' && c <= 'F')
                    val = val * 16 + (c - 'A' + 10);
                else {
                    repone_c();
                    break;
                }
            }
            return val;
        }
    default:
        if (c >= '0' && c <= '7') {
            int val = c - '0';
            for (int i = 0; i < 2; i++) {
                c = lege_c();
                if (c >= '0' && c <= '7')
                    val = val * 8 + (c - '0');
                else {
                    repone_c();
                    break;
                }
            }
            return val;
        }
        return c;
    }
}

/* ================================================================
 * signum spectans (peek)
 * ================================================================ */

static signum_t sig_spectans;
static int      habet_spectantem = 0;
static const char *plica_spectans = NULL;

/* ================================================================
 * lexator principalis
 * ================================================================ */

static int lege_signum_internum(void)
{
inicio:
    praetermitte_spatia();
    sig.linea = cur_linea;
    sig_linea = cur_linea;
    plica_currentis = cur_nomen;

    int c = lege_c();
    if (c == -1) {
        sig.genus = T_EOF;
        return T_EOF;
    }

    /* nova linea — continua */
    if (c == '\n')
        goto inicio;

    /* §6.10.4: directiva lineae emissa ab ic */
    if (c == '#') {
        tracta_directivam_lineae();
        goto inicio;
    }

    /* numerus — §6.4.4.1 (integer), §6.4.4.2 (fluitans) */
    if (c >= '0' && c <= '9') {
        char num_buf[256];
        int num_len      = 0;
        int est_fluitans = 0;

        long val        = 0;
        num_buf[num_len ++] = (char)c;
        if (c == '0') {
            c = lege_c();
            if (c == 'x' || c == 'X') {
                num_buf[num_len++] = (char)c;
                while ((c = lege_c()) != -1) {
                    if (c >= '0' && c <= '9')
                        val = val * 16 + (c - '0');
                    else if (c >= 'a' && c <= 'f')
                        val = val * 16 + (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F')
                        val = val * 16 + (c - 'A' + 10);
                    else
                        break;
                    num_buf[num_len++] = (char)c;
                }
            } else if (c >= '0' && c <= '7' && c != '.') {
                val = c - '0';
                num_buf[num_len++] = (char)c;
                while ((c = lege_c()) != -1 && c >= '0' && c <= '7') {
                    val = val * 8 + (c - '0');
                    num_buf[num_len++] = (char)c;
                }
            }
        } else {
            val = c - '0';
            while ((c = lege_c()) != -1 && c >= '0' && c <= '9') {
                val = val * 10 + (c - '0');
                num_buf[num_len++] = (char)c;
            }
        }
        /* §6.4.4.2: punctum decimale → constans fluitans */
        if (c == '.') {
            est_fluitans    = 1;
            num_buf[num_len ++] = '.';
            while ((c = lege_c()) != -1 && c >= '0' && c <= '9')
                num_buf[num_len++] = (char)c;
        }
        /* §6.4.4.2: exponent e/E → constans fluitans */
        if (c == 'e' || c == 'E') {
            est_fluitans = 1;
            num_buf[num_len++] = (char)c;
            c = lege_c();
            if (c == '+' || c == '-') {
                num_buf[num_len++] = (char)c;
                c = lege_c();
            }
            while (c >= '0' && c <= '9') {
                num_buf[num_len++] = (char)c;
                c = lege_c();
            }
        }
        /* suffixum — §6.4.4.1, §6.4.4.2 */
        if (est_fluitans) {
            if (c == 'f' || c == 'F' || c == 'l' || c == 'L')
                c = lege_c();
        } else {
            sig .num_sfx_u = 0;
            sig .num_sfx_l = 0;
            while (
                c == 'u' || c == 'U' || c == 'l' || c == 'L' ||
                c == 'f' || c == 'F'
            ) {
                if (c == 'f' || c == 'F')
                    est_fluitans = 1;
                else if (c == 'u' || c == 'U')
                    sig.num_sfx_u = 1;
                else
                    sig.num_sfx_l++;
                c = lege_c();
            }
        }
        if (c != -1)
            repone_c();

        if (est_fluitans) {
            num_buf[num_len] = '\0';
            sig.genus        = T_NUM_FLUAT;
            sig.valor_f      = strtod(num_buf, NULL);
            sig.valor        = 0;
            return T_NUM_FLUAT;
        }
        sig .genus = T_NUM;
        sig .valor = val;
        return T_NUM;
    }

    /* chorda — §6.4.5 */
    if (c == '"') {
        int i = 0;
        while ((c = lege_c()) != -1 && c != '"') {
            if (c == '\\')
                c = lege_effugium();
            if (i >= MAX_CHORDA - 1)
                erratum_ad(
                    cur_linea,
                    "chorda litteralis nimis longa (max %d)", MAX_CHORDA - 1
                );
            sig.chorda[i++] = c;
        }
        sig .chorda[i]   = '\0';
        sig .lon_chordae = i;
        sig .genus       = T_STR;

        /* §6.4.5p4: concatenatio chordarum litteralium adiacentium */
        for (;;) {
            praetermitte_spatia();
            c = lege_c();
            while (c == '\n') {
                praetermitte_spatia();
                c = lege_c();
            }
            if (c == '"') {
                while ((c = lege_c()) != -1 && c != '"') {
                    if (c == '\\')
                        c = lege_effugium();
                    if (i >= MAX_CHORDA - 1)
                        erratum_ad(
                            cur_linea,
                            "chorda litteralis nimis longa (max %d)",
                            MAX_CHORDA - 1
                        );
                    sig.chorda[i++] = c;
                }
                sig .chorda[i]   = '\0';
                sig .lon_chordae = i;
            } else {
                if (c != -1)
                    repone_c();
                break;
            }
        }
        return T_STR;
    }

    /* character — §6.4.4.4 */
    if (c == '\'') {
        c = lege_c();
        if (c == '\\')
            c = lege_effugium();
        sig .valor = c;
        c   = lege_c();
        if (c != '\'')
            erratum_ad(
                cur_linea,
                "character litteralis non clausum: expectabatur '"
            );
        sig.genus = T_CHARLIT;
        return T_CHARLIT;
    }

    /* identificator vel vocabulum clavis
     * (octeti >= 0x80 admittuntur ut sequentiae UTF-8) */
    if (
        (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
        || (c & 0x80)
    ) {
        int i = 0;
        sig   .chorda[i++] = c;
        while ((c = lege_c()) != -1) {
            if (
                (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || (c & 0x80)
            ) {
                if (i >= MAX_CHORDA - 1)
                    erratum_ad(cur_linea, "identificator nimis longus");
                sig.chorda[i++] = c;
            } else {
                repone_c();
                break;
            }
        }
        sig.chorda[i] = '\0';

        if (!utf8_valida(sig.chorda, i))
            erratum_ad(
                cur_linea, "identificator cum UTF-8 invalida: '%s'", sig.chorda
            );

        /* est vocabulum clavis? */
        for (int j = 0; vocabula[j].nomen; j++) {
            if (strcmp(sig.chorda, vocabula[j].nomen) == 0) {
                sig.genus = vocabula[j].genus;
                return sig.genus;
            }
        }

        sig.genus = T_IDENT;
        return T_IDENT;
    }

    /* operatores */
    switch (c) {
    case '+': {
            int c2 = lege_c();
            if (c2 == '+') {
                sig.genus = T_PLUSPLUS;
                return T_PLUSPLUS;
            }
            if (c2 == '=') {
                sig.genus = T_PLUSEQ;
                return T_PLUSEQ;
            }
            repone_c();
            sig.genus = T_PLUS;
            return T_PLUS;
        }
    case '-': {
            int c2 = lege_c();
            if (c2 == '-') {
                sig.genus = T_MINUSMINUS;
                return T_MINUSMINUS;
            }
            if (c2 == '=') {
                sig.genus = T_MINUSEQ;
                return T_MINUSEQ;
            }
            if (c2 == '>') {
                sig.genus = T_ARROW;
                return T_ARROW;
            }
            repone_c();
            sig.genus = T_MINUS;
            return T_MINUS;
        }
    case '*': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_STAREQ;
                return T_STAREQ;
            }
            repone_c();
            sig.genus = T_STAR;
            return T_STAR;
        }
    case '/': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_SLASHEQ;
                return T_SLASHEQ;
            }
            repone_c();
            sig.genus = T_SLASH;
            return T_SLASH;
        }
    case '%': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_PERCENTEQ;
                return T_PERCENTEQ;
            }
            repone_c();
            sig.genus = T_PERCENT;
            return T_PERCENT;
        }
    case '&': {
            int c2 = lege_c();
            if (c2 == '&') {
                sig.genus = T_AMPAMP;
                return T_AMPAMP;
            }
            if (c2 == '=') {
                sig.genus = T_AMPEQ;
                return T_AMPEQ;
            }
            repone_c();
            sig.genus = T_AMP;
            return T_AMP;
        }
    case '|': {
            int c2 = lege_c();
            if (c2 == '|') {
                sig.genus = T_PIPEPIPE;
                return T_PIPEPIPE;
            }
            if (c2 == '=') {
                sig.genus = T_PIPEEQ;
                return T_PIPEEQ;
            }
            repone_c();
            sig.genus = T_PIPE;
            return T_PIPE;
        }
    case '^': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_CARETEQ;
                return T_CARETEQ;
            }
            repone_c();
            sig.genus = T_CARET;
            return T_CARET;
        }
    case '<': {
            int c2 = lege_c();
            if (c2 == '<') {
                int c3 = lege_c();
                if (c3 == '=') {
                    sig.genus = T_LTLTEQ;
                    return T_LTLTEQ;
                }
                repone_c();
                sig.genus = T_LTLT;
                return T_LTLT;
            }
            if (c2 == '=') {
                sig.genus = T_LTEQ;
                return T_LTEQ;
            }
            repone_c();
            sig.genus = T_LT;
            return T_LT;
        }
    case '>': {
            int c2 = lege_c();
            if (c2 == '>') {
                int c3 = lege_c();
                if (c3 == '=') {
                    sig.genus = T_GTGTEQ;
                    return T_GTGTEQ;
                }
                repone_c();
                sig.genus = T_GTGT;
                return T_GTGT;
            }
            if (c2 == '=') {
                sig.genus = T_GTEQ;
                return T_GTEQ;
            }
            repone_c();
            sig.genus = T_GT;
            return T_GT;
        }
    case '=': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_EQEQ;
                return T_EQEQ;
            }
            repone_c();
            sig.genus = T_ASSIGN;
            return T_ASSIGN;
        }
    case '!': {
            int c2 = lege_c();
            if (c2 == '=') {
                sig.genus = T_BANGEQ;
                return T_BANGEQ;
            }
            repone_c();
            sig.genus = T_BANG;
            return T_BANG;
        }
    case '~': sig.genus = T_TILDE;     return T_TILDE;
    case '?': sig.genus = T_QUESTION;  return T_QUESTION;
    case ':': sig.genus = T_COLON;     return T_COLON;
    case ';': sig.genus = T_SEMICOLON; return T_SEMICOLON;
    case ',': sig.genus = T_COMMA;     return T_COMMA;
    case '(': sig.genus = T_LPAREN;    return T_LPAREN;
    case ')': sig.genus = T_RPAREN;    return T_RPAREN;
    case '[': sig.genus = T_LBRACKET;  return T_LBRACKET;
    case ']': sig.genus = T_RBRACKET;  return T_RBRACKET;
    case '{': sig.genus = T_LBRACE;    return T_LBRACE;
    case '}': sig.genus = T_RBRACE;    return T_RBRACE;
    case '.': {
            int c2 = lege_c();
            if (c2 == '.') {
                int c3 = lege_c();
                if (c3 == '.') {
                    sig.genus = T_ELLIPSIS;
                    return T_ELLIPSIS;
                }
                repone_c();
            }
            repone_c();
            sig.genus = T_DOT;
            return T_DOT;
        }
    }

    erratum_ad(cur_linea, "character ignotus: '%c' (%d)", c, c);
    return T_EOF;
}

/* ================================================================
 * interfacies publica
 * ================================================================ */

void lex_initia(const char *nomen, const char *fons, int longitudo)
{
    free(cur_nomen);
    cur_nomen         = strdup(nomen);
    cur_fons          = fons;
    cur_longitudo     = longitudo;
    cur_positus       = 0;
    cur_linea         = 1;
    plica_currentis   = cur_nomen;
    habet_spectantem  = 0;
    num_typedef_nominum = 0;
}

void lex_proximum(void)
{
    if (habet_spectantem) {
        memcpy(&sig, &sig_spectans, sizeof(signum_t));
        sig_linea        = sig.linea;
        plica_currentis  = plica_spectans;
        habet_spectantem = 0;
        return;
    }
    lege_signum_internum();
}

int lex_specta(void)
{
    if (habet_spectantem)
        return sig_spectans.genus;
    signum_t salva;
    memcpy(&salva, &sig, sizeof(signum_t));
    int salva_linea = sig_linea;
    const char      *salva_plica = plica_currentis;
    lege_signum_internum();
    memcpy(&sig_spectans, &sig, sizeof(signum_t));
    plica_spectans = plica_currentis;
    memcpy(&sig, &salva, sizeof(signum_t));
    sig_linea        = salva_linea;
    plica_currentis  = salva_plica;
    habet_spectantem = 1;
    return sig_spectans.genus;
}
