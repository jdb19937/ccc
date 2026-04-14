/*
 * lexator.c — CCC lexator et praeprocessor
 *
 * Lexator C99 cum praeprocessore simplici.
 * Capita interna pro functionibus systematis (stdio, stdlib,
 * termios, etc.) inclusa sunt.
 */

#include "ccc.h"

/* ================================================================
 * capita interna — omnia capita systematis in una chorda
 * ================================================================ */

static const char caput_internum[] =
    "#ifndef _CCC_INTERNA_H\n"
    "#define _CCC_INTERNA_H\n"
    "\n"
    "typedef unsigned long __ccc_size_t;\n"
    "typedef long __ccc_ssize_t;\n"
    "#define size_t __ccc_size_t\n"
    "#define ssize_t __ccc_ssize_t\n"
    "#define NULL ((void *)0)\n"
    "typedef int _Bool;\n"
    "typedef int pid_t;\n"
    "typedef int mode_t;\n"
    "typedef int off_t;\n"
    "typedef long time_t;\n"
    "typedef unsigned char uint8_t;\n"
    "typedef unsigned short uint16_t;\n"
    "typedef unsigned int uint32_t;\n"
    "typedef unsigned long uint64_t;\n"
    "typedef char int8_t;\n"
    "typedef short int16_t;\n"
    "typedef int int32_t;\n"
    "typedef long int64_t;\n"
    "#define EOF (-1)\n"
    "#define SEEK_SET 0\n"
    "#define SEEK_CUR 1\n"
    "#define SEEK_END 2\n"
    "\n"
    "/* stdio */\n"
    "typedef struct __sFILE FILE;\n"
    "FILE *fopen(const char *, const char *);\n"
    "int fclose(FILE *);\n"
    "unsigned long fwrite(const void *, unsigned long, unsigned long, FILE *);\n"
    "int snprintf(char *, unsigned long, const char *, ...);\n"
    "int sprintf(char *, const char *, ...);\n"
    "int printf(const char *, ...);\n"
    "int fprintf(FILE *, const char *, ...);\n"
    "void perror(const char *);\n"
    "long getline(char **, unsigned long *, FILE *);\n"
    "int fseek(FILE *, long, int);\n"
    "long ftell(FILE *);\n"
    "\n"
    "/* stdlib */\n"
    "void *malloc(unsigned long);\n"
    "void *realloc(void *, unsigned long);\n"
    "void *calloc(unsigned long, unsigned long);\n"
    "void free(void *);\n"
    "void exit(int);\n"
    "long strtol(const char *, char **, int);\n"
    "char *strdup(const char *);\n"
    "int atoi(const char *);\n"
    "void abort(void);\n"
    "int atexit(void (*)(void));\n"
    "\n"
    "/* string */\n"
    "void *memcpy(void *, const void *, unsigned long);\n"
    "void *memmove(void *, const void *, unsigned long);\n"
    "void *memset(void *, int, unsigned long);\n"
    "int memcmp(const void *, const void *, unsigned long);\n"
    "int strcmp(const char *, const char *);\n"
    "int strncmp(const char *, const char *, unsigned long);\n"
    "unsigned long strlen(const char *);\n"
    "char *strchr(const char *, int);\n"
    "char *strrchr(const char *, int);\n"
    "char *strstr(const char *, const char *);\n"
    "char *strncpy(char *, const char *, unsigned long);\n"
    "char *strcpy(char *, const char *);\n"
    "char *strcat(char *, const char *);\n"
    "\n"
    "/* ctype */\n"
    "int isspace(int);\n"
    "int isdigit(int);\n"
    "int isalpha(int);\n"
    "int isalnum(int);\n"
    "int toupper(int);\n"
    "int tolower(int);\n"
    "\n"
    "/* unistd */\n"
    "#define STDIN_FILENO 0\n"
    "#define STDOUT_FILENO 1\n"
    "#define STDERR_FILENO 2\n"
    "long read(int, void *, unsigned long);\n"
    "long write(int, const void *, unsigned long);\n"
    "int close(int);\n"
    "unsigned int sleep(unsigned int);\n"
    "void _exit(int);\n"
    "char *strerror(int);\n"
    "\n"
    "/* errno */\n"
    "int *__error(void);\n"
    "#define errno (*__error())\n"
    "#define EAGAIN 35\n"
    "#define EINTR 4\n"
    "\n"
    "/* termios */\n"
    "#define NCCS 24\n"
    "#define BRKINT  2\n"
    "#define ICRNL   256\n"
    "#define INPCK   16\n"
    "#define ISTRIP  32\n"
    "#define IXON    512\n"
    "#define OPOST   1\n"
    "#define CS8     768\n"
    "#define ECHO    8\n"
    "#define ICANON  256\n"
    "#define IEXTEN  1024\n"
    "#define ISIG    128\n"
    "#define VMIN    16\n"
    "#define VTIME   17\n"
    "#define TCSAFLUSH 2\n"
    "struct termios {\n"
    "    unsigned long c_iflag;\n"
    "    unsigned long c_oflag;\n"
    "    unsigned long c_cflag;\n"
    "    unsigned long c_lflag;\n"
    "    unsigned char c_cc[24];\n"
    "    unsigned long c_ispeed;\n"
    "    unsigned long c_ospeed;\n"
    "};\n"
    "int tcgetattr(int, struct termios *);\n"
    "int tcsetattr(int, int, const struct termios *);\n"
    "\n"
    "/* sys/ioctl */\n"
    "#define TIOCGWINSZ 1074295912\n"
    "struct winsize {\n"
    "    unsigned short ws_row;\n"
    "    unsigned short ws_col;\n"
    "    unsigned short ws_xpixel;\n"
    "    unsigned short ws_ypixel;\n"
    "};\n"
    "int ioctl(int, unsigned long, ...);\n"
    "\n"
    "/* stdarg — non plene supportatum, sed declarationes */\n"
