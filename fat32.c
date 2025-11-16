#include <stdio.h>
#include <stdlib.h>

#include <disk_io.h>
#include <type.h>

fat32_t *create_fat32()
{
    fat32_t *fs = NULL;
    uint8_t sec_buf[BLOCK_SIZE];
    struct sector0_struct *sector0 = (struct sector0_struct *) sec_buf;

    read_sector(0, sec_buf);

    if (sector0->boot_sec_sig != 0xAA55 || sector0->bpb.fat_sz32 == 0)
        goto NOT_FAT32;


    fs = malloc(sizeof(fat32_t));
    fs->first_fat_sec = sector0->bpb.rsvd_sec_cnt;
    fs->first_data_sec = fs->first_fat_sec + \
            sector0->bpb.num_fats * sector0->bpb.fat_sz32;
    fs->byte_per_sec = sector0->bpb.byte_per_sec;
    fs->sec_per_clus = sector0->bpb.sec_per_clus;
    fs->root_clus = sector0->bpb.root_clus;
    fs->fs_info = sector0->bpb.fs_info;

    read_sector(fs->fs_info, sec_buf);
    fs->fsi_free_cnt = *((uint32_t *)(sec_buf + 488));
    fs->fsi_nxt_free = *((uint32_t *)(sec_buf + 492));


NOT_FAT32:
    return fs;
}


#define get_clus_first_sec(fs, n) ((n - 2) * (fs)->sec_per_clus + (fs)->first_data_sec)

#define get_clus_fat_sec(fs, n) ((fs)->first_fat_sec + (n * 4) / (fs)->byte_per_sec)

#define get_clus_fat_offset(fs, n) ((n * 4) % (fs)->byte_per_sec)


inline uint32_t get_next_clus(fat32_t *fs, uint32_t n)
{
    uint8_t sector[BLOCK_SIZE];
    read_sector(get_clus_fat_sec(fs, n), sector);

    return *((uint32_t *)(sector + get_clus_fat_offset(fs, n))) & 0x0FFFFFFF;
}

inline void update_next_clus(fat32_t *fs, uint32_t n, uint32_t ent_val)
{
    uint32_t offset = get_clus_fat_offset(fs, n);
    uint32_t sec_num = get_clus_fat_sec(fs, n);
    uint8_t sector[BLOCK_SIZE];
    read_sector(sec_num, sector);
    *((uint32_t *)(sector + offset)) = \
        (*((uint32_t *)(sector + offset)) & 0xF0000000) \
            | (ent_val & 0x0FFFFFFF);
    write_sector(sec_num, sector);
}

void list_dir(fat32_t *fs, uint32_t clus)
{
    fat32_dir_t *root_dir_buf = malloc(BLOCK_SIZE);

    uint32_t first_sec = get_clus_first_sec(fs, clus);
    for (uint32_t i = 0;i < fs->sec_per_clus;i++) {
        read_sector(first_sec + i, root_dir_buf);
        for (uint32_t j = 0;j < BLOCK_SIZE / sizeof(fat32_dir_t);j++) {
            if (root_dir_buf[j].short_name[0] == 0x00)
                goto ls_end;
            if (root_dir_buf[j].short_name[0] == 0xE5)
                continue;
            if (root_dir_buf[j].attr & DIR_ATTR_VOLUME_ID)
                continue;
            printf("%s\n", root_dir_buf[j].short_name);
        }

    }
ls_end:
    free(root_dir_buf);
}

int main()
{
    mount_disk("disk.bin");

    fat32_t *fs = create_fat32();

    list_dir(fs, fs->root_clus);


    unmount_disk();
}
