/* pouet_party.c - v4 con malloc Amiga (FAST con fallback CHIP) */

#include <stdio.h>
#include <string.h>
#include <exec/memory.h>
#include <proto/exec.h>

#define MAX_STR 256
#define MAX_PRODS 256
#define MAX_INVITATIONS 32

typedef struct {
    char id[16];
    char name[MAX_STR];
    char type[MAX_STR];
    char groups[MAX_STR];
    char platform[MAX_STR];
    char compo_name[MAX_STR];
    char place[8];
    char voteup[16];
    char votepig[16];
    char votedown[16];
    char voteavg[16];
    char download[MAX_STR];
} Prod;

typedef struct {
    char id[16];
    char name[MAX_STR];
    char web[MAX_STR];
    char added_date[MAX_STR];
    char link_download[MAX_STR];
    char link_demozoo[16];
} Party;

typedef struct {
    char compo[MAX_STR];
    char platform[MAX_STR];
    char id[16];
    int inv_only;
    int noheader;
    int only_id;
} Filters;

/* Buffer JSON allocato dinamicamente con AllocMem */
static char  *g_json      = NULL;
static ULONG  g_json_size = 0;   /* dimensione allocata, serve per FreeMem */

static Party g_party;
static Prod  g_prods[MAX_PRODS];
static int   g_prod_count = 0;
static Prod  g_invitations[MAX_INVITATIONS];
static int   g_inv_count  = 0;

/* ================= allocazione Amiga ================= */

/*
 * Alloca 'size' byte chiedendo prima MEMF_FAST|MEMF_CLEAR.
 * Se la Fast RAM non e' disponibile (o non c'e' abbastanza),
 * ritenta con MEMF_CHIP|MEMF_CLEAR.
 * Restituisce il puntatore oppure NULL se entrambi falliscono.
 * Salva la dimensione in *out_size per poter chiamare FreeMem
 * con il parametro corretto.
 */
static char *amiga_alloc(ULONG size, ULONG *out_size)
{
    char *ptr;

    *out_size = size;

    /* Primo tentativo: Fast RAM */
    ptr = (char *)AllocMem(size, MEMF_FAST | MEMF_CLEAR);
    if (ptr) {
        return ptr;
    }

    /* Fallback: Chip RAM */
    ptr = (char *)AllocMem(size, MEMF_CHIP | MEMF_CLEAR);
    return ptr;   /* puo' essere NULL se anche la chip e' esaurita */
}

static void amiga_free(char *ptr, ULONG size)
{
    if (ptr) FreeMem(ptr, size);
}

/* ================= parsing base ================= */

static const char *skip_ws(const char *p)
{
    while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n') p++;
    return p;
}

static const char *read_scalar(const char *p, char *dst, int len)
{
    int i = 0;
    p = skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\') {
                p++; if (!*p) break;
                if (i < len-1) dst[i++] = *p;
            } else {
                if (i < len-1) dst[i++] = *p;
            }
            p++;
        }
        dst[i] = '\0';
        if (*p == '"') p++;
        return p;
    }
    while (*p && *p!=',' && *p!='}' && *p!=']')
        if (i < len-1) dst[i++] = *p++;
    dst[i] = '\0';
    return p;
}

static const char *skip_value(const char *p)
{
    int d;
    p = skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\') { p++; if (*p) p++; }
            else p++;
        }
        if (*p == '"') p++;
        return p;
    }
    if (*p == '{') {
        d=1; p++;
        while (*p && d>0) {
            if (*p=='{') d++;
            else if (*p=='}') d--;
            p++;
        }
        return p;
    }
    if (*p == '[') {
        d=1; p++;
        while (*p && d>0) {
            if (*p=='[') d++;
            else if (*p==']') d--;
            p++;
        }
        return p;
    }
    while (*p && *p!=',' && *p!='}' && *p!=']') p++;
    return p;
}

typedef const char *(*FieldCb)(const char*, const char*, void*);

static const char *iter_object(const char *p, FieldCb cb, void *ud)
{
    char key[64];
    const char *after;
    if (*p == '{') p++;
    while (*p) {
        p = skip_ws(p);
        if (!*p || *p == '}') { if (*p == '}') p++; break; }
        if (*p == ',') { p++; continue; }
        if (*p != '"') { p++; continue; }
        p = read_scalar(p, key, sizeof(key));
        p = skip_ws(p);
        if (*p == ':') p++;
        p = skip_ws(p);
        after = cb ? cb(p, key, ud) : NULL;
        p = after ? after : skip_value(p);
    }
    return p;
}

/* ================= filtri ================= */

static int ci_contains(const char *hay, const char *needle)
{
    int hi, ni;
    if (!needle[0]) return 1;
    for (hi=0; hay[hi]; hi++) {
        for (ni=0; needle[ni]; ni++) {
            char h = hay[hi+ni];
            char n = needle[ni];
            if (!h) break;
            if (h>='A'&&h<='Z') h+=32;
            if (n>='A'&&n<='Z') n+=32;
            if (h!=n) break;
        }
        if (!needle[ni]) return 1;
    }
    return 0;
}

