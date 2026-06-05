/*
 * pouet_lastid.c
 *
 * Dato un file JSON tipo:
 *   { "success": true, "prods": [ {"id":"106327", ...}, ... ] }
 *
 * Stampa il primo "id" trovato nell'array "prods".
 * Poiche' Pouet restituisce i prod in ordine decrescente di id,
 * il primo e' il piu' alto (= l'ultimo prod aggiunto).
 *
 * Strategia memory-friendly per Amiga:
 *   - Legge il file a chunk da CHUNK_SIZE byte (default 4 KB)
 *   - Non carica mai l'intero JSON in RAM
 *   - Non usa malloc/AllocMem per il JSON
 *   - Unico buffer statico sul BSS: 4 KB
 *
 * Compilazione (vbcc/gcc cross):
 *   vc +aos68k -o pouet_lastid pouet_lastid.c
 *   m68k-amigaos-gcc -O2 -o pouet_lastid pouet_lastid.c
 */

#include <stdio.h>
#include <string.h>

#define CHUNK_SIZE 4096

/* stati della mini state-machine */
typedef enum {
    S_ROOT,          /* aspetta la chiave "prods"                 */
    S_PRODS_ARRAY,   /* dentro l'array prods, aspetta '['         */
    S_FIRST_OBJ,     /* dentro il primo oggetto, aspetta "id"     */
    S_READ_ID,       /* dopo ':'  legge il valore dell'id         */
    S_DONE           /* id trovato, possiamo uscire               */
} State;

int main(int argc, char *argv[])
{
    FILE  *f;
    static char buf[CHUNK_SIZE + 1]; /* +1 per il terminatore di sicurezza */
    size_t n;
    State  state = S_ROOT;
    char   id_val[32];
    int    id_len = 0;
    int    depth  = 0;       /* profondita' '{' per S_FIRST_OBJ   */

    if (argc < 2) {
        fprintf(stderr, "Uso: %s <file.json>\n", argv[0]);
        return 1;
    }

    f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "Impossibile aprire: %s\n", argv[1]);
        return 1;
    }

    while (state != S_DONE) {
        n = fread(buf, 1, CHUNK_SIZE, f);
        if (n == 0) break;           /* EOF o errore                */
        buf[n] = '\0';

        {
            const char *p = buf;
            const char *end = buf + n;

            while (p < end && state != S_DONE) {
                char c = *p;

                switch (state) {

                /* ---- cerca la stringa "prods" come chiave ---- */
                case S_ROOT: {
                    static const char target[] = "\"prods\"";
                    static int        tpos      = 0;
                    if (c == target[tpos]) {
                        tpos++;
                        if (target[tpos] == '\0') {
                            tpos  = 0;
                            state = S_PRODS_ARRAY;
                        }
                    } else {
                        tpos = (c == target[0]) ? 1 : 0;
                    }
                    break;
                }

                /* ---- aspetta '[' che apre l'array prods ---- */
                case S_PRODS_ARRAY:
                    if (c == '[') state = S_FIRST_OBJ;
                    break;

                /* ---- dentro il primo oggetto, cerca "id" ---- */
                case S_FIRST_OBJ: {
                    static const char tid[] = "\"id\"";
                    static int        tpos  = 0;

                    /* traccia la profondita' per stare nel primo oggetto */
                    if      (c == '{') depth++;
                    else if (c == '}') { depth--; if (depth < 0) goto done; }

                    if (c == tid[tpos]) {
                        tpos++;
                        if (tid[tpos] == '\0') {
                            tpos  = 0;
                            state = S_READ_ID;
                        }
                    } else {
                        tpos = (c == tid[0]) ? 1 : 0;
                    }
                    break;
                }

                /* ---- legge il valore dopo "id": ---- */
                case S_READ_ID:
                    /* salta whitespace e ':' */
                    if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':')
                        break;

                    /* legge cifre (numeriche) o stringa quotata */
                    if (c == '"') {
                        /* valore stringa: leggi fino a '"' di chiusura */
                        p++;
                        while (p < end && *p != '"' && id_len < (int)sizeof(id_val)-1)
                            id_val[id_len++] = *p++;
                        id_val[id_len] = '\0';
                        state = S_DONE;
                    } else if (c >= '0' && c <= '9') {
                        /* valore numerico: leggi tutte le cifre */
                        while (p < end && *p >= '0' && *p <= '9' && id_len < (int)sizeof(id_val)-1)
                            id_val[id_len++] = *p++;
                        id_val[id_len] = '\0';
                        state = S_DONE;
                        continue;   /* p gia' avanzato nel while interno */
                    }
                    break;

                case S_DONE:
                default:
                    break;
                }

                p++;
            }
        }
    }

done:
    fclose(f);

    if (state == S_DONE && id_len > 0) {
        printf("%s\n", id_val);
        return 0;
    }

    fprintf(stderr, "id non trovato nel file\n");
    return 1;
}
