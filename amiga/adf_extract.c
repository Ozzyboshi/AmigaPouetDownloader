/*
 * adf_extract.c — Estrae il contenuto di un file ADF (Amiga Disk File)
 *
 * Supporta: OFS e FFS
 * RAM: tutto statico (BSS/data), zero malloc, zero ricorsione.
 *
 * Compilazione:  gcc -O2 -Wall -o adf_extract adf_extract.c
 * Uso:           ./adf_extract <file.adf> <cartella_destinazione>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#define SECTOR_SIZE      512
#define NUM_SECTORS      1760
#define ADF_SIZE         (NUM_SECTORS * SECTOR_SIZE)
#define ROOT_BLOCK       880
#define HASHTABLE_SIZE   72
#define MAX_DATA_BLKPTR  72
#define MAX_FILE_BLOCKS  8192

#define NUM_CACHED       8      /* 8 x 512 = 4 KB */
#define DIR_STACK_MAX    32     /* max livelli directory annidati */
#define PATH_MAX_LEN     256

#define T_SHORT     2
#define T_DATA      8
#define T_LIST      16
#define ST_ROOT     1
#define ST_USERDIR  2
#define ST_FILE    -3

#define FSFLAG_FFS  0x01

#define OFF_TYPE        0
#define OFF_HIGH_SEQ    8
#define OFF_FIRST_DATA  16
#define OFF_DATABLK_0   24
#define OFF_BYTE_SIZE   324
#define OFF_NAME        432
#define OFF_HASH_CHAIN  496
#define OFF_PARENT      500
#define OFF_SUBTYPE     508

#define OFS_OFF_DATASIZ 12
#define OFS_OFF_NEXT    16
#define OFS_HEADER_SZ   24

/* -----------------------------------------------------------------------
 * Strutture statiche globali — vivono nel BSS, non costano stack né heap
 * --------------------------------------------------------------------- */
typedef struct {
    uint8_t  data[SECTOR_SIZE];
    uint32_t blk_num;
    uint32_t last_use;
    int      valid;
} CacheSlot;

typedef struct {
    uint32_t dir_blk;
    char     dest[PATH_MAX_LEN];
} DirEntry;

/* ADF state */
static FILE      *g_fp;
static int        g_ffs;
static uint32_t   g_clock;
static CacheSlot  g_cache[NUM_CACHED];

/* Stack directory iterativo */
static DirEntry   g_dirstack[DIR_STACK_MAX];

/* Buffer di lavoro riutilizzati — mai sullo stack delle funzioni */
static uint8_t    g_local_dir[SECTOR_SIZE];   /* copia dir/ext corrente  */
static uint8_t    g_local_blk[SECTOR_SIZE];   /* copia header/ext FFS    */
static char       g_path[PATH_MAX_LEN];        /* path corrente           */
static char       g_name[64];                  /* nome entry corrente     */
static char       g_volname[64];               /* nome volume             */

/* -----------------------------------------------------------------------
 * Utilità
 * --------------------------------------------------------------------- */
static inline uint32_t be32(const uint8_t *p, int off)
{
    const uint8_t *b = p + off;
    return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|
           ((uint32_t)b[2]<< 8)| (uint32_t)b[3];
}

static inline int32_t be32s(const uint8_t *p, int off)
{
    return (int32_t)be32(p, off);
}

static const uint8_t *get_sector(uint32_t blk)
{
    if (blk == 0 || blk >= NUM_SECTORS) return NULL;

    g_clock++;

    for (int i = 0; i < NUM_CACHED; i++) {
        if (g_cache[i].valid && g_cache[i].blk_num == blk) {
            g_cache[i].last_use = g_clock;
            return g_cache[i].data;
        }
    }

    int victim = 0;
    for (int i = 1; i < NUM_CACHED; i++) {
        if (!g_cache[i].valid) { victim = i; break; }
        if (g_cache[i].last_use < g_cache[victim].last_use)
            victim = i;
    }

    if (fseek(g_fp, (long)blk * SECTOR_SIZE, SEEK_SET) != 0) return NULL;
    if (fread(g_cache[victim].data, 1, SECTOR_SIZE, g_fp) != SECTOR_SIZE)
        return NULL;

    g_cache[victim].blk_num  = blk;
    g_cache[victim].last_use = g_clock;
    g_cache[victim].valid    = 1;
    return g_cache[victim].data;
}

