#pragma once

#include <stdint.h>
#include <stddef.h>

#define END_OF_CLUS 0x0FFFFFF8

typedef struct __attribute__((packed)) {
    uint16_t byte_per_sec;
    uint8_t sec_per_clus;
    uint16_t rsvd_sec_cnt;
    uint8_t num_fats;
    uint16_t root_ent_cnt;
    uint16_t tot_sec_16; // 0 in fat32
    uint8_t media;
    uint16_t fat_sz_16; // 0 in fat32
    uint16_t sec_per_trk; // useless when use flash
    uint16_t num_heads; // useless when use flash
    uint32_t hidd_sec;
    uint32_t tot_sec32;
    // fat32 only:
    uint32_t fat_sz32;
    uint16_t ext_flags; // [0-3]: user fat num, [7]: 0 mirror to all FAT
    uint16_t fs_ver;
    uint32_t root_clus;
    uint16_t fs_info;
    uint16_t bk_boot_sec;
    uint8_t reserved[12];
} bpb_t;

struct __attribute__((packed)) sector0_struct {
    uint8_t bs_jmpBoot[3];
    uint8_t bs_oem_name[8];
    bpb_t bpb;
    uint8_t drv_num;
    uint8_t reserved1;
    uint8_t bs_boot_sig;
    uint32_t vol_ID;
    uint8_t vol_lab[11];
    char file_sys_type[8]; // "FAT32"
    uint8_t teddy[420];
    uint16_t boot_sec_sig; // 0xAA55
};

typedef struct {
    uint32_t first_fat_sec;
    uint32_t first_data_sec;
    uint32_t byte_per_sec;
    uint32_t sec_per_clus;
    uint32_t root_clus;

    uint32_t fs_info;
    uint32_t fsi_free_cnt;
    uint32_t fsi_nxt_free;
} fat32_t;

#define DIR_ATTR_READ_ONLY 0x01
#define DIR_ATTR_HIDDEN 0x02
#define DIR_ATTR_SYSTEM 0x04
#define DIR_ATTR_VOLUME_ID 0x08
#define DIR_ATTR_DIRECTORY 0x10
#define DIR_ATTR_ARCHIVE 0x20
#define DIR_ATTR_LONG_NAME (DIR_ATTR_READ_ONLY | \
                            DIR_ATTR_HIDDEN | \
                            DIR_ATTR_SYSTEM | \
                            DIR_ATTR_VOLUME_ID)

typedef struct __attribute__((packed)) {
    uint8_t short_name[11]; 
    uint8_t attr;
    uint8_t ntres; // useless
    uint8_t crt_time_tenth;
    uint16_t crt_time;
    uint16_t crt_date;
    uint16_t lst_acc_date;
    uint16_t fst_clus_hi;
    uint16_t wrt_time;
    uint16_t wrt_date;
    uint16_t fst_clus_lo;
    uint32_t file_size;

} fat32_dir_t;

typedef struct __attribute__((packed)) {
    uint8_t ord;
    uint8_t name1[10];
    uint8_t attr;
    uint8_t type;
    uint8_t chk_sum;
    uint8_t name2[12];
    uint16_t meow;
    uint8_t name3[4];
} fat32_long_name_t;

typedef struct {
    uint8_t *name; //long name, this maybe replace to uint8_t array in picos
    uint16_t name_sz;
    uint8_t attr;
    uint32_t file_size;
    uint32_t fst_clus;
    fat32_t *fs;
    struct dir_block *fst_dir_block;
} file_t;

/* assume a page size is 4KB */
#define PAGE_SIZE 4096 
#define LONGEST_NAME_SZ 520
#define FILE_LST_SZ ((PAGE_SIZE - sizeof(struct dir_block *)) / sizeof(file_t))

typedef struct dir_block{
    file_t file_lst[FILE_LST_SZ];
    size_t file_cnt;
    struct dir_block *next;

    /*
        size of dir_block_t should close to page size
    */
} dir_block_t;