;

/* capita POSIX — separata ne chorda nimis longa sit */
static const char caput_internum_posix[] =
    "/* signal */\n"
    "typedef int sig_atomic_t;\n"
    "typedef void (*__sighandler_t)(int);\n"
    "struct sigaction {\n"
    "    __sighandler_t sa_handler;\n"
    "    int sa_mask;\n"
    "    int sa_flags;\n"
    "};\n"
    "#define SIG_DFL ((__sighandler_t)0)\n"
    "#define SIG_IGN ((__sighandler_t)1)\n"
    "#define SIGINT 2\n"
    "#define SIGCHLD 20\n"
    "#define SA_RESTART 0x0002\n"
    "#define SA_NOCLDSTOP 0x0008\n"
    "int sigaction(int, const struct sigaction *, struct sigaction *);\n"
    "__sighandler_t signal(int, __sighandler_t);\n"
    "int kill(int, int);\n"
    "\n"
    "/* sys/wait */\n"
    "#define WNOHANG 1\n"
    "#define WUNTRACED 2\n"
    "#define WIFEXITED(s) (((s) & 0x7f) == 0)\n"
    "#define WEXITSTATUS(s) (((s) >> 8) & 0xff)\n"
    "#define WIFSIGNALED(s) (((s) & 0x7f) != 0)\n"
    "#define WTERMSIG(s) ((s) & 0x7f)\n"
    "int waitpid(int, int *, int);\n"
    "int fork(void);\n"
    "int execvp(const char *, char *const *);\n"
    "int pipe(int *);\n"
    "int dup2(int, int);\n"
    "int setpgid(int, int);\n"
    "int tcsetpgrp(int, int);\n"
    "int getpid(void);\n"
    "int getpgrp(void);\n"
    "\n"
    "/* fcntl */\n"
    "#define O_RDONLY 0\n"
    "#define O_WRONLY 1\n"
    "#define O_CREAT  512\n"
    "#define O_TRUNC  1024\n"
    "#define O_APPEND 8\n"
    "int open(const char *, int, ...);\n"
    "\n"
    "/* sys/stat */\n"
    "struct stat {\n"
    "    int st_dev;\n"
    "    unsigned long st_ino;\n"
    "    unsigned short st_mode;\n"
    "    unsigned short st_nlink;\n"
    "    unsigned int st_uid;\n"
    "    unsigned int st_gid;\n"
    "    int st_rdev;\n"
    "    long st_size;\n"
    "};\n"
    "int stat(const char *, struct stat *);\n"
    "#define S_ISDIR(m) (((m) & 0170000) == 0040000)\n"
    "\n"
    "/* dirent */\n"
    "struct dirent {\n"
    "    unsigned long d_ino;\n"
    "    char d_name[256];\n"
    "};\n"
    "typedef struct __DIR DIR;\n"
    "DIR *opendir(const char *);\n"
    "struct dirent *readdir(DIR *);\n"
    "int closedir(DIR *);\n"
    "\n"
    "/* pwd */\n"
    "struct passwd {\n"
    "    char *pw_name;\n"
    "    char *pw_dir;\n"
    "};\n"
    "struct passwd *getpwuid(unsigned int);\n"
    "unsigned int getuid(void);\n"
    "\n"
    "/* misc */\n"
    "int chdir(const char *);\n"
    "char *getcwd(char *, unsigned long);\n"
    "char *getenv(const char *);\n"
    "int setenv(const char *, const char *, int);\n"
    "int unsetenv(const char *);\n"
    "int access(const char *, int);\n"
    "#define X_OK 1\n"
    "#define R_OK 4\n"
    "int isatty(int);\n"
    "int putchar(int);\n"
    "int fputs(const char *, void *);\n"
    "int fflush(void *);\n"
    "int fileno(void *);\n"
    "extern void *__stdinp;\n"
    "extern void *__stdoutp;\n"
    "extern void *__stderrp;\n"
    "#define stdin __stdinp\n"
    "#define stdout __stdoutp\n"
    "#define stderr __stderrp\n"
    "int fgetc(void *);\n"
    "int feof(void *);\n"
    "char *fgets(char *, int, void *);\n"
    "unsigned long fread(void *, unsigned long, unsigned long, void *);\n"
    "\n"
    "#endif\n"