static int mkdirp(const char *path)
{
    /* buffer locale — piccolo e usato solo qui, va bene sullo stack */
    char tmp[PATH_MAX_LEN];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (!len) return 0;
    if (tmp[len-1]=='/') tmp[--len]='\0';
    for (size_t i = 1; i <= len; i++) {
        if (tmp[i]=='/' || tmp[i]=='\0') {
            char sv = tmp[i]; tmp[i] = '\0';
            if (mkdir(tmp, 0755) != 0 && errno != EEXIST)
                { perror(tmp); return -1; }
            tmp[i] = sv;
        }
    }
    return 0;
}

static void read_name(const uint8_t *blk, char *out, size_t outsz)
{
    uint8_t n = blk[OFF_NAME];
    if (n > 30) n = 30;
    size_t cp = (n < outsz-1) ? n : outsz-1;
    memcpy(out, blk+OFF_NAME+1, cp);
    out[cp] = '\0';
}

/* -----------------------------------------------------------------------
 * OFS
 * --------------------------------------------------------------------- */
static int extract_file_ofs(uint32_t hdr_blk, uint32_t file_size, FILE *out)
{
    const uint8_t *hdr = get_sector(hdr_blk);
    if (!hdr) return -1;

    uint32_t cur_blk       = be32(hdr, OFF_FIRST_DATA);
    uint32_t bytes_written = 0;
    int      count         = 0;

    while (cur_blk != 0 && bytes_written < file_size &&
           count < MAX_FILE_BLOCKS) {
        count++;
        const uint8_t *db = get_sector(cur_blk);
        if (!db) break;

        uint32_t dsz = be32(db, OFS_OFF_DATASIZ);
        if (dsz > SECTOR_SIZE - OFS_HEADER_SZ)
            dsz = SECTOR_SIZE - OFS_HEADER_SZ;
        uint32_t remaining = file_size - bytes_written;
        if (dsz > remaining) dsz = remaining;

        if (dsz > 0) {
            fwrite(db + OFS_HEADER_SZ, 1, dsz, out);
            bytes_written += dsz;
        }
        cur_blk = be32(db, OFS_OFF_NEXT);
    }
    return (int)bytes_written;
}

/* -----------------------------------------------------------------------
 * FFS — usa g_local_blk per tenere copia dell'header/extension corrente
 * --------------------------------------------------------------------- */
static int extract_file_ffs(uint32_t hdr_blk, uint32_t file_size, FILE *out)
{
    const uint8_t *tmp = get_sector(hdr_blk);
    if (!tmp) return -1;
    memcpy(g_local_blk, tmp, SECTOR_SIZE);

    uint32_t bytes_written = 0;

    for (;;) {
        uint32_t high_seq = be32(g_local_blk, OFF_HIGH_SEQ);
        if (high_seq > MAX_DATA_BLKPTR) high_seq = MAX_DATA_BLKPTR;

        for (int i = 71; i >= (int)(72 - high_seq); i--) {
            uint32_t db_num = be32(g_local_blk, OFF_DATABLK_0 + i * 4);
            if (db_num == 0) continue;

            const uint8_t *db = get_sector(db_num);
            if (!db) continue;

            uint32_t chunk     = SECTOR_SIZE;
            uint32_t remaining = file_size - bytes_written;
            if (chunk > remaining) chunk = remaining;

            if (chunk > 0) {
                fwrite(db, 1, chunk, out);
                bytes_written += chunk;
            }
            if (bytes_written >= file_size) goto ffs_done;
        }

        uint32_t next_ext = be32(g_local_blk, OFF_HASH_CHAIN);
        if (next_ext == 0) break;
        const uint8_t *nb = get_sector(next_ext);
        if (!nb) break;
        memcpy(g_local_blk, nb, SECTOR_SIZE);
    }

ffs_done:
    return (int)bytes_written;
}

/* -----------------------------------------------------------------------
 * Estrazione singolo file
 * --------------------------------------------------------------------- */
static int extract_file(uint32_t hdr_blk, const char *dest_path)
{
    const uint8_t *hdr = get_sector(hdr_blk);
    if (!hdr) return -1;

    if (be32s(hdr, OFF_TYPE) != T_SHORT || be32s(hdr, OFF_SUBTYPE) != ST_FILE)
        return -1;

    uint32_t file_size = be32(hdr, OFF_BYTE_SIZE);

    FILE *out = fopen(dest_path, "wb");
    if (!out) { perror(dest_path); return -1; }

    int written;
    if (g_ffs)
        written = extract_file_ffs(hdr_blk, file_size, out);
    else
        written = extract_file_ofs(hdr_blk, file_size, out);

    fclose(out);

    if ((uint32_t)written != file_size)
        fprintf(stderr, "  [WARN] '%s': scritto %d/%u byte\n",
                dest_path, written, file_size);
    return 0;
}

