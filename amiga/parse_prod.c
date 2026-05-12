/*
 * pouet_prod.c
 * Parser JSON zero-malloc per l'API pouet.net /v1/prod/?id=N
 *
 * Portabile: C89/C90, nessuna dipendenza esterna tranne libc.
 *
 * Compilazione Linux:
 *   gcc -ansi -pedantic -Wall -o pouet_prod pouet_prod.c
 *
 * Compilazione Amiga (vbcc):
 *   vc +aos68k -o pouet_prod pouet_prod.c
 *
 * Uso:
 *   curl -s "https://api.pouet.net/v1/prod/?id=1" > prod1.json
 *   ./pouet_prod prod1.json
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Limiti statici                                                       */
/* ------------------------------------------------------------------ */
#define JSON_BUF_SIZE   524288   /* 512 KB                             */
#define MAX_STR         256
#define MAX_GROUPS      16
#define MAX_CREDITS     32
#define MAX_PLATFORMS   8

/* ------------------------------------------------------------------ */
/* Strutture dati                                                       */
/* ------------------------------------------------------------------ */
typedef struct {
    char id[16];
    char name[MAX_STR];
} Group;

typedef struct {
    char nickname[MAX_STR];
    char role[MAX_STR];
} Credit;

typedef struct {
    char name[MAX_STR];
} Platform;

typedef struct {
    char party_name[MAX_STR];
    char compo_name[MAX_STR];
    char ranking[8];
    char year[8];
} Placing;

typedef struct {
    char     id[16];
    char     name[MAX_STR];
    char     type[MAX_STR];
    char     added_date[MAX_STR];
    char     release_date[MAX_STR];
    char     voteup[16];
    char     votepig[16];
    char     votedown[16];
    char     voteavg[16];
    char     download[MAX_STR];
    char     screenshot[MAX_STR];
    char     demozoo[16];
    char     party_compo_name[MAX_STR];
    char     party_place[8];
    char     party_year[8];
    char     rank[16];
    Group    groups[MAX_GROUPS];
    int      group_count;
    Credit   credits[MAX_CREDITS];
    int      credit_count;
    Platform platforms[MAX_PLATFORMS];
    int      platform_count;
    Placing  placing;          /* primo placing */
    int      has_placing;
} Prod;

/* ------------------------------------------------------------------ */
/* Buffer globale                                                       */
/* ------------------------------------------------------------------ */
static char g_json[JSON_BUF_SIZE];
static Prod g_prod;

/* ================================================================== */
/* Primitivi di parsing                                                */
/* ================================================================== */

static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

/* Legge qualsiasi scalare JSON in dst (stringa quotata, numero, null, bool) */
static const char *read_scalar(const char *p, char *dst, int len)
{
    int i = 0;
    p = skip_ws(p);

    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\') {
                p++;
                if (!*p) break;
                switch (*p) {
                    case '"':  if (i < len-1) dst[i++] = '"';  break;
                    case '\\': if (i < len-1) dst[i++] = '\\'; break;
                    case '/':  if (i < len-1) dst[i++] = '/';  break;
                    case 'n':  if (i < len-1) dst[i++] = '\n'; break;
                    case 'r':  if (i < len-1) dst[i++] = '\r'; break;
                    case 't':  if (i < len-1) dst[i++] = '\t'; break;
                    default:   if (i < len-1) dst[i++] = *p;   break;
                }
            } else {
                if (i < len-1) dst[i++] = *p;
            }
            p++;
        }
        dst[i] = '\0';
        if (*p == '"') p++;
        return p;
    }

    /* numero, null, true, false */
    while (*p && *p != ',' && *p != '}' && *p != ']' &&
           *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
        if (i < len-1) dst[i++] = *p;
        p++;
    }
    dst[i] = '\0';
    return p;
}

