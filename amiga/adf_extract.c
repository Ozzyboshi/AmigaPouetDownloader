/*
 * adf_extract.c — Estrae il contenuto di un file ADF (Amiga Disk File)
 *
 * Supporta: OFS (Original File System) e FFS (Fast File System)
 * Dipendenze: solo libreria C standard
 *
 * Compilazione:  gcc -O2 -Wall -o adf_extract adf_extract.c
 * Uso:           ./adf_extract <file.adf> <cartella_destinazione>
 *
 * -----------------------------------------------------------------------
 * Layout blocco file header (512 byte = 128 longword):
 *   lw[0]       type       = T_SHORT (2)
 *   lw[1]       header_key = numero del blocco stesso
 *   lw[2]       high_seq   = n. data block ptr nel file header (max 72)
 *   lw[4]       first_data = primo data block della catena
 *   lw[6..77]   data block pointers (72 slot), in ordine INVERSO:
 *               lw[77] (off 308, indice 71) = primo blocco
 *               lw[77-high_seq+1]           = ultimo blocco nel header
 *   lw[81]      byte_size = dimensione file in byte  (off 324)
 *   off 432     nome BCPL: primo byte=lunghezza, poi i caratteri
 *   lw[-4]      (off 496) prossima entry nella hash chain
 *   lw[-3]      (off 500) parent block
 *   lw[-1]      (off 508) subtype: ST_FILE=-3, ST_USERDIR=2, ST_ROOT=1
 *
 * -----------------------------------------------------------------------
 * OFS data block (type=8):
 *   lw[0]  type       = T_DATA (8)
 *   lw[1]  header_key = blocco header del file
 *   lw[2]  seq_num    = numero sequenza (1-based)
 *   lw[3]  data_size  = n. byte payload validi in questo blocco
 *   lw[4]  next       = prossimo data block (0 se ultimo)
 *   payload da offset 24, per data_size byte
 *
 * Per OFS, l'estrattore segue la catena via lw[4].next a partire da
 * lw[4].first_data dell'header, ignorando la tabella nel file header
 * (più semplice e corretto anche per file > 72 blocchi).
 *
 * FFS data block: tutto il settore è payload (no header interno).
 * Per FFS si usano i puntatori nel file header + extension block.
 *
 * -----------------------------------------------------------------------
 * FFS extension block (type=T_LIST=16, subtype=ST_FILE=-3):
 *   stessa struttura dei data block pointer (lw[6..77], high_seq a lw[2])
 *   lw[-4] (off 496) = prossimo extension block (0 se ultimo)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/stat.h>

#define SECTOR_SIZE     512
#define NUM_SECTORS     1760
#define ADF_SIZE        (NUM_SECTORS * SECTOR_SIZE)
#define ROOT_BLOCK      880
#define HASHTABLE_SIZE  72
#define MAX_DATA_BLKPTR 72
#define MAX_FILE_BLOCKS 8192   /* protezione loop infinito */

#define T_SHORT     2
#define T_DATA      8
#define T_LIST      16
#define ST_ROOT     1
#define ST_USERDIR  2
#define ST_FILE    -3

#define FSFLAG_FFS  0x01

/* Offset */
#define OFF_TYPE        0
#define OFF_HIGH_SEQ    8
#define OFF_FIRST_DATA  16
#define OFF_DATABLK_0   24    /* lw[6]: inizio array 72 ptr              */
#define OFF_BYTE_SIZE   324   /* lw[81]: byte_size                       */
#define OFF_NAME        432   /* BCPL: lunghezza + caratteri             */
#define OFF_HASH_CHAIN  496   /* lw[-4]: hash chain / extension          */
#define OFF_PARENT      500   /* lw[-3]: parent block                    */
#define OFF_SUBTYPE     508   /* lw[-1]: subtype                         */

/* OFS data block */
#define OFS_OFF_SEQ     8    /* seq_num  */
#define OFS_OFF_DATASIZ 12   /* data_size (byte validi)                  */
#define OFS_OFF_NEXT    16   /* next data block                          */
#define OFS_HEADER_SZ   24   /* dimensione header in ogni data block OFS */

