#include <stdint.h>

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
