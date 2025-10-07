// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
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

uint64_t g_random_seed = 0;

#pragma pack(push, 1)
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
void crc32_init(void){
    if (crc_inited) return;
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
    crc_inited = 1;
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint8_t blk[BS];
    memset(blk, 0, sizeof(blk));
    size_t copy = sizeof(*sb);
    if (copy > BS-4) copy = BS-4;
    memcpy(blk, sb, copy);
    uint32_t s = crc32(blk, BS-4);
    sb->checksum = s;
    return s;
}
void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE];
    memcpy(tmp, ino, INODE_SIZE);
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c;
}
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];
    de->checksum = x;
}
static inline void bset(uint8_t *bm, uint64_t idx) { bm[idx >> 3] |=  (1u << (idx & 7)); }
static inline void bclr(uint8_t *bm, uint64_t idx) { bm[idx >> 3] &= ~(1u << (idx & 7)); }
static inline int  bget(const uint8_t *bm, uint64_t idx){ return (bm[idx >> 3] >> (idx & 7)) & 1; }

static void write_superblock_block(FILE* fp, const superblock_t* in){
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
    if (fwrite(blk, 1, BS, fp) != BS) {
        fprintf(stderr, "write superblock failed: %s\n", strerror(errno));
        exit(1);
    }
}
static void write_zero_blocks(FILE* fp, uint64_t blocks){
    static uint8_t zero[BS];
    memset(zero, 0, sizeof(zero));
    for (uint64_t i=0;i<blocks;i++){
        if (fwrite(zero, 1, BS, fp) != BS){
            fprintf(stderr, "write zeros failed: %s\n", strerror(errno));
            exit(1);
        }
    }
}
static void usage(const char* prog){
    fprintf(stderr, "Usage: %s --image out.img --size-kib N --inodes M\n", prog);
    fprintf(stderr, "  N: total KiB (>= 180, multiple of 4, <= 4096)\n");
    fprintf(stderr, "  M: inode count (128..512)\n");
}
static int mul_overflow_u64(uint64_t a, uint64_t b, uint64_t *out){
    if (a == 0 || b == 0){ *out = 0; return 0; }
    if (a > UINT64_MAX / b) return 1;
    *out = a * b; return 0;
}