;

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
    {NULL, 0}
};

/* ================================================================
 * macrae (praeparatore)
 * ================================================================ */

typedef struct {
    char nomen[256];
    char valor[MAX_CHORDA];
    int activa;
    int est_functionalis;       /* 1 si macra functionalis */
    int num_parametrorum;
    char parametri[16][128];    /* nomina parametrorum */
} macra_t;

static macra_t macrae[MAX_MACRAE];
static int num_macrarum = 0;

static macra_t *macra_quaere(const char *nomen)
{
    for (int i = 0; i < num_macrarum; i++)
        if (macrae[i].activa && strcmp(macrae[i].nomen, nomen) == 0)
            return &macrae[i];
    return NULL;
}

static macra_t *macra_defini(const char *nomen, const char *valor)
{
    macra_t *m = macra_quaere(nomen);
    if (m) {
        strncpy(m->valor, valor, MAX_CHORDA - 1);
        return m;
    }
    if (num_macrarum >= MAX_MACRAE)
        erratum("nimis multae macrae");
    m = &macrae[num_macrarum++];
    strncpy(m->nomen, nomen, 255);
    strncpy(m->valor, valor, MAX_CHORDA - 1);
    m->activa = 1;
    m->est_functionalis = 0;
    m->num_parametrorum = 0;
    return m;
}

static void macra_dele(const char *nomen)
{
    macra_t *m = macra_quaere(nomen);
    if (m)
        m->activa = 0;
}

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
 * acervus plicarum (pro #include)
 * ================================================================ */

typedef struct {
    const char *nomen;
    const char *fons;
    int longitudo;
    int positus;
    int linea;
} plica_ctx_t;

static plica_ctx_t acervus_plicarum[MAX_PLICAE_ACERVUS];
static int vertex_plicarum = 0;

/* contextus currens */
static const char *cur_nomen;
static const char *cur_fons;
static int cur_longitudo;
static int cur_positus;
static int cur_linea;

/* ================================================================
 * acervus expansionis macrorum
 * ================================================================ */

typedef struct {
    const char *fons;
    int longitudo;
    int positus;
} expansio_ctx_t;

static expansio_ctx_t acervus_expansionum[64];
static int vertex_expansionum = 0;

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
    /* primum ex expansione macrorum */
    while (vertex_expansionum > 0) {
        expansio_ctx_t *e = &acervus_expansionum[vertex_expansionum - 1];
        if (e->positus < e->longitudo)
            return (unsigned char)e->fons[e->positus++];
        vertex_expansionum--;
    }

    for (;;) {
        if (cur_positus < cur_longitudo) {
            char c = cur_fons[cur_positus++];
            if (c == '\n')
                cur_linea++;
            /* continuatio lineae: \ ante novam lineam */
            if (c == '\\' && cur_positus < cur_longitudo && cur_fons[cur_positus] == '\n') {
                cur_positus++;
                cur_linea++;
                continue;
            }
            return (unsigned char)c;
        }
        /* fine plicae currentis — restitue ex acervo */
        if (vertex_plicarum <= 0)
            return -1;
        vertex_plicarum--;
        plica_ctx_t *p = &acervus_plicarum[vertex_plicarum];
        cur_nomen      = p->nomen;
        cur_fons       = p->fons;
        cur_longitudo  = p->longitudo;
        cur_positus    = p->positus;
        cur_linea      = p->linea;
    }
}