/* Salta un valore JSON completo — garantisce avanzamento */
static const char *skip_value(const char *p)
{
    p = skip_ws(p);
    if (!*p) return p;

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
        int d = 1; p++;
        while (*p && d > 0) {
            if (*p == '"') {
                p++;
                while (*p && *p != '"') { if (*p=='\\'){p++;if(*p)p++;}else p++; }
                if (*p == '"') p++;
            } else if (*p=='{') { d++; p++; }
            else if (*p=='}') { d--; p++; }
            else p++;
        }
        return p;
    }
    if (*p == '[') {
        int d = 1; p++;
        while (*p && d > 0) {
            if (*p == '"') {
                p++;
                while (*p && *p != '"') { if (*p=='\\'){p++;if(*p)p++;}else p++; }
                if (*p == '"') p++;
            } else if (*p=='[') { d++; p++; }
            else if (*p==']') { d--; p++; }
            else p++;
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']')
        p++;
    return p;
}

/* ================================================================== */
/* Parser di sotto-oggetti specifici                                   */
/* ================================================================== */

/*
 * Itera un oggetto JSON chiamando cb(key, value_ptr, userdata)
 * per ogni coppia chiave/valore al livello corrente (non ricorsivo).
 * p punta a '{'.
 * Restituisce puntatore dopo '}'.
 */
typedef const char *(*FieldCb)(const char *val_p, const char *key, void *ud);

static const char *iter_object(const char *p, FieldCb cb, void *ud)
{
    char key[64];
    p = skip_ws(p);
    if (*p == '{') p++;
    while (*p) {
        p = skip_ws(p);
        if (!*p || *p == '}') { if (*p=='}') p++; break; }
        if (*p == ',') { p++; continue; }
        if (*p != '"') { p++; continue; }
        p = read_scalar(p, key, sizeof(key));
        p = skip_ws(p);
        if (*p == ':') p++;
        p = skip_ws(p);
        /* chiama callback: se restituisce non-NULL usa quel puntatore,
           altrimenti salta il valore */
        if (cb) {
            const char *after = cb(p, key, ud);
            p = after ? after : skip_value(p);
        } else {
            p = skip_value(p);
        }
    }
    return p;
}

/* ---- group ---- */
static const char *cb_group(const char *p, const char *key, void *ud)
{
    Group *g = (Group *)ud;
    if (strcmp(key, "id")   == 0) return read_scalar(p, g->id,   sizeof(g->id));
    if (strcmp(key, "name") == 0) return read_scalar(p, g->name, sizeof(g->name));
    return NULL;
}

/* ---- credit/user ---- */
typedef struct { char nick[MAX_STR]; } UserCtx;
static const char *cb_user(const char *p, const char *key, void *ud)
{
    UserCtx *u = (UserCtx *)ud;
    if (strcmp(key, "nickname") == 0) return read_scalar(p, u->nick, sizeof(u->nick));
    return NULL;
}

typedef struct { Credit *cr; } CreditCtx;
static const char *cb_credit(const char *p, const char *key, void *ud)
{
    CreditCtx *c = (CreditCtx *)ud;
    if (strcmp(key, "role") == 0)
        return read_scalar(p, c->cr->role, sizeof(c->cr->role));
    if (strcmp(key, "user") == 0) {
        UserCtx u; u.nick[0] = '\0';
        iter_object(p, cb_user, &u);
        strncpy(c->cr->nickname, u.nick, sizeof(c->cr->nickname)-1);
        c->cr->nickname[sizeof(c->cr->nickname)-1] = '\0';
        return skip_value(p);
    }
    return NULL;
}

/* ---- placing ---- */
typedef struct { Placing *pl; } PlacingCtx;
static const char *cb_party_inner(const char *p, const char *key, void *ud)
{
    Placing *pl = (Placing *)ud;
    if (strcmp(key, "name") == 0) return read_scalar(p, pl->party_name, sizeof(pl->party_name));
    return NULL;
}
static const char *cb_placing(const char *p, const char *key, void *ud)
{
    PlacingCtx *c = (PlacingCtx *)ud;
    if (strcmp(key, "ranking")    == 0) return read_scalar(p, c->pl->ranking,    sizeof(c->pl->ranking));
    if (strcmp(key, "year")       == 0) return read_scalar(p, c->pl->year,       sizeof(c->pl->year));
    if (strcmp(key, "compo_name") == 0) return read_scalar(p, c->pl->compo_name, sizeof(c->pl->compo_name));
    if (strcmp(key, "party")      == 0) {
        iter_object(p, cb_party_inner, c->pl);
        return skip_value(p);
    }
    return NULL;
}