int main(int argc, char** argv){
    crc32_init();
    const char* image_path = NULL;
    long size_kib = -1;
    long inode_cnt = -1;
    if (argc < 7){
        usage(argv[0]);
        return 1;
    }
    for (int i=1; i<argc; i++){
        if (strcmp(argv[i], "--image")==0 && i+1<argc){ image_path = argv[++i]; }
        else if (strcmp(argv[i], "--size-kib")==0 && i+1<argc){
            errno = 0;
            size_kib = strtol(argv[++i], NULL, 10);
            if (errno != 0) { fprintf(stderr, "Invalid number for --size-kib\n"); return 1; }
        }
        else if (strcmp(argv[i], "--inodes")==0 && i+1<argc){
            errno = 0;
            inode_cnt = strtol(argv[++i], NULL, 10);
            if (errno != 0) { fprintf(stderr, "Invalid number for --inodes\n"); return 1; }
        }
        else { usage(argv[0]); return 1; }
    }
    if (!image_path){ fprintf(stderr, "Missing --image\n"); usage(argv[0]); return 1; }
    if (size_kib < 180 || (size_kib % 4) != 0 || size_kib > 4096){ fprintf(stderr, "Error: --size-kib must be between 180 and 4096, multiple of 4\n"); return 1; }
    if (inode_cnt < 128 || inode_cnt > 512){ fprintf(stderr, "Error: --inodes must be between 128 and 512\n"); return 1; }
    FILE* fp = fopen(image_path, "wb");
    if (!fp){
        fprintf(stderr, "Cannot create image '%s': %s\n", image_path, strerror(errno));
        return 1;
    }
    uint64_t total_blocks;
    if (mul_overflow_u64((uint64_t)size_kib, 1024u, &total_blocks)){
        fprintf(stderr, "Size too large\n"); fclose(fp); return 1;
    }
    total_blocks /= BS;
    if (total_blocks < 7){
        fprintf(stderr, "Image too small for headers/inode table.\n"); fclose(fp); return 1;
    }
    uint64_t inode_table_blocks = ((uint64_t)inode_cnt * INODE_SIZE + (BS - 1)) / BS;
    if (inode_table_blocks == 0){ fprintf(stderr,"Internal error: inode_table_blocks == 0\n"); fclose(fp); return 1; }
    superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = 0x4D565346u;
    sb.version = 1;
    sb.block_size = BS;
    sb.total_blocks = total_blocks;
    sb.inode_count = (uint64_t)inode_cnt;
    sb.inode_bitmap_start  = 1;
    sb.inode_bitmap_blocks = 1;
    sb.data_bitmap_start   = 2;
    sb.data_bitmap_blocks  = 1;
    sb.inode_table_start   = 3;
    sb.inode_table_blocks  = inode_table_blocks;
    uint64_t data_region_start = sb.inode_table_start + sb.inode_table_blocks;
    if (data_region_start + 1 > sb.total_blocks){
        fprintf(stderr, "Image too small for headers/inode table.\n");
        fclose(fp); return 1;
    }
    sb.data_region_start = data_region_start;
    sb.data_region_blocks = sb.total_blocks - sb.data_region_start;
    sb.root_inode = ROOT_INO;
    sb.mtime_epoch = (uint64_t)time(NULL);
    sb.flags = 0;
    sb.checksum = 0;
    uint8_t inode_bm[BS]; memset(inode_bm, 0, BS);
    uint8_t data_bm[BS];  memset(data_bm,  0, BS);
    bset(inode_bm, 0);
    bset(data_bm, 0);
    size_t it_bytes = (size_t)inode_table_blocks * BS;
    uint8_t* inode_table = calloc(1, it_bytes);
    if (!inode_table){
        fprintf(stderr, "OOM creating inode table (%zu bytes)\n", it_bytes);
        fclose(fp); return 1;
    }
    inode_t* ino = (inode_t*)inode_table;
    memset(ino, 0, sizeof(*ino));
    ino->mode = 0040000;
    ino->links = 2;
    ino->uid = 0; ino->gid = 0;
    uint64_t now = (uint64_t)time(NULL);
    ino->atime = ino->mtime = ino->ctime = now;
    ino->size_bytes = 128;
    for (int i=0;i<DIRECT_MAX;i++) ino->direct[i]=0;
    uint64_t abs_root_block = sb.data_region_start + 0;
    if (abs_root_block > UINT32_MAX){
        fprintf(stderr, "data region start too large\n");
        free(inode_table); fclose(fp); return 1;
    }
    ino->direct[0] = (uint32_t)abs_root_block;
    inode_crc_finalize(ino);
    uint8_t* rootblk = calloc(1, BS);
    if (!rootblk){
        fprintf(stderr, "OOM preparing root block\n");
        free(inode_table); fclose(fp); return 1;
    }
    dirent64_t* de = (dirent64_t*)rootblk;
    memset(de, 0, sizeof(*de));
    de->inode_no = ROOT_INO;
    de->type = 2;
    strncpy(de->name, ".", sizeof(de->name)-1);
    dirent_checksum_finalize(de);
    de++;
    memset(de, 0, sizeof(*de));
    de->inode_no = ROOT_INO;
    de->type = 2;
    strncpy(de->name, "..", sizeof(de->name)-1);
    dirent_checksum_finalize(de);
    superblock_crc_finalize(&sb);
    write_superblock_block(fp, &sb);
    if (fwrite(inode_bm, 1, BS, fp) != BS){ fprintf(stderr, "write inode bitmap failed: %s\n", strerror(errno)); free(inode_table); free(rootblk); fclose(fp); return 1; }
    if (fwrite(data_bm, 1, BS, fp) != BS){ fprintf(stderr, "write data bitmap failed: %s\n", strerror(errno)); free(inode_table); free(rootblk); fclose(fp); return 1; }
    if (fwrite(inode_table, 1, it_bytes, fp) != it_bytes){ fprintf(stderr, "write inode table failed: %s\n", strerror(errno)); free(inode_table); free(rootblk); fclose(fp); return 1; }
    if (fwrite(rootblk, 1, BS, fp) != BS){ fprintf(stderr, "write root dir block failed: %s\n", strerror(errno)); free(inode_table); free(rootblk); fclose(fp); return 1; }
    uint64_t remaining = sb.data_region_blocks - 1;
    write_zero_blocks(fp, remaining);
    free(inode_table);
    free(rootblk);
    fclose(fp);
    fprintf(stderr, "mkfs_builder: wrote %s (%" PRIu64 " blocks, %" PRIu64 " inode blocks, data starts at %" PRIu64 ")\n",
            image_path, sb.total_blocks, sb.inode_table_blocks, sb.data_region_start);
    return 0;
}