static void repone_c(void)
{
    /* repone unum characterem */
    if (vertex_expansionum > 0) {
        acervus_expansionum[vertex_expansionum - 1].positus--;
        return;
    }
    if (cur_positus > 0) {
        cur_positus--;
        if (cur_positus < cur_longitudo && cur_fons[cur_positus] == '\n')
            cur_linea--;
    }
}

static void pelle_expansionem(const char *fons, int lon)
{
    if (vertex_expansionum >= 64)
        erratum("expansio macrorum nimis profunda");
    expansio_ctx_t *e = &acervus_expansionum[vertex_expansionum++];
    e->fons = fons;
    e->longitudo = lon;
    e->positus = 0;
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
                /* commentarium lineae */
                while ((c = lege_c()) != -1 && c != '\n') {}
                continue;
            } else if (c2 == '*') {
                /* commentarium blocki */
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
 * conditiones praeprocessoris (#if/#ifdef acervus)
 * ================================================================ */

static int cond_acervus[64];
static int cond_vertex = 0;
/* 1 = emittimus, 0 = suppressi, 2 = in ramo else suppressi */

static int cond_activa(void)
{
    for (int i = 0; i < cond_vertex; i++)
        if (cond_acervus[i] != 1)
            return 0;
    return 1;
}

/* ================================================================
 * praeprocessor — directiva
 * ================================================================ */

static void lege_nomen_pp(char *alveus, int max)
{
    int i = 0;
    int c;
    praetermitte_spatia();
    while ((c = lege_c()) != -1) {
        if (
            (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_'
        ) {
            if (i < max - 1)
                alveus[i++] = c;
        } else {
            repone_c();
            break;
        }
    }
    alveus[i] = '\0';
}

static void lege_residuum_lineae(char *alveus, int max)
{
    int i = 0;
    int c;
    /* praetermitte spatia initialia */
    while ((c = lege_c()) != -1 && (c == ' ' || c == '\t')) {}
    if (c != -1 && c != '\n') {
        alveus[i++] = c;
        while ((c = lege_c()) != -1 && c != '\n') {
            if (i < max - 1)
                alveus[i++] = c;
        }
    }
    /* tolle spatia terminalia */
    while (i > 0 && (alveus[i-1] == ' ' || alveus[i-1] == '\t' || alveus[i-1] == '\r'))
        i--;
    alveus[i] = '\0';
}

static void praetermitte_lineam(void)
{
    int c;
    while ((c = lege_c()) != -1 && c != '\n') {}
}

static void pelle_plicam(const char *nomen, const char *fons, int lon)
{
    if (vertex_plicarum >= MAX_PLICAE_ACERVUS)
        erratum("nimis multae plicae inclusae");
    plica_ctx_t *p = &acervus_plicarum[vertex_plicarum++];
    p->nomen       = cur_nomen;
    p->fons        = cur_fons;
    p->longitudo   = cur_longitudo;
    p->positus     = cur_positus;
    p->linea       = cur_linea;

    cur_nomen     = nomen;
    cur_fons      = fons;
    cur_longitudo = lon;
    cur_positus   = 0;
    cur_linea     = 1;
}

static int tracta_directivam(void)
{
    /* iamiam post '#' */
    char directiva[64];
    lege_nomen_pp(directiva, sizeof(directiva));

    if (strcmp(directiva, "include") == 0) {
        if (!cond_activa()) {
            praetermitte_lineam();
            return 1;
        }
        praetermitte_spatia();
        int c = lege_c();
        char via[512];
        int i = 0;
        if (c == '"') {
            while ((c = lege_c()) != -1 && c != '"' && c != '\n')
                if (i < 511)
                    via[i++] = c;
            via[i] = '\0';
            praetermitte_lineam();

            /* proba plicam localem */
            char via_plena[1024];
            snprintf(
                via_plena, sizeof(via_plena), "%s%s",
                fons_directorium ? fons_directorium : "./", via
            );
            int lon;
            char *fons = lege_plicam(via_plena, &lon);
            pelle_plicam(via, fons, lon);
        } else if (c == '<') {
            while ((c = lege_c()) != -1 && c != '>' && c != '\n')
                if (i < 511)
                    via[i++] = c;
            via[i] = '\0';
            praetermitte_lineam();

            /* omnia capita systematis -> capita interna */
            pelle_plicam(via, caput_internum_posix, (int)strlen(caput_internum_posix));
            pelle_plicam(via, caput_internum, (int)strlen(caput_internum));
        }
        return 1;
    }

    if (strcmp(directiva, "define") == 0) {
        if (!cond_activa()) {
            praetermitte_lineam();
            return 1;
        }
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        /* proba si macra functionalis: '(' sine spatio post nomen */
        int c_peek = lege_c();
        if (c_peek == '(') {
            /* macra functionalis — lege parametros */
            char param_nomina[16][128];
            int nparam = 0;
            for (;;) {
                char pn[128];
                int pi = 0;
                /* praetermitte spatia */
                int pc;
                while ((pc = lege_c()) == ' ' || pc == '\t') {}
                if (pc == ')')
                    break;
                if (pc == ',' )
                    continue;
                /* lege nomen parametri */
                pn[pi++] = pc;
                while ((pc = lege_c()) != -1 && pc != ',' && pc != ')' && pc != ' ' && pc != '\t') {
                    if (pi < 127)
                        pn[pi++] = pc;
                }
                pn[pi] = '\0';
                if (nparam < 16)
                    strncpy(param_nomina[nparam++], pn, 127);
                if (pc == ')')
                    break;
            }
            char valor[MAX_CHORDA];
            lege_residuum_lineae(valor, sizeof(valor));
            macra_t *m = macra_defini(nomen, valor);
            m->est_functionalis = 1;
            m->num_parametrorum = nparam;
            for (int i = 0; i < nparam; i++)
                strncpy(m->parametri[i], param_nomina[i], 127);
        } else {
            repone_c();
            char valor[MAX_CHORDA];
            lege_residuum_lineae(valor, sizeof(valor));
            macra_defini(nomen, valor);
        }
        return 1;
    }

    if (strcmp(directiva, "undef") == 0) {
        if (!cond_activa()) {
            praetermitte_lineam();
            return 1;
        }
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        macra_dele(nomen);
        praetermitte_lineam();
        return 1;
    }

    if (strcmp(directiva, "ifdef") == 0) {
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        praetermitte_lineam();
        if (cond_vertex >= 64)
            erratum("#ifdef nimis profundum");
        if (!cond_activa()) {
            cond_acervus[cond_vertex++] = 0;
        } else {
            cond_acervus[cond_vertex++] = macra_quaere(nomen) ? 1 : 0;
        }
        return 1;
    }

    if (strcmp(directiva, "ifndef") == 0) {
        char nomen[256];
        lege_nomen_pp(nomen, sizeof(nomen));
        praetermitte_lineam();
        if (cond_vertex >= 64)
            erratum("#ifndef nimis profundum");
        if (!cond_activa()) {
            cond_acervus[cond_vertex++] = 0;
        } else {
            cond_acervus[cond_vertex++] = macra_quaere(nomen) ? 0 : 1;
        }
        return 1;
    }

    if (strcmp(directiva, "if") == 0) {
        if (cond_vertex >= 64)
            erratum("#if nimis profundum");
        if (!cond_activa()) {
            cond_acervus[cond_vertex++] = 0;
            praetermitte_lineam();
            return 1;
        }
        /* evaluatio simplex: supportamus '0', '1', 'defined(X)' */
        char expr[MAX_CHORDA];
        lege_residuum_lineae(expr, sizeof(expr));
        /* §6.10.1: macrae expanduntur ante evaluationem #if */
        {
            char expandata[MAX_CHORDA];
            int ei = 0;
            for (int ci = 0; expr[ci] && ei < MAX_CHORDA - 1; ) {
                if (
                    (expr[ci] >= 'a' && expr[ci] <= 'z') ||
                    (expr[ci] >= 'A' && expr[ci] <= 'Z') || expr[ci] == '_'
                ) {
                    char tok[256];
                    int ti = 0;
                    while (
                        expr[ci] && (
                            (expr[ci] >= 'a' && expr[ci] <= 'z') ||
                            (expr[ci] >= 'A' && expr[ci] <= 'Z') ||
                            (expr[ci] >= '0' && expr[ci] <= '9') || expr[ci] == '_'
                        )
                    )
                        if (ti < 255)
                            tok[ti++] = expr[ci++];
                    else
                        ci++;
                    tok[ti]         = '\0';
                    macra_t *m      = macra_quaere(tok);
                    const char *rep = (m && !m->est_functionalis) ? m->valor : tok;
                    while (*rep && ei < MAX_CHORDA - 1)
                        expandata[ei++] = *rep++;
                } else {
                    expandata[ei++] = expr[ci++];
                }
            }
            expandata[ei] = '\0';
            strncpy(expr, expandata, MAX_CHORDA - 1);
        }
        int val = 0;
        if (strstr(expr, "defined")) {
            /* extrahre nomen */
            char *p = strstr(expr, "defined");
            p += 7;
            while (*p == ' ' || *p == '(')
                p++;
            char nomen[256];
            int i = 0;
            while (*p && *p != ')' && *p != ' ' && i < 255)
                nomen[i++] = *p++;
            nomen[i] = '\0';
            val      = macra_quaere(nomen) ? 1 : 0;
        } else {
            /* evaluatio simplex expressionum: N > M, N == M, etc. */
            char *gt = strstr(expr, ">=");
            char *lt = strstr(expr, "<=");
            char *eq = strstr(expr, "==");
            char *ne = strstr(expr, "!=");
            char *g  = !gt ? strchr(expr, '>') : NULL;
            char *l  = !lt ? strchr(expr, '<') : NULL;
            if (eq)
                val = atoi(expr) == atoi(eq + 2);
            else if (ne)
                val = atoi(expr) != atoi(ne + 2);
            else if (gt)
                val = atoi(expr) >= atoi(gt + 2);
            else if (lt)
                val = atoi(expr) <= atoi(lt + 2);
            else if (g)
                val = atoi(expr) >  atoi(g + 1);
            else if (l)
                val = atoi(expr) <  atoi(l + 1);
            else
                val = atoi(expr);
        }
        cond_acervus[cond_vertex++] = val ? 1 : 0;
        return 1;
    }

    if (strcmp(directiva, "elif") == 0) {
        praetermitte_lineam();
        if (cond_vertex <= 0)
            erratum("#elif sine #if");
        if (cond_acervus[cond_vertex - 1] == 1) {
            cond_acervus[cond_vertex - 1] = 2; /* iam satisfactum */
        } else if (cond_acervus[cond_vertex - 1] == 0) {
            /* proba novam conditionem — simpliciter ponamus 0 */
            cond_acervus[cond_vertex - 1] = 0;
        }
        return 1;
    }

    if (strcmp(directiva, "else") == 0) {
        praetermitte_lineam();
        if (cond_vertex <= 0)
            erratum("#else sine #if");
        if (cond_acervus[cond_vertex - 1] == 1)
            cond_acervus[cond_vertex - 1] = 2;
        else if (cond_acervus[cond_vertex - 1] == 0)
            cond_acervus[cond_vertex - 1] = 1;
        return 1;
    }

    if (strcmp(directiva, "endif") == 0) {
        praetermitte_lineam();
        if (cond_vertex <= 0)
            erratum("#endif sine #if");
        cond_vertex--;
        return 1;
    }

    if (
        strcmp(directiva, "pragma") == 0 ||
        strcmp(directiva, "error") == 0 ||
        strcmp(directiva, "warning") == 0 ||
        strcmp(directiva, "line") == 0 ||
        directiva[0] == '\0'
    ) {
        praetermitte_lineam();
        return 1;
    }

    /* directiva ignota — praetermitte */
    praetermitte_lineam();
    return 1;
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
    case '0':  return '\0';
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
 * lexator principalis
 * ================================================================ */

static int lege_signum_internum(void)
{
inicio:
    praetermitte_spatia();
    sig.linea = cur_linea;
    sig_linea = cur_linea;

    int c = lege_c();
    if (c == -1) {
        sig.genus = T_EOF;
        return T_EOF;
    }

    /* nova linea — potentialiter directiva */
    if (c == '\n')
        goto inicio;

    /* directiva praeprocessoris */
    if (c == '#') {
        if (tracta_directivam())
            goto inicio;
        sig.genus = T_HASH;
        return T_HASH;
    }

    /* lineas suppressas ignora */
    if (!cond_activa()) {
        /* praetermitte usque ad novam lineam */
        while (c != '\n' && c != -1)
            c = lege_c();
        goto inicio;
    }

    /* numerus */
    if (c >= '0' && c <= '9') {
        long val = 0;
        if (c == '0') {
            c = lege_c();
            if (c == 'x' || c == 'X') {
                while ((c = lege_c()) != -1) {
                    if (c >= '0' && c <= '9')
                        val = val * 16 + (c - '0');
                    else if (c >= 'a' && c <= 'f')
                        val = val * 16 + (c - 'a' + 10);
                    else if (c >= 'A' && c <= 'F')
                        val = val * 16 + (c - 'A' + 10);
                    else
                        break;
                }
            } else if (c >= '0' && c <= '7') {
                val = c - '0';
                while ((c = lege_c()) != -1 && c >= '0' && c <= '7')
                    val = val * 8 + (c - '0');
            } else {
                /* erat solum 0 */
            }
        } else {
            val = c - '0';
            while ((c = lege_c()) != -1 && c >= '0' && c <= '9')
                val = val * 10 + (c - '0');
        }
        /* tractatio numeri cum puncto (double/float → fictus ut long) */
        if (c == '.') {
            /* pars fractionalis — quia double est fictus, serva ut long */
            /* sed mantissa truncatur ad integrum partiale */
            long frac_part = 0;
            long frac_div  = 1;
            while ((c = lege_c()) != -1 && c >= '0' && c <= '9') {
                frac_part = frac_part * 10 + (c - '0');
                frac_div *= 10;
            }
            /* pro computatione ficta: valor = pars integra */
            /* frac_part ignoratur (nulla arithmetica FP) */
            (void)frac_part;
            (void)frac_div;
        }
        /* praetermitte suffixum (U, L, UL, ULL, f, F, etc.) */
        while (
            c == 'u' || c == 'U' || c == 'l' || c == 'L' ||
            c == 'f' || c == 'F'
        )
            c = lege_c();
        /* praetermitte exponentem (e, E) */
        if (c == 'e' || c == 'E') {
            c = lege_c();
            if (c == '+' || c == '-')
                c = lege_c();
            while (c >= '0' && c <= '9')
                c = lege_c();
        }
        if (c != -1)
            repone_c();
        sig.genus = T_NUM;
        sig.valor = val;
        return T_NUM;
    }

    /* chorda */
    if (c == '"') {
        int i = 0;
        while ((c = lege_c()) != -1 && c != '"') {
            if (c == '\\')
                c = lege_effugium();
            if (i < MAX_CHORDA - 1)
                sig.chorda[i++] = c;
        }
        sig.chorda[i]   = '\0';
        sig.lon_chordae = i;
        sig.genus       = T_STR;

        /* §6.4.5: concatenatio chordarum litteralium adiacentium */
        for (;;) {
            praetermitte_spatia();
            c = lege_c();
            /* praetermitte novas lineas inter chordas */
            while (c == '\n') {
                praetermitte_spatia();
                c = lege_c();
            }
            if (c == '"') {
                while ((c = lege_c()) != -1 && c != '"') {
                    if (c == '\\')
                        c = lege_effugium();
                    if (i < MAX_CHORDA - 1)
                        sig.chorda[i++] = c;
                }
                sig.chorda[i]   = '\0';
                sig.lon_chordae = i;
            } else {
                if (c != -1)
                    repone_c();
                break;
            }
        }
        return T_STR;
    }

    /* character */
    if (c == '\'') {
        c = lege_c();
        if (c == '\\')
            c = lege_effugium();
        sig.valor = c;
        c         = lege_c(); /* praetermitte ' conclusivum */
        sig.genus = T_CHARLIT;
        return T_CHARLIT;
    }

    /* identificator vel vocabulum clavis */
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') {
        int i = 0;
        sig.chorda[i++] = c;
        while ((c = lege_c()) != -1) {
            if (
                (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_'
            ) {
                if (i < MAX_CHORDA - 1)
                    sig.chorda[i++] = c;
            } else {
                repone_c();
                break;
            }
        }
        sig.chorda[i] = '\0';

        /* est vocabulum clavis? */
        for (int j = 0; vocabula[j].nomen; j++) {
            if (strcmp(sig.chorda, vocabula[j].nomen) == 0) {
                sig.genus = vocabula[j].genus;
                return sig.genus;
            }
        }

        /* est macra? */
        macra_t *m = macra_quaere(sig.chorda);
        if (m && m->est_functionalis) {
            /* macra functionalis — lege argumenta */
            praetermitte_spatia();
            int pc = lege_c();
            if (pc == '\n') {
                praetermitte_spatia();
                pc = lege_c();
            }
            if (pc == '(') {
                char args[16][MAX_CHORDA];
                int nargs   = 0;
                int profund = 0;
                int ai      = 0;
                for (;;) {
                    pc = lege_c();
                    if (pc == -1)
                        break;
                    if (pc == '(') {
                        profund++;
                        if (ai < MAX_CHORDA - 1)
                            args[nargs][ai++] = pc;
                    } else if (pc == ')') {
                        if (profund == 0) {
                            args[nargs][ai] = '\0';
                            if (ai > 0 || nargs > 0)
                                nargs++;
                            break;
                        }
                        profund--;
                        if (ai < MAX_CHORDA - 1)
                            args[nargs][ai++] = pc;
                    } else if (pc == ',' && profund == 0) {
                        args[nargs][ai] = '\0';
                        nargs++;
                        ai = 0;
                    } else {
                        if (ai < MAX_CHORDA - 1)
                            args[nargs][ai++] = pc;
                    }
                }
                /* substitue parametros in corpore macrae */
                static char expansio_alvei[8][MAX_CHORDA * 2];
                static int expansio_vertex = 0;
                char *expansio = expansio_alvei[expansio_vertex % 8];
                expansio_vertex++;
                int ei = 0;
                const char *corpus = m->valor;
                int clon = (int)strlen(corpus);
                for (int ci = 0; ci < clon; ) {
                    /* proba si est nomen parametri */
                    if (
                        (corpus[ci] >= 'a' && corpus[ci] <= 'z') ||
                        (corpus[ci] >= 'A' && corpus[ci] <= 'Z') ||
                        corpus[ci] == '_'
                    ) {
                        char tok[128];
                        int ti = 0;
                        while (
                            ci < clon && (
                                (corpus[ci] >= 'a' && corpus[ci] <= 'z') ||
                                (corpus[ci] >= 'A' && corpus[ci] <= 'Z') ||
                                (corpus[ci] >= '0' && corpus[ci] <= '9') ||
                                corpus[ci] == '_'
                            )
                        ) {
                            if (ti < 127)
                                tok[ti++] = corpus[ci];
                            ci++;
                        }
                        tok[ti] = '\0';
                        /* quaere in parametris */
                        int invenit = 0;
                        for (int pi = 0; pi < m->num_parametrorum; pi++) {
                            if (strcmp(tok, m->parametri[pi]) == 0 && pi < nargs) {
                                /* substitue argumentum */
                                const char *arg = args[pi];
                                while (*arg && ei < MAX_CHORDA * 2 - 1)
                                    expansio[ei++] = *arg++;
                                invenit = 1;
                                break;
                            }
                        }
                        if (!invenit) {
                            for (int ki = 0; ki < ti && ei < MAX_CHORDA * 2 - 1; ki++)
                                expansio[ei++] = tok[ki];
                        }
                    } else {
                        if (ei < MAX_CHORDA * 2 - 1)
                            expansio[ei++] = corpus[ci];
                        ci++;
                    }
                }
                expansio[ei] = '\0';
                pelle_expansionem(expansio, ei);
                goto inicio;
            } else {
                repone_c();
            }
        } else if (m && m->valor[0] != '\0') {
            /* expande macram simplicem — pelle textum expansionis */
            pelle_expansionem(m->valor, strlen(m->valor));
            goto inicio;
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
    case '~': sig.genus = T_TILDE; return T_TILDE;
    case '?': sig.genus = T_QUESTION; return T_QUESTION;
    case ':': sig.genus = T_COLON; return T_COLON;
    case ';': sig.genus = T_SEMICOLON; return T_SEMICOLON;
    case ',': sig.genus = T_COMMA; return T_COMMA;
    case '(': sig.genus = T_LPAREN; return T_LPAREN;
    case ')': sig.genus = T_RPAREN; return T_RPAREN;
    case '[': sig.genus = T_LBRACKET; return T_LBRACKET;
    case ']': sig.genus = T_RBRACKET; return T_RBRACKET;
    case '{': sig.genus = T_LBRACE; return T_LBRACE;
    case '}': sig.genus = T_RBRACE; return T_RBRACE;
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
 * signum spectans (peek)
 * ================================================================ */

static signum_t sig_spectans;
static int habet_spectantem = 0;

void lex_initia(const char *nomen, const char *fons, int longitudo)
{
    cur_nomen     = nomen;
    cur_fons      = fons;
    cur_longitudo = longitudo;
    cur_positus   = 0;
    cur_linea     = 1;
    vertex_plicarum = 0;
    vertex_expansionum = 0;
    habet_spectantem = 0;
    cond_vertex = 0;
    num_macrarum = 0;
    num_typedef_nominum = 0;
}

void lex_proximum(void)
{
    if (habet_spectantem) {
        memcpy(&sig, &sig_spectans, sizeof(signum_t));
        sig_linea        = sig.linea;
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
    lege_signum_internum();
    memcpy(&sig_spectans, &sig, sizeof(signum_t));
    memcpy(&sig, &salva, sizeof(signum_t));
    sig_linea        = salva_linea;
    habet_spectantem = 1;
    return sig_spectans.genus;
}