/* ---- prod principale ---- */
static const char *cb_prod(const char *p, const char *key, void *ud)
{
    Prod *pr = (Prod *)ud;

    if (strcmp(key, "id")              == 0) return read_scalar(p, pr->id,              sizeof(pr->id));
    if (strcmp(key, "name")            == 0) return read_scalar(p, pr->name,            sizeof(pr->name));
    if (strcmp(key, "type")            == 0) return read_scalar(p, pr->type,            sizeof(pr->type));
    if (strcmp(key, "addedDate")       == 0) return read_scalar(p, pr->added_date,      sizeof(pr->added_date));
    if (strcmp(key, "releaseDate")     == 0) return read_scalar(p, pr->release_date,    sizeof(pr->release_date));
    if (strcmp(key, "voteup")          == 0) return read_scalar(p, pr->voteup,          sizeof(pr->voteup));
    if (strcmp(key, "votepig")         == 0) return read_scalar(p, pr->votepig,         sizeof(pr->votepig));
    if (strcmp(key, "votedown")        == 0) return read_scalar(p, pr->votedown,        sizeof(pr->votedown));
    if (strcmp(key, "voteavg")         == 0) return read_scalar(p, pr->voteavg,         sizeof(pr->voteavg));
    if (strcmp(key, "download")        == 0) return read_scalar(p, pr->download,        sizeof(pr->download));
    if (strcmp(key, "screenshot")      == 0) return read_scalar(p, pr->screenshot,      sizeof(pr->screenshot));
    if (strcmp(key, "demozoo")         == 0) return read_scalar(p, pr->demozoo,         sizeof(pr->demozoo));
    if (strcmp(key, "party_compo_name")== 0) return read_scalar(p, pr->party_compo_name,sizeof(pr->party_compo_name));
    if (strcmp(key, "party_place")     == 0) return read_scalar(p, pr->party_place,     sizeof(pr->party_place));
    if (strcmp(key, "party_year")      == 0) return read_scalar(p, pr->party_year,      sizeof(pr->party_year));
    if (strcmp(key, "rank")            == 0) return read_scalar(p, pr->rank,            sizeof(pr->rank));

    /* --- groups: array di oggetti --- */
    if (strcmp(key, "groups") == 0) {
        const char *q = skip_ws(p);
        if (*q == '[') {
            q++;
            while (*q) {
                q = skip_ws(q);
                if (*q == ']') { q++; break; }
                if (*q == ',') { q++; continue; }
                if (*q == '{' && pr->group_count < MAX_GROUPS) {
                    q = iter_object(q, cb_group, &pr->groups[pr->group_count]);
                    pr->group_count++;
                } else {
                    q = skip_value(q);
                }
            }
        }
        return skip_value(p);
    }

    /* --- platforms: OGGETTO con chiavi numeriche --- */
    if (strcmp(key, "platforms") == 0) {
        const char *q = skip_ws(p);
        if (*q == '{') {
            q++;
            while (*q) {
                char pkey[32];
                q = skip_ws(q);
                if (*q == '}') { q++; break; }
                if (*q == ',') { q++; continue; }
                if (*q != '"') { q++; continue; }
                /* chiave numerica tipo "73" — la ignoriamo */
                q = read_scalar(q, pkey, sizeof(pkey));
                q = skip_ws(q);
                if (*q == ':') q++;
                q = skip_ws(q);
                /* valore: oggetto con "name" */
                if (*q == '{' && pr->platform_count < MAX_PLATFORMS) {
                    Platform *pl = &pr->platforms[pr->platform_count];
                    {
                        /* estrai "name" dall oggetto platform */
                        const char *qq = q;
                        char pk2[32];
                        if (*qq == '{') qq++;
                        while (*qq) {
                            qq = skip_ws(qq);
                            if (!*qq || *qq == '}') break;
                            if (*qq == ',') { qq++; continue; }
                            if (*qq != '"') { qq++; continue; }
                            qq = read_scalar(qq, pk2, sizeof(pk2));
                            qq = skip_ws(qq);
                            if (*qq == ':') qq++;
                            qq = skip_ws(qq);
                            if (strcmp(pk2, "name") == 0)
                                qq = read_scalar(qq, pl->name, sizeof(pl->name));
                            else
                                qq = skip_value(qq);
                        }
                    }
                    pr->platform_count++;
                }
                q = skip_value(q);
            }
        }
        return skip_value(p);
    }

    /* --- credits: array di {user, role} --- */
    if (strcmp(key, "credits") == 0) {
        const char *q = skip_ws(p);
        if (*q == '[') {
            q++;
            while (*q) {
                q = skip_ws(q);
                if (*q == ']') { q++; break; }
                if (*q == ',') { q++; continue; }
                if (*q == '{' && pr->credit_count < MAX_CREDITS) {
                    CreditCtx cc;
                    cc.cr = &pr->credits[pr->credit_count];
                    cc.cr->nickname[0] = '\0';
                    cc.cr->role[0]     = '\0';
                    q = iter_object(q, cb_credit, &cc);
                    pr->credit_count++;
                } else {
                    q = skip_value(q);
                }
            }
        }
        return skip_value(p);
    }

    /* --- placings: array, prendiamo solo il primo --- */
    if (strcmp(key, "placings") == 0) {
        const char *q = skip_ws(p);
        if (*q == '[') {
            q++;
            q = skip_ws(q);
            if (*q == '{' && !pr->has_placing) {
                PlacingCtx pc;
                pc.pl = &pr->placing;
                memset(pc.pl, 0, sizeof(*pc.pl));
                iter_object(q, cb_placing, &pc);
                pr->has_placing = 1;
            }
        }
        return skip_value(p);
    }

    return NULL; /* salta */
}

