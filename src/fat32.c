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

#define print_file_name(j, file) \
    do {\
        for (uint16_t j = 0;j < (file).name_sz && (file).name[j] != 0xFF;j++)\
            if (is_valid_ascii((file).name[j]))\
                printf("%c", (file).name[j]);\
        printf("\n");\
    } while(0)
    

void ls_dir(dir_block_t *dir)
{
    for (dir_block_t *now = dir;now;now = now->next)
        for (size_t i = 0;i < now->file_cnt;i++)
            print_file_name(j, now->file_lst[i]);
}

file_t *find_file(dir_block_t *dir, const char *file_name, size_t name_size)
{
    uint16_t *unicode = malloc(sizeof(uint16_t) * name_size);

    for (size_t i = 0;i < name_size;i++)
        unicode[i] = file_name[i];

    file_t *target = NULL;

    for (dir_block_t *now = dir;now;now = now->next) {
        for (size_t i = 0;i < now->file_cnt;i++) {
            int result = memcmp(now->file_lst[i].name, unicode, sizeof(uint16_t) * name_size);
            if (!result) {
                target = now->file_lst + i;
                goto find_file;
            }
        }
    }
find_file:
    free(unicode);
    return target;
}

#define min(x, y) ((x < y) ? x : y)

void read_file(file_t *file, void *buf, size_t cnt)
{
    if (file->file_size == 0)
        return;

    uint8_t *read_block = malloc(BLOCK_SIZE);
    uint8_t *fat_buf = malloc(BLOCK_SIZE);
    uint32_t clus = file->fst_clus;
    uint32_t fat_sec = 0;

    while (clus < END_OF_CLUS) {
        uint32_t first_sec = get_clus_first_sec(file->fs, clus);

        for (uint32_t i = 0;i < file->fs->sec_per_clus;i++) {
            read_sector(first_sec + i, read_block);
            int copy_size = min(cnt, BLOCK_SIZE);
            memcpy(buf, read_block, copy_size);
            buf += copy_size;
            cnt -= copy_size;

            if (cnt == 0) 
                goto end_read;
                
        }

        if (get_clus_fat_sec(file->fs, clus) != fat_sec) {
            fat_sec = get_clus_fat_sec(file->fs, clus);
            read_sector(fat_sec, fat_buf);
        }

        clus = get_next_clus(file->fs, clus, fat_buf); 

    }
end_read:
    free(read_block);
    free(fat_buf);
    return;
}


int main()
{
    mount_disk("disk.bin");

    fat32_t *fs = create_fat32();
    dir_block_t *root = load_dir(fs, fs->root_clus);

    ls_dir(root);

    char target_name[] = "meow.txt";

    file_t *target = find_file(root, target_name, sizeof(target_name));

    if (target)
        print_file_name(j, *target);
        
    uint8_t *target_buf = malloc(BLOCK_SIZE);

    read_file(target, target_buf, BLOCK_SIZE);

    printf("%s", target_buf);

    unmount_disk();
    free(root);

}
