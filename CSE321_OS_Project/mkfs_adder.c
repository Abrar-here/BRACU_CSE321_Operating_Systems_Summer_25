// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t inode_count;
    uint64_t inode_bitmap_start;
    uint64_t inode_bitmap_blocks;
    uint64_t data_bitmap_start;
    uint64_t data_bitmap_blocks;
    uint64_t inode_table_start;
    uint64_t inode_table_blocks;
    uint64_t data_region_start;
    uint64_t data_region_blocks;
    uint64_t root_inode;
    uint64_t mtime_epoch;
    uint32_t flags;
    uint32_t checksum;
} superblock_t;
#pragma pack(pop)
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

#pragma pack(push,1)
typedef struct {
    uint16_t mode;
    uint16_t links;
    uint32_t uid;
    uint32_t gid;
    uint64_t size_bytes;
    uint64_t atime;
    uint64_t mtime;
    uint64_t ctime;
    uint32_t direct[DIRECT_MAX];
    uint32_t reserved_0;
    uint32_t reserved_1;
    uint32_t reserved_2;
    uint32_t proj_id;
    uint32_t uid16_gid16;
    uint64_t xattr_ptr;
    uint64_t inode_crc;
} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t inode_no;
    uint8_t  type;
    char     name[58];
    uint8_t  checksum;
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");

static uint32_t CRC32_TAB[256];
static int crc_inited = 0;
static void crc32_init(void){
    if (crc_inited) return;
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
    crc_inited = 1;
}
static uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
static void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE];
    memcpy(tmp, ino, INODE_SIZE);
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c;
}
static void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];
    de->checksum = x;
}
static int write_superblock_block(FILE* fp, const superblock_t* in){
    uint8_t blk[BS];
    memset(blk, 0, sizeof(blk));
    size_t copy = sizeof(*in);
    if (copy > BS-4) copy = BS-4;
    memcpy(blk, in, copy);
    uint32_t c = crc32(blk, BS-4);
    blk[BS-4] = (uint8_t)( c        & 0xFF);
    blk[BS-3] = (uint8_t)((c >> 8 ) & 0xFF);
    blk[BS-2] = (uint8_t)((c >> 16) & 0xFF);
    blk[BS-1] = (uint8_t)((c >> 24) & 0xFF);
    if (fseek(fp, 0, SEEK_SET)!=0) return -1;
    if (fwrite(blk, 1, BS, fp) != BS) return -1;
    return 0;
}
static int read_superblock(FILE* fp, superblock_t* out_sb){
    uint8_t blk[BS];
    if (fseek(fp, 0, SEEK_SET)!=0) return -1;
    if (fread(blk, 1, BS, fp) != BS) return -1;
    uint32_t got = blk[BS-4] | (blk[BS-3]<<8) | (blk[BS-2]<<16) | (blk[BS-1]<<24);
    uint32_t calc = crc32(blk, BS-4);
    if (got != calc){
        fprintf(stderr, "superblock checksum mismatch (got %08x, calc %08x)\n", got, calc);
        return -1;
    }
    memset(out_sb, 0, sizeof(*out_sb));
    size_t copy = sizeof(*out_sb);
    if (copy > BS-4) copy = BS-4;
    memcpy(out_sb, blk, copy);
    return 0;
}
static inline void bset(uint8_t *bm, uint64_t idx) { bm[idx >> 3] |=  (1u << (idx & 7)); }
static inline void bclr(uint8_t *bm, uint64_t idx) { bm[idx >> 3] &= ~(1u << (idx & 7)); }
static inline int  bget(const uint8_t *bm, uint64_t idx){ return (bm[idx >> 3] >> (idx & 7)) & 1; }
static void usage(const char* prog){
    fprintf(stderr, "Usage: %s --input in.img --output out.img --file path [--name filename]\n", prog);
}
static int mul_overflow_u64(uint64_t a, uint64_t b, uint64_t *out){
    if (a == 0 || b == 0){ *out = 0; return 0; }
    if (a > UINT64_MAX / b) return 1;
    *out = a * b; return 0;
}
static const char* basename_c(const char* p){
    const char* s = strrchr(p, '/');
#ifdef _WIN32
    const char* s2 = strrchr(p, '\\');
    if (!s || (s2 && s2> s)) s = s2;
#endif
    return s ? (s+1) : p;
}