static int prod_matches(const Prod *pr, const Filters *f)
{
    if (f->id[0]       && strcmp(pr->id, f->id)!=0)             return 0;
    if (f->compo[0]    && !ci_contains(pr->compo_name, f->compo)) return 0;
    if (f->platform[0] && !ci_contains(pr->platform, f->platform)) return 0;
    return 1;
}

/* ================= parsing prod ================= */

static const char *cb_prod(const char *p, const char *key, void *ud)
{
    Prod *pr = (Prod*)ud;
    if (!strcmp(key,"id"))               return read_scalar(p, pr->id,        sizeof(pr->id));
    if (!strcmp(key,"name"))             return read_scalar(p, pr->name,      sizeof(pr->name));
    if (!strcmp(key,"party_compo_name")) return read_scalar(p, pr->compo_name,sizeof(pr->compo_name));
    if (!strcmp(key,"party_place"))      return read_scalar(p, pr->place,     sizeof(pr->place));
    return NULL;
}

static const char *parse_prod_obj(const char *p, Prod *pr)
{
    memset(pr, 0, sizeof(*pr));
    iter_object(p, cb_prod, pr);
    return skip_value(p);
}

/* ================= parsing party ================= */

static const char *cb_party(const char *p, const char *key, void *ud)
{
    Party *pa = (Party*)ud;
    if (!strcmp(key,"id"))   return read_scalar(p, pa->id,   sizeof(pa->id));
    if (!strcmp(key,"name")) return read_scalar(p, pa->name, sizeof(pa->name));
    if (!strcmp(key,"prods")) {
        const char *q = skip_ws(p);
        if (*q == '[') {
            q++;
            while (*q) {
                q = skip_ws(q);
                if (*q == ']') { q++; break; }
                if (*q == ',') { q++; continue; }
                if (*q == '{' && g_prod_count < MAX_PRODS)
                    parse_prod_obj(q, &g_prods[g_prod_count++]);
                q = skip_value(q);
            }
        }
        return skip_value(p);
    }
    return NULL;
}

static const char *cb_root(const char *p, const char *key, void *ud)
{
    if (!strcmp(key,"party"))
        return iter_object(p, cb_party, ud);
    return NULL;
}

/* ================= lettura file ================= */

static int read_file(const char *path)
{
    FILE *f;
    long  size;

    f = fopen(path, "r");
    if (!f) return 0;

    /* Determina la dimensione esatta del file */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    size = ftell(f);
    if (size <= 0)                   { fclose(f); return 0; }
    rewind(f);

    /* Alloca con preferenza Fast RAM, fallback Chip RAM */
    g_json = amiga_alloc((ULONG)(size + 1), &g_json_size);
    if (!g_json) {
        fprintf(stderr, "Memoria insufficiente (%ld bytes richiesti)\n", size + 1);
        fclose(f);
        return 0;
    }

    fread(g_json, 1, (size_t)size, f);
    g_json[size] = '\0';
    fclose(f);
    return (int)size;
}

/* ================= output ================= */

static void print_prods_filtered(Prod *arr, int n, const Filters *f)
{
    int i, shown = 0;
    for (i = 0; i < n; i++) {
        if (!prod_matches(&arr[i], f)) continue;
        if (f->only_id) {
            printf("%s\n", arr[i].id[0] ? arr[i].id : "?");
        } else {
            printf("[%-6s] %s\n",
                arr[i].id[0]   ? arr[i].id   : "?",
                arr[i].name[0] ? arr[i].name : "(n/a)");
        }
        shown++;
    }
    if (!shown && !f->only_id)
        printf("(nessun risultato)\n");
}

static void print_all(const Filters *f)
{
    if (!f->only_id && !f->noheader)
        printf("Party: %s (id:%s)\n", g_party.name, g_party.id);
    print_prods_filtered(g_prods, g_prod_count, f);
}

/* ================= args ================= */

static int parse_args(int argc, char *argv[], Filters *f)
{
    int i;
    memset(f, 0, sizeof(*f));
    for (i = 2; i < argc; i++) {
        if (!strcmp(argv[i],"--compo") && i+1<argc)
            strncpy(f->compo, argv[++i], MAX_STR-1);
        else if (!strcmp(argv[i],"--platform") && i+1<argc)
            strncpy(f->platform, argv[++i], MAX_STR-1);
        else if (!strcmp(argv[i],"--id") && i+1<argc)
            strncpy(f->id, argv[++i], 15);
        else if (!strcmp(argv[i],"--inv"))
            f->inv_only = 1;
        else if (!strcmp(argv[i],"--noheader"))
            f->noheader = 1;
        else if (!strcmp(argv[i],"--onlyid"))
            f->only_id = 1;
        else
            return 0;
    }
    return 1;
}

/* ================= main ================= */

int main(int argc, char *argv[])
{
    Filters f;

    if (argc < 2) return 1;
    if (!parse_args(argc, argv, &f)) return 1;

    if (!read_file(argv[1])) return 1;

    /* Parsing: usa g_json ancora in memoria */
    iter_object(g_json, cb_root, &g_party);

    /* Libera subito il buffer JSON: le struct Prod/Party sono gia' popolate */
    amiga_free(g_json, g_json_size);
    g_json      = NULL;
    g_json_size = 0;

    print_all(&f);
    return 0;
}
