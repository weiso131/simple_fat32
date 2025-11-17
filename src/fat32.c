#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <disk_io.h>
#include <type.h>
#include <fat32_util.h>
#include <dir_block.h>

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

void ls_dir(dir_block_t *dir)
{
    for (dir_block_t *now = dir;now;now = now->next) {
        for (size_t i = 0;i < now->file_cnt;i++) {
            for (uint16_t j = 0;j < now->file_lst[i].name_sz;j++)
                if (is_valid_ascii(now->file_lst[i].name[j]))
                    printf("%c", now->file_lst[i].name[j]);
            printf("\n");
        }
        now = now->next;
    }
}


int main()
{
    mount_disk("disk.bin");

    fat32_t *fs = create_fat32();
    dir_block_t *root = load_dir(fs, fs->root_clus);

    ls_dir(root);


    unmount_disk();
    free(root);

}