/* ---- radice ---- */
static const char *cb_root(const char *p, const char *key, void *ud)
{
    if (strcmp(key, "prod") == 0) {
        iter_object(p, cb_prod, ud);
        return skip_value(p);
    }
    return NULL;
}

/* ================================================================== */
/* Lettura file                                                        */
/* ================================================================== */
static int read_file(const char *path)
{
    FILE *f;
    int total = 0, n;

    f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Errore: impossibile aprire '%s'\n", path);
        return 0;
    }
    while ((n = (int)fread(g_json + total, 1,
                           JSON_BUF_SIZE - 1 - total, f)) > 0) {
        total += n;
        if (total >= JSON_BUF_SIZE - 1) {
            fprintf(stderr, "Avviso: file troncato a %d byte\n", JSON_BUF_SIZE);
            break;
        }
    }
    fclose(f);
    g_json[total] = '\0';
    return total;
}

/* ================================================================== */
/* Output                                                              */
/* ================================================================== */
static void print_prod(const Prod *pr, int idx)
{
    int i;
    switch (idx)
    {
        case 1:  printf("%s\n", pr->id[0]              ? pr->id              : "(n/a)"); break;
        case 2:  printf("%s\n", pr->name[0]            ? pr->name            : "(n/a)"); break;
        case 3:  printf("%s\n", pr->type[0]            ? pr->type            : "(n/a)"); break;
        case 4:  printf("%s\n", pr->added_date[0]      ? pr->added_date      : "(n      /a)"); break;
        case 5:  printf("%s\n", pr->release_date[0]             ? pr->release_date    : "(n/a)"); break;
        case 6:  printf("%s\n", pr->voteup[0]          ? pr->voteup          : "0"); break;
        case 7:  printf("%s\n", pr->votepig[0]         ? pr->votepig         : "0"); break;
        case 8:  printf("%s\n", pr->votedown[0]        ? pr->votedown        : "0"); break;
        case 9:  printf("%s\n", pr->voteavg[0]         ? pr->voteavg         : "0"); break;
        case 10: printf("%s\n", pr->download[0]        ? pr->download        : "(n/a)"); break;
        case 11: printf("%s\n", pr->screenshot[0]      ? pr->screenshot      : "(n/a)"); break;
        case 12: printf("%s\n", pr->demozoo[0]         ? pr->demozoo         : "(n/a)"); break;
        case 13: printf("%s\n", pr->party_compo_name[0] ? pr->party_compo_name : "(n/a)"); break;
        case 14: printf("%s\n", pr->party_place[0]      ? pr->party_place     : "(n/a)"); break;
        case 15: printf("%s\n", pr->party_year[0]       ? pr->party_year      : "(n/a)"); break;
        case 16: printf("%s\n", pr->rank[0]            ? pr->rank            : "(n/a)"); break;
        case 17: printf("%d\n", pr->group_count); break;
        case 18: for (i = 0; i < pr->group_count; i++)
                    printf("%s\n", pr->groups[i].name[0] ? pr->groups[i].name : "(n/a)"); break;
        case 19: printf("%d\n", pr->platform_count); break;
        case 20: for (i = 0; i < pr->platform_count; i++)
                    printf("%s\n", pr->platforms[i].name[0] ? pr->platforms[i].name : "(n/a)"); break;
        case 21: printf("%d\n", pr->credit_count); break;
        case 22: for (i = 0; i < pr->credit_count; i++)
                    printf("%s\n", pr->credits[i].nickname[0] ? pr->credits[i].nickname : "(n/a)"); break;
        case 23: for (i = 0; i < pr->credit_count; i++)
                    printf("%s\n", pr->credits[i].role[0] ? pr->credits[i].role : "(n/a)"); break; 
        case 24: if (pr->has_placing) {
                    printf("%s\n", pr->placing.party_name[0] ? pr->placing.party_name : "(n/a)");
                    printf("%s\n", pr->placing.compo_name[0] ? pr->placing.compo_name : "(n/a)");
                    printf("%s\n", pr->placing.ranking[0]    ? pr->placing.ranking    : "(n/a)");
                    printf("%s\n", pr->placing.year[0]       ? pr->placing.year       : "(n/a)");
                } else {
                    printf("(no placing)\n");
                }
                break;
        case 25: for (i = 0; i < pr->group_count; i++)
                    printf("%s\n", pr->groups[i].id[0] ? pr->groups[i].id : "(n/a)"); break;
        case 26: for (i = 0; i < pr->credit_count; i++)
                    printf("%s\n", pr->credits[i].nickname[0] ? pr->credits[i].nickname : "(n/a)"); break;
        case 27: for (i = 0; i < pr->credit_count; i++)
                    printf("%s\n", pr->credits[i].role[0] ? pr->credits[i].role : "(n/a)"); break;  
        case 28: for (i = 0; i < pr->platform_count; i++)
                    printf("%s\n", pr->platforms[i].name[0] ? pr->platforms[i].name : "(n/a)"); break;
        case 29: for (i = 0; i < pr->platform_count; i++)
                    printf("%s\n", pr->platforms[i].name[0] ? pr->platforms[i].name : "(n/a)"); break;
        case 30: for (i = 0; i < pr->group_count; i++)
                    printf("%s\n", pr->groups[i].id[0] ? pr->groups[i].id : "(n/a)"); break;
        case 31: for (i = 0; i < pr->credit_count; i++)
                    printf("%s\n", pr->credits[i].nickname[0] ? pr->credits[i].nickname : "(n/a)"); break;
        case 32: for (i = 0; i < pr->credit_count; i++)
                    printf("%s\n", pr->credits[i].role[0] ? pr->credits[i].role : "(n/a)"); break;
        case 33: for (i = 0; i < pr->platform_count; i++)
                    printf("%s\n", pr->platforms[i].name[0] ? pr->platforms[i].name : "(n/a)"); break;
        case 34: if (pr->release_date) printf("%c%c%c%c\n", pr->release_date[0],pr->release_date[1],pr->release_date[2],pr->release_date[3]); 
                 else printf("(n/a)\n");
                 break;
        
        
        default: 
    
     
            printf("========================================\n");
            printf("  ID          : %s\n", pr->id[0]              ? pr->id              : "(n/a)");
            printf("  Nome        : %s\n", pr->name[0]            ? pr->name            : "(n/a)");
            printf("  Tipo        : %s\n", pr->type[0]            ? pr->type            : "(n/a)");
            printf("  Aggiunto    : %s\n", pr->added_date[0]      ? pr->added_date      : "(n/a)");
            printf("  Release     : %s\n", pr->release_date[0]    ? pr->release_date    : "(n/a)");
            printf("  Rank        : %s\n", pr->rank[0]            ? pr->rank            : "(n/a)");
            printf("  Download    : %s\n", pr->download[0]        ? pr->download        : "(n/a)");
            printf("  Screenshot  : %s\n", pr->screenshot[0]      ? pr->screenshot      : "(n/a)");
            printf("  Demozoo     : %s\n", pr->demozoo[0] && strcmp(pr->demozoo,"0")!=0 ? pr->demozoo : "(n/a)");

            printf("\n--- Voti ---\n");
            printf("  Thumb up    : %s\n", pr->voteup[0]   ? pr->voteup   : "0");
            printf("  Thumb pig   : %s\n", pr->votepig[0]  ? pr->votepig  : "0");
            printf("  Thumb down  : %s\n", pr->votedown[0] ? pr->votedown : "0");
            printf("  Media       : %s\n", pr->voteavg[0]  ? pr->voteavg  : "0");

            if (pr->platform_count > 0) {
                printf("\n--- Piattaforme ---\n");
                for (i = 0; i < pr->platform_count; i++)
                    printf("  [%d] %s\n", i+1, pr->platforms[i].name);
            }

            if (pr->group_count > 0) {
                printf("\n--- Gruppi ---\n");
                for (i = 0; i < pr->group_count; i++)
                    printf("  [%d] %s (id: %s)\n", i+1, pr->groups[i].name, pr->groups[i].id);
            }

            if (pr->has_placing) {
                printf("\n--- Party ---\n");
                printf("  Party  : %s (%s)\n", pr->placing.party_name[0] ? pr->placing.party_name : "(n/a)",
                                                pr->placing.year[0]       ? pr->placing.year       : "?");
                printf("  Compo  : %s\n",      pr->placing.compo_name[0] ? pr->placing.compo_name : "(n/a)");
                printf("  Posto  : %s\n",      pr->placing.ranking[0]    ? pr->placing.ranking    : "(n/a)");
            }

            if (pr->credit_count > 0) {
                printf("\n--- Credits ---\n");
                for (i = 0; i < pr->credit_count; i++)
                    printf("  %-24s  %s\n", pr->credits[i].nickname, pr->credits[i].role);
            }

            printf("========================================\n");
    }
}

/* ================================================================== */
/* main                                                                */
/* ================================================================== */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr,
            "Uso: %s <file.json>\n"
            "Esempio:\n"
            "  curl -s \"https://api.pouet.net/v1/prod/?id=1\" > prod1.json\n"
            "  %s prod1.json\n",
            argv[0], argv[0]);
        return 1;
    }

    if (!read_file(argv[1]))
    {
        fprintf(stderr, "Errore: impossibile leggere '%s' falling back to ram:prod.json\n", argv[1]);
        if (!read_file("ram:prod.json"))
        {
            fprintf(stderr, "Errore: impossibile leggere 'ram:prod.json'\n");
            return 1;
        }
    }

    memset(&g_prod, 0, sizeof(g_prod));
    iter_object(g_json, cb_root, &g_prod);

    if (!g_prod.id[0]) {
        fprintf(stderr, "Errore: nessuna prod trovata in '%s'\n", argv[1]);
        return 1;
    }

    print_prod(&g_prod, argc > 2 ? atoi(argv[2]) : 0 );
    return 0;
}