/* -----------------------------------------------------------------------
 * Visita directory — iterativa, usa g_dirstack e g_local_dir statici
 * --------------------------------------------------------------------- */
static void scan_dir(uint32_t start_blk, const char *start_dest)
{
    int top = 0;
    g_dirstack[0].dir_blk = start_blk;
    snprintf(g_dirstack[0].dest, PATH_MAX_LEN, "%s", start_dest);
    top = 1;

    while (top > 0) {
        DirEntry cur = g_dirstack[--top];

        const uint8_t *blk = get_sector(cur.dir_blk);
        if (!blk) continue;
        memcpy(g_local_dir, blk, SECTOR_SIZE);

        for (int slot = 0; slot < HASHTABLE_SIZE; slot++) {
            uint32_t entry = be32(g_local_dir, OFF_DATABLK_0 + slot * 4);

            while (entry != 0) {
                const uint8_t *eb = get_sector(entry);
                if (!eb) break;

                int32_t type    = be32s(eb, OFF_TYPE);
                int32_t subtype = be32s(eb, OFF_SUBTYPE);

                read_name(eb, g_name, sizeof(g_name));
                for (char *p = g_name; *p; p++)
                    if (*p=='/'||*p=='\\'||*p==':') *p='_';

                uint32_t next_entry = be32(eb, OFF_HASH_CHAIN);

                if (g_name[0] != '\0') {
                    snprintf(g_path, PATH_MAX_LEN, "%s/%s",
                             cur.dest, g_name);

                    if (type == T_SHORT && subtype == ST_FILE) {
                        printf("  FILE  %s\n", g_path);
                        extract_file(entry, g_path);
                    } else if (type == T_SHORT &&
                               (subtype == ST_USERDIR || subtype == ST_ROOT)) {
                        printf("  DIR   %s/\n", g_path);
                        if (mkdirp(g_path) == 0) {
                            if (top < DIR_STACK_MAX) {
                                g_dirstack[top].dir_blk = entry;
                                snprintf(g_dirstack[top].dest,
                                         PATH_MAX_LEN, "%s", g_path);
                                top++;
                            } else {
                                fprintf(stderr,
                                    "[WARN] annidamento eccessivo: %s\n",
                                    g_path);
                            }
                        }
                    }
                }

                entry = next_entry;
            }
        }
    }
}

/* -----------------------------------------------------------------------
 * Boot block
 * --------------------------------------------------------------------- */
static int check_bootblock(void)
{
    uint8_t buf[4];
    fseek(g_fp, 0, SEEK_SET);
    if (fread(buf, 1, 4, g_fp) != 4) return -1;

    if (buf[0]!='D'||buf[1]!='O'||buf[2]!='S') {
        fprintf(stderr, "Boot block: firma non DOS (%02X %02X %02X)\n",
                buf[0], buf[1], buf[2]);
        return -1;
    }
    uint8_t flags = buf[3];
    printf("Boot block: DOS flags=0x%02X - Filesystem: %s\n",
           flags, (flags & FSFLAG_FFS) ? "FFS" : "OFS");
    return (flags & FSFLAG_FFS) ? 1 : 0;
}

/* -----------------------------------------------------------------------
 * main
 * --------------------------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <file.adf> <cartella_destinazione>\n",
                argv[0]);
        return 1;
    }

    g_fp = fopen(argv[1], "rb");
    if (!g_fp) { perror(argv[1]); return 1; }

    fseek(g_fp, 0, SEEK_END);
    long fsize = ftell(g_fp);
    if (fsize < ADF_SIZE) {
        fprintf(stderr, "File troppo piccolo (%ld byte, attesi %d)\n",
                fsize, ADF_SIZE);
        fclose(g_fp); return 1;
    }

    int ffs = check_bootblock();
    if (ffs < 0) { fclose(g_fp); return 1; }
    g_ffs = ffs;

    if (mkdirp(argv[2]) != 0) { fclose(g_fp); return 1; }

    const uint8_t *root = get_sector(ROOT_BLOCK);
    if (!root) { fclose(g_fp); return 1; }

    read_name(root, g_volname, sizeof(g_volname));
    printf("Volume: \"%s\"\n", g_volname);
    printf("Estrazione in: %s\n\n", argv[2]);

    scan_dir(ROOT_BLOCK, argv[2]);

    printf("\nFatto.\n");
    fclose(g_fp);
    return 0;
}