typedef struct { uint8_t data[SECTOR_SIZE]; } Sector;
typedef struct { Sector *sectors; int ffs; } ADF;

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

static const uint8_t *get_sector(const ADF *adf, uint32_t blk)
{
    if (blk == 0 || blk >= NUM_SECTORS) return NULL;
    return adf->sectors[blk].data;
}

static int mkdirp(const char *path)
{
    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (!len) return 0;
    if (tmp[len-1]=='/') tmp[--len]='\0';
    for (size_t i=1; i<=len; i++) {
        if (tmp[i]=='/' || tmp[i]=='\0') {
            char sv=tmp[i]; tmp[i]='\0';
            if (mkdir(tmp,0755)!=0 && errno!=EEXIST) { perror(tmp); return -1; }
            tmp[i]=sv;
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
 * Estrazione OFS: segue la catena next nei data block
 * Partenza da first_data (lw[4] dell'header).
 * --------------------------------------------------------------------- */
static int extract_file_ofs(const ADF *adf, uint32_t hdr_blk,
                             uint32_t file_size, FILE *out)
{
    const uint8_t *hdr = get_sector(adf, hdr_blk);
    if (!hdr) return -1;

    uint32_t cur_blk = be32(hdr, OFF_FIRST_DATA);
    uint32_t bytes_written = 0;
    int count = 0;

    while (cur_blk != 0 && bytes_written < file_size &&
           count < MAX_FILE_BLOCKS) {
        count++;
        const uint8_t *db = get_sector(adf, cur_blk);
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
 * Estrazione FFS: usa tabella puntatori nell'header + extension block.
 * I 72 ptr sono a lw[6..77] in ordine INVERSO:
 *   indice 71 (lw[77], off 308) = primo blocco del gruppo
 *   indice 72-high_seq          = ultimo blocco del gruppo
 * --------------------------------------------------------------------- */
static int extract_file_ffs(const ADF *adf, uint32_t hdr_blk,
                             uint32_t file_size, FILE *out)
{
    const uint8_t *cur = get_sector(adf, hdr_blk);
    if (!cur) return -1;

    uint32_t bytes_written = 0;

    for (;;) {
        uint32_t high_seq = be32(cur, OFF_HIGH_SEQ);
        if (high_seq > MAX_DATA_BLKPTR) high_seq = MAX_DATA_BLKPTR;

        /* Iterazione dall'indice 71 scendendo per high_seq passi */
        for (int i = 71; i >= (int)(72 - high_seq); i--) {
            uint32_t db_num = be32(cur, OFF_DATABLK_0 + i * 4);
            if (db_num == 0) continue;

            const uint8_t *db = get_sector(adf, db_num);
            if (!db) continue;

            uint32_t chunk = SECTOR_SIZE;
            uint32_t remaining = file_size - bytes_written;
            if (chunk > remaining) chunk = remaining;

            if (chunk > 0) {
                fwrite(db, 1, chunk, out);
                bytes_written += chunk;
            }

            if (bytes_written >= file_size) goto ffs_done;
        }

        /* Extension block */
        uint32_t next_ext = be32(cur, OFF_HASH_CHAIN);
        if (next_ext == 0) break;
        cur = get_sector(adf, next_ext);
        if (!cur) break;
    }

ffs_done:
    return (int)bytes_written;
}

static int extract_file(const ADF *adf, uint32_t hdr_blk,
                        const char *dest_path)
{
    const uint8_t *hdr = get_sector(adf, hdr_blk);
    if (!hdr) return -1;

    if (be32s(hdr, OFF_TYPE) != T_SHORT || be32s(hdr, OFF_SUBTYPE) != ST_FILE)
        return -1;

    uint32_t file_size = be32(hdr, OFF_BYTE_SIZE);

    FILE *out = fopen(dest_path, "wb");
    if (!out) { perror(dest_path); return -1; }

    int written;
    if (adf->ffs)
        written = extract_file_ffs(adf, hdr_blk, file_size, out);
    else
        written = extract_file_ofs(adf, hdr_blk, file_size, out);

    fclose(out);

    if ((uint32_t)written != file_size)
        fprintf(stderr, "  [WARN] '%s': scritto %d/%u byte\n",
                dest_path, written, file_size);
    return 0;
}

static void scan_dir(const ADF *adf, uint32_t dir_blk, const char *dest)
{
    const uint8_t *blk = get_sector(adf, dir_blk);
    if (!blk) return;

    for (int slot = 0; slot < HASHTABLE_SIZE; slot++) {
        uint32_t entry = be32(blk, OFF_DATABLK_0 + slot * 4);

        while (entry != 0) {
            const uint8_t *eb = get_sector(adf, entry);
            if (!eb) break;

            int32_t type    = be32s(eb, OFF_TYPE);
            int32_t subtype = be32s(eb, OFF_SUBTYPE);

            char name[64];
            read_name(eb, name, sizeof(name));
            for (char *p = name; *p; p++)
                if (*p=='/'||*p=='\\'||*p==':') *p='_';

            if (name[0] == '\0') {
                entry = be32(eb, OFF_HASH_CHAIN);
                continue;
            }

            char path[4096];
            snprintf(path, sizeof(path), "%s/%s", dest, name);

            if (type == T_SHORT && subtype == ST_FILE) {
                printf("  FILE  %s\n", path);
                extract_file(adf, entry, path);
            } else if (type == T_SHORT &&
                       (subtype == ST_USERDIR || subtype == ST_ROOT)) {
                printf("  DIR   %s/\n", path);
                if (mkdirp(path) == 0)
                    scan_dir(adf, entry, path);
            }

            entry = be32(eb, OFF_HASH_CHAIN);
        }
    }
}

static int check_bootblock(const ADF *adf)
{
    const uint8_t *bb = adf->sectors[0].data;
    if (bb[0]!='D'||bb[1]!='O'||bb[2]!='S') {
        fprintf(stderr, "Boot block: firma non DOS (%02X %02X %02X)\n",
                bb[0], bb[1], bb[2]);
        return -1;
    }
    uint8_t flags = bb[3];
    printf("Boot block: DOS flags=0x%02X — Filesystem: %s\n",
           flags, (flags & FSFLAG_FFS) ? "FFS" : "OFS");
    return (flags & FSFLAG_FFS) ? 1 : 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <file.adf> <cartella_destinazione>\n", argv[0]);
        return 1;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (fsize < ADF_SIZE) {
        fprintf(stderr, "File troppo piccolo (%ld byte, attesi %d)\n",
                fsize, ADF_SIZE);
        fclose(f); return 1;
    }

    ADF adf;
    adf.sectors = (Sector *)malloc(NUM_SECTORS * sizeof(Sector));
    if (!adf.sectors) { fprintf(stderr, "OOM\n"); fclose(f); return 1; }

    size_t rd = fread(adf.sectors, sizeof(Sector), NUM_SECTORS, f);
    fclose(f);
    if (rd != NUM_SECTORS) {
        fprintf(stderr, "Lettura incompleta: %zu/%d settori\n", rd, NUM_SECTORS);
        free(adf.sectors); return 1;
    }

    int ffs = check_bootblock(&adf);
    if (ffs < 0) { free(adf.sectors); return 1; }
    adf.ffs = ffs;

    if (mkdirp(argv[2]) != 0) { free(adf.sectors); return 1; }

    const uint8_t *root = get_sector(&adf, ROOT_BLOCK);
    if (!root) { free(adf.sectors); return 1; }

    char volname[64];
    read_name(root, volname, sizeof(volname));
    printf("Volume: \"%s\"\n", volname);
    printf("Estrazione in: %s\n\n", argv[2]);

    scan_dir(&adf, ROOT_BLOCK, argv[2]);

    printf("\nFatto.\n");
    free(adf.sectors);
    return 0;
}