int main(int argc, char** argv){
    crc32_init();
    const char* in_img = NULL;
    const char* out_img = NULL;
    const char* host_file = NULL;
    const char* name_opt = NULL;
    if (argc < 7){
        usage(argv[0]);
        return 1;
    }
    for (int i=1;i<argc;i++){
        if (strcmp(argv[i],"--input")==0 && i+1<argc) in_img = argv[++i];
        else if (strcmp(argv[i],"--output")==0 && i+1<argc) out_img = argv[++i];
        else if (strcmp(argv[i],"--file")==0 && i+1<argc) host_file = argv[++i];
        else if (strcmp(argv[i],"--name")==0 && i+1<argc) name_opt = argv[++i];
        else { usage(argv[0]); return 1; }
    }
    if (!in_img || !out_img || !host_file){
        usage(argv[0]); return 1;
    }
    if (strcmp(in_img, out_img) == 0){
        fprintf(stderr,"Error: input and output image must be different files.\n");
        return 1;
    }
    FILE* hf = fopen(host_file, "rb");
    if (!hf){ perror("open host file"); return 1; }
    if (fseek(hf, 0, SEEK_END)!=0){ perror("fseek host"); fclose(hf); return 1; }
    long fsz_l = ftell(hf);
    if (fsz_l < 0){ perror("ftell host"); fclose(hf); return 1; }
    uint64_t fsz = (uint64_t)fsz_l;
    if (fseek(hf, 0, SEEK_SET)!=0){ perror("rewind host"); fclose(hf); return 1; }
    uint8_t *file_buf = malloc((size_t)fsz + 1);
    if (!file_buf){ fprintf(stderr,"OOM reading host file\n"); fclose(hf); return 1; }
    size_t got_r = fread(file_buf, 1, (size_t)fsz, hf);
    if (got_r != (size_t)fsz){ fprintf(stderr,"read host file failed\n"); free(file_buf); fclose(hf); return 1; }
    file_buf[fsz] = 0;
    uint64_t need_blocks = (fsz + (BS-1)) / BS;
    if (need_blocks == 0) need_blocks = 1;
    if (need_blocks > DIRECT_MAX){
        fprintf(stderr, "Error: file needs %" PRIu64 " blocks (> %d direct)\n", need_blocks, DIRECT_MAX);
        free(file_buf); fclose(hf); return 1;
    }
    FILE* fi = fopen(in_img, "rb");
    if (!fi){ perror("open input img"); free(file_buf); fclose(hf); return 1; }
    superblock_t sb;
    if (read_superblock(fi, &sb) != 0){
        fprintf(stderr, "Invalid or corrupt superblock.\n");
        fclose(fi); free(file_buf); fclose(hf); return 1;
    }
    if (sb.magic != 0x4D565346u || sb.block_size != BS){
        fprintf(stderr, "Invalid magic or block size.\n");
        fclose(fi); free(file_buf); fclose(hf); return 1;
    }
    if (sb.inode_bitmap_blocks != 1 || sb.data_bitmap_blocks != 1){
        fprintf(stderr, "Unexpected bitmap sizes.\n");
        fclose(fi); free(file_buf); fclose(hf); return 1;
    }
    FILE* fo = fopen(out_img, "wb+");
    if (!fo){ perror("open output img"); fclose(fi); free(file_buf); fclose(hf); return 1; }
    if (fseek(fi, 0, SEEK_SET)!=0){ perror("seek in"); fclose(fi); fclose(fo); free(file_buf); fclose(hf); return 1; }
    static uint8_t buf[1<<16];
    for (;;){
        size_t r = fread(buf, 1, sizeof(buf), fi);
        if (r == 0) break;
        if (fwrite(buf, 1, r, fo) != r){ fprintf(stderr,"copy failed\n"); fclose(fi); fclose(fo); free(file_buf); fclose(hf); return 1; }
    }
    uint8_t inode_bm[BS], data_bm[BS];
    if (fseek(fo, sb.inode_bitmap_start * BS, SEEK_SET) != 0){ perror("seek fo inode bm"); goto fail; }
    if (fread(inode_bm, 1, BS, fo) != BS){ fprintf(stderr,"read inode bm\n"); goto fail; }
    if (fseek(fo, sb.data_bitmap_start * BS, SEEK_SET) != 0){ perror("seek fo data bm"); goto fail; }
    if (fread(data_bm, 1, BS, fo) != BS){ fprintf(stderr,"read data bm\n"); goto fail; }
    uint64_t new_ino_idx = (uint64_t)-1;
    for (uint64_t i = 1; i < sb.inode_count; i++){
        if (!bget(inode_bm, i)){ new_ino_idx = i; break; }
    }
    if (new_ino_idx == (uint64_t)-1){
        fprintf(stderr, "No free inode available.\n"); goto fail;
    }
    uint32_t new_ino_no = (uint32_t)(new_ino_idx + 1);
    uint64_t free_blocks = 0;
    for (uint64_t i=0;i<sb.data_region_blocks;i++) if (!bget(data_bm, i)) free_blocks++;
    if (need_blocks > free_blocks){
        fprintf(stderr, "Not enough free data blocks (need %" PRIu64 ", have %" PRIu64 ")\n", need_blocks, free_blocks);
        goto fail;
    }
    uint64_t chosen[DIRECT_MAX]; memset(chosen, 0, sizeof(chosen));
    uint64_t got = 0;
    for (uint64_t i=0;i<sb.data_region_blocks && got<need_blocks; i++){
        if (!bget(data_bm, i)){
            chosen[got++] = i;
            bset(data_bm, i);
        }
    }
    if (got < need_blocks){
        fprintf(stderr, "Unexpected: insufficient blocks after allocation loop\n"); goto fail;
    }
    uint64_t it_off = sb.inode_table_start * BS;
    uint64_t it_bytes = sb.inode_table_blocks * BS;
    uint8_t* it = (uint8_t*) malloc(it_bytes);
    if (!it){ fprintf(stderr,"OOM inode table\n"); goto fail; }
    if (fseek(fo, it_off, SEEK_SET) != 0){ perror("seek it"); free(it); goto fail; }
    if (fread(it, 1, it_bytes, fo) != it_bytes){ fprintf(stderr,"read inode table\n"); free(it); goto fail; }
    inode_t* root = (inode_t*) it;
    if (root->direct[0] == 0){
        fprintf(stderr,"Root has no data block\n"); free(it); goto fail;
    }
    uint64_t entries_used = root->size_bytes / 64;
    if (entries_used >= (BS / 64)){
        fprintf(stderr,"Root directory is full.\n"); free(it); goto fail;
    }
    inode_t* newi = (inode_t*)(it + (new_ino_no - 1) * INODE_SIZE);
    memset(newi, 0, sizeof(*newi));
    newi->mode = 0100000;
    newi->links = 1;
    newi->uid = 0; newi->gid = 0;
    uint64_t now = (uint64_t)time(NULL);
    newi->atime = newi->mtime = newi->ctime = now;
    newi->size_bytes = fsz;
    for (int i=0;i<DIRECT_MAX;i++) newi->direct[i] = 0;
    for (uint64_t i=0;i<need_blocks;i++){
        uint64_t rel = chosen[i];
        uint64_t absb = sb.data_region_start + rel;
        if (absb > UINT32_MAX){ fprintf(stderr,"data block index too large\n"); free(it); goto fail; }
        newi->direct[i] = (uint32_t)absb;
    }
    inode_crc_finalize(newi);
    for (uint64_t i=0;i<need_blocks;i++){
        uint64_t absb = newi->direct[i];
        if (fseek(fo, absb * BS, SEEK_SET) != 0){ perror("seek data block"); free(it); goto fail; }
        uint8_t block[BS];
        memset(block, 0, BS);
        size_t toread = BS;
        if ((i+1)*BS > fsz){
            uint64_t start = i * BS;
            uint64_t remain = fsz - start;
            toread = (size_t)remain;
        }
        memcpy(block, file_buf + i * BS, toread);
        if (fwrite(block, 1, BS, fo) != BS){ fprintf(stderr,"write data block\n"); free(it); goto fail; }
    }
    uint32_t root_block = root->direct[0];
    if (root_block == 0){ fprintf(stderr,"root has no data block\n"); free(it); goto fail; }
    uint8_t dirblk[BS];
    if (fseek(fo, root_block * BS, SEEK_SET) != 0){ perror("seek root blk"); free(it); goto fail; }
    if (fread(dirblk, 1, BS, fo) != BS){ fprintf(stderr,"read root blk\n"); free(it); goto fail; }
    dirent64_t* de = (dirent64_t*)dirblk;
    dirent64_t* dst = de + entries_used;
    memset(dst, 0, sizeof(*dst));
    dst->inode_no = new_ino_no;
    dst->type = 1;
    const char* bn = name_opt ? name_opt : basename_c(host_file);
    strncpy(dst->name, bn, sizeof(dst->name)-1);
    dirent_checksum_finalize(dst);
    if (fseek(fo, root_block * BS, SEEK_SET) != 0){ perror("seek root blk 2"); free(it); goto fail; }
    if (fwrite(dirblk, 1, BS, fo) != BS){ fprintf(stderr,"write root blk\n"); free(it); goto fail; }
    root->size_bytes += 64;
    inode_crc_finalize(root);
    if (fseek(fo, it_off, SEEK_SET) != 0){ perror("seek it write"); free(it); goto fail; }
    if (fwrite(it, 1, it_bytes, fo) != it_bytes){ fprintf(stderr,"write inode table\n"); free(it); goto fail; }
    free(it);
    bset(inode_bm, new_ino_idx);
    if (fseek(fo, sb.inode_bitmap_start * BS, SEEK_SET) != 0){ perror("seek ibm write"); goto fail; }
    if (fwrite(inode_bm, 1, BS, fo) != BS){ fprintf(stderr,"write inode bm\n"); goto fail; }
    if (fseek(fo, sb.data_bitmap_start * BS, SEEK_SET) != 0){ perror("seek dbm write"); goto fail; }
    if (fwrite(data_bm, 1, BS, fo) != BS){ fprintf(stderr,"write data bm\n"); goto fail; }
    sb.mtime_epoch = (uint64_t)time(NULL);
    if (write_superblock_block(fo, &sb) != 0){
        fprintf(stderr, "write superblock failed\n"); goto fail;
    }
    fclose(hf);
    fclose(fi);
    fclose(fo);
    fprintf(stderr, "mkfs_adder: added '%s' (inode #%u, %" PRIu64 " bytes, %" PRIu64 " block(s))\n",
            bn, new_ino_no, fsz, need_blocks);
    if (fsz > 0){
        if (fwrite(file_buf, 1, (size_t)fsz, stdout) != (size_t)fsz){
        }
        if (file_buf[fsz-1] != '\n') putchar('\n');
    } else {
        printf("(empty file)\n");
    }
    free(file_buf);
    return 0;
fail:
    if (fi) fclose(fi);
    if (fo) fclose(fo);
    if (hf) fclose(hf);
    if (file_buf) free(file_buf);
    return 1;